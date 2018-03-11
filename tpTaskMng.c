/***************************************************************
 * 主控程序
 **************************************************************/
#include "tpbus.h"

int  tpTaskRun(TPTASK *ptpTask);
void tpTaskRest(int sig);
void tpTaskResp(MQHDL *mqh, int q, char *clazz, char *result, int flag);

/***************************************************************
 * 主函数
 **************************************************************/
int main(int argc, char *argv[])
{
    int    i,n,q,ret;
    int    len,total;
    int    taskId;
    char   result[256];
    char   clazz[MSGCLZLEN+1];
    time_t t;
    MQHDL *mqh;
    TPMSG  tpMsg;
    TPTASK *ptpTask;

    if (getppid() != tpGetPid("tpbus")) {
        printf("请通过tpbus命令启动\n");    
        return -1;
    }

    /*初始化进程*/
    if (tpTaskInit(0, "tpTaskMng", NULL) != 0) {
        LOG_ERR("初始化任务管理进程出错");
        return -1;
    }

    /*初始化队列*/
    if ((ret = mq_init()) != 0) {
        LOG_ERR("mq_init() error: retcode=%d", ret);
        return -1;
    }

    /*绑定任务管理队列*/
    if ((ret = mq_bind(Q_TSK, &mqh)) != 0) {
        LOG_ERR("mq_bind(%d) error: retcode=%d", Q_TSK, ret);
        return -1;
    }

    /*清理异常情况下可能先前遗留的进程*/
    /*for (i=0; i<gShm.pShmList->tpTask.nItems; i++) {
        if (gShm.ptpTask[i].pid > 0) {
            kill(gShm.ptpTask[i].pid, SIGKILL);
            gShm.ptpTask[i].pid = 0;
            time(&gShm.ptpTask[i].stopTime);
        }
    }*/

    /*设置子进程退出信号*/
    signal(SIGCLD, tpTaskRest);

    while (1) {
        clazz[0] = 0;
        len = sizeof(TPMSG);
        if ((ret = mq_pend(mqh, NULL, &q, NULL, clazz, (char *)&tpMsg, &len, 5)) != 0) {
            if (ret != ERR_TIMEOUT) {
                LOG_ERR("mq_pend(%d) error: retcode=%d", Q_TSK, ret);
                return -1;
            }
            signal(SIGCLD, SIG_IGN);
            for (i=0; i<gShm.pShmList->tpTask.nItems; i++) {
                if (gShm.ptpTask[i].pid > 0 && kill(gShm.ptpTask[i].pid, 0) == -1) {
                    if (gShm.ptpTask[i].restedNum >= gShm.ptpTask[i].maxRestNum) {
                        gShm.ptpTask[i].pid = 0;
                        gShm.ptpTask[i].startTime = 0;
                        if (!gShm.ptpTask[i].stopTime) time(&gShm.ptpTask[i].stopTime);
                    } else {
                        if ((gShm.ptpTask[i].pid = tpTaskRun(&gShm.ptpTask[i])) > 0) {
                            time(&gShm.ptpTask[i].startTime);
                        } else {
                            gShm.ptpTask[i].pid = 0;
                            gShm.ptpTask[i].startTime = 0;
                            if (!gShm.ptpTask[i].stopTime) time(&gShm.ptpTask[i].stopTime);
                        }
                        gShm.ptpTask[i].restedNum++;
                    }
                }
            }
            signal(SIGCLD, tpTaskRest);
            continue;
        }
        LOG_MSG("收到队列[%d]发来的报文", q);
        if (hasLogLevel(DEBUG)) tpHeadDebug((TPHEAD *)&tpMsg.head);
        if (hasLogLevel(DEBUG)) LOG_HEX(tpMsg.body, tpMsg.head.bodyLen);

        if (tpShmRetouch() != 0) {
            LOG_ERR("刷新共享内存指针出错");
            continue;
        }

        switch (tpMsg.head.msgType) {
            case MSGSYS_START: /*启动命令*/
            case MSGSYS_STOP:  /*停止命令*/
                tpMsg.body[tpMsg.head.bodyLen] = 0;
                taskId = atoi(tpMsg.body);
                if (taskId <= 0) { /*未指定ID时启动所有任务*/
                    ptpTask = gShm.ptpTask;
                    total = gShm.pShmList->tpTask.nItems;
                } else { /*启动指定ID任务*/
                    if (!(ptpTask = tpTaskShmGet(taskId, NULL))) {
                        snprintf(result, sizeof(result), "无任务[%d]配置", taskId);
                        tpTaskResp(mqh, q, clazz, result, 1);
                        continue;
                    }
                    total = 1;
                }
                break;
            default:
                snprintf(result, sizeof(result), "命令报文不正确");
                tpTaskResp(mqh, q, clazz, result, 1);
                continue;
        }

        /*屏蔽子进程退出信号*/
        /*在执行启停命令过程中将暂时不会捕捉子进程退出信号*/
        signal(SIGCLD, SIG_IGN);

        time(&t);
        switch (tpMsg.head.msgType) {
            case MSGSYS_START: /*启动命令*/
                for (i=0,n=0; i<total; i++) {
                    if (ptpTask[i].pid > 0 && kill(ptpTask[i].pid, 0) == 0) {
                        snprintf(result, sizeof(result), "任务[%d]已是运行状态", ptpTask[i].taskId); 
                        tpTaskResp(mqh, q, clazz, result, 0); 
                        continue;
                    }                        
                    if ((ptpTask[i].pid = tpTaskRun(&ptpTask[i])) <= 0) {
                        ptpTask[i].pid = 0;
                        ptpTask[i].startTime = 0;
                        snprintf(result, sizeof(result), "启动任务[%d]失败", ptpTask[i].taskId);
                        tpTaskResp(mqh, q, clazz, result, 0);                            
                    }
                    time(&ptpTask[i].startTime);
                    n++;                    
                }
                /*检查任务进程状态*/
                if (n>0) {
                    sleep(1);
                    for (i=0; i<total; i++) {
                        if (ptpTask[i].pid > 0 && ptpTask[i].startTime >= t) {
                            if (kill(ptpTask[i].pid, 0) == -1) {
                                ptpTask[i].pid = 0;
                                ptpTask[i].startTime = 0;
                                snprintf(result, sizeof(result), "启动任务[%d]失败", ptpTask[i].taskId);
                            } else {
                                snprintf(result, sizeof(result), "启动任务[%d]成功", ptpTask[i].taskId);
                            }
                            tpTaskResp(mqh, q, clazz, result, 0); 
                        }
                    }
                }
                tpTaskResp(mqh, q, clazz, "", 1);
                break;
            case MSGSYS_STOP: /*停止命令*/
                for (i=0,n=0; i<total; i++) {
                    if (ptpTask[i].pid <= 0 || kill(ptpTask[i].pid, 0) == -1) {
                        ptpTask[i].pid = 0;
                        snprintf(result, sizeof(result), "任务[%d]已是停止状态", ptpTask[i].taskId); 
                        tpTaskResp(mqh, q, clazz, result, 0); 
                        continue;
                    }
                    kill(ptpTask[i].pid, SIGKILL);
                    time(&ptpTask[i].stopTime);
                    n++;
                }
                /*检查任务进程状态*/
                if (n>0) {
                    sleep(1);
                    for (i=0; i<total; i++) {
                        if (ptpTask[i].pid > 0 && ptpTask[i].stopTime >= t) {
                            snprintf(result, sizeof(result), "停止任务[%d]成功", ptpTask[i].taskId);
                            ptpTask[i].pid = 0;
                            tpTaskResp(mqh, q, clazz, result, 0);
                        }
                    }
                }
                tpTaskResp(mqh, q, clazz, "", 1);
                if (total >= gShm.pShmList->tpTask.nItems) return 0;
                break;
            default:
                break;
        } 

        /*恢复子进程退出信号设置*/
        signal(SIGCLD, tpTaskRest);
    }    
}

/***************************************************************
 * 启动任务进程
 **************************************************************/
int tpTaskRun(TPTASK *ptpTask)
{
    int    i,j,p;
    pid_t  pid;
    char   pathFile[256];
    char   *argv[20],args[21][256];

    /*解析程序参数*/
    strcpy(pathFile, ptpTask->pathFile);
    for (i=0, j=0, p=0; pathFile[p]; p++) {
        if (pathFile[p] == ' ') {
            if (j == 0) continue;
            if (i >= 20) {
                LOG_ERR("程序参数最多不能超过20个");
                return -1;
            }
            args[i][j] = 0;
            argv[i] = (char *)(&args[i][0]);
            i++;
            j = 0;
        } else {
            args[i][j++] = pathFile[p];
        }
    }
    if (j > 0) {
        args[i][j] = 0;
        argv[i] = (char *)(&args[i][0]);
        i++;
    }
    argv[i] = NULL;

    if (argv[0] == NULL) {
        LOG_ERR("未配置任务执行文件");
        return -1;
    }
   
    if (args[0][0] != '/') { /*支持相对路径*/
        snprintf(pathFile, sizeof(pathFile), "%s/bin/%s", getenv("TPBUS_HOME"), args[0]);
        strcpy(args[0], pathFile);
    } 

    if ((access(args[0], F_OK) == -1) || (access(args[0], X_OK) == -1)) { 
        LOG_ERR("任务执行文件[%s]不存在或无可执行权限", args[0]);
        return -1;
    }

    if ((pid = fork()) == 0) {
        execvp(*argv, argv);
        exit(0);
    } else if (pid > 0) {
        return pid;
    } else {
        LOG_ERR("fork() error: %s", strerror(errno));
        return -1;
    }
}

/***************************************************************
 * 自动重启 任务进程(SIGCLD信号处理程序)
 **************************************************************/
void tpTaskRest(int sig)
{
    int i,status;
    pid_t pid;
    TPTASK *ptpTask = NULL;

    while ((pid = waitpid(0, &status, WNOHANG)) > 0) {
        LOG_MSG("任务进程[pid=%ld]退出信号", pid);

        if (tpShmRetouch() != 0) {
            LOG_ERR("刷新共享内存指针出错");
            continue;
        }

        for (i=0; i<gShm.pShmList->tpTask.nItems; i++) {
            if (pid == gShm.ptpTask[i].pid) {
                ptpTask = &gShm.ptpTask[i];
                break;
            }
        }
        if (!ptpTask) {
            LOG_ERR("无任务进程[pid=%ld]记录", pid);
            continue;
        }
        if (ptpTask->maxRestNum <= 0) {
            ptpTask->pid = 0;
            time(&ptpTask->stopTime);
            continue;
        }
        if (ptpTask->restedNum >= ptpTask->maxRestNum) {
            LOG_MSG("任务进程[ID=%d]已达到最大重启次数", ptpTask->taskId);
            continue;
        }
        LOG_MSG("自动重启任务进程[ID=%d]", ptpTask->taskId);
        ptpTask->restedNum++;
        time(&ptpTask->startTime);
        if ((ptpTask->pid = tpTaskRun(ptpTask)) <= 0) {
            LOG_ERR("重启任务进程[ID=%d]失败", ptpTask->taskId);
            ptpTask->pid = 0;
            time(&ptpTask->stopTime);            
        }        
    }
}

/***************************************************************
 * 返回命令执行结果信息
 **************************************************************/
void tpTaskResp(MQHDL *mqh, int q, char *clazz, char *result, int flag)
{
    int ret;
    TPMSG tpMsg;

    memset(&tpMsg.head, 0, sizeof(TPHEAD));
    tpMsg.head.msgType = MSGSYS_RESULT;
    tpMsg.head.bodyLen = strlen(result);

    if (!flag && tpMsg.head.bodyLen <= 0) return;
    if (tpMsg.head.bodyLen > 0) {
        LOG_MSG("%s", result);
        memcpy(tpMsg.body, result, tpMsg.head.bodyLen);
        if ((ret = mq_post(mqh, NULL, q, 0, clazz, (char *)&tpMsg, sizeof(TPHEAD)+tpMsg.head.bodyLen, 0)) != 0) {
            LOG_ERR("mq_post(%d) error: retcode=%d", q, ret);
        }
        if (flag) { /*本次命令执行结束*/
            tpMsg.head.bodyLen = 0;
            if ((ret = mq_post(mqh, NULL, q, 0, clazz, (char *)&tpMsg, sizeof(TPHEAD), 0)) != 0) {
                LOG_ERR("mq_post(%d) error: retcode=%d", q, ret);
            }    
        }        
    } else {
        if ((ret = mq_post(mqh, NULL, q, 0, clazz, (char *)&tpMsg, sizeof(TPHEAD), 0)) != 0) {
            LOG_ERR("mq_post(%d) error: retcode=%d", q, ret);
        }    
    }
}

