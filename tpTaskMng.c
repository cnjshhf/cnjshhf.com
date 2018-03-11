/***************************************************************
 * ���س���
 **************************************************************/
#include "tpbus.h"

int  tpTaskRun(TPTASK *ptpTask);
void tpTaskRest(int sig);
void tpTaskResp(MQHDL *mqh, int q, char *clazz, char *result, int flag);

/***************************************************************
 * ������
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
        printf("��ͨ��tpbus��������\n");    
        return -1;
    }

    /*��ʼ������*/
    if (tpTaskInit(0, "tpTaskMng", NULL) != 0) {
        LOG_ERR("��ʼ�����������̳���");
        return -1;
    }

    /*��ʼ������*/
    if ((ret = mq_init()) != 0) {
        LOG_ERR("mq_init() error: retcode=%d", ret);
        return -1;
    }

    /*������������*/
    if ((ret = mq_bind(Q_TSK, &mqh)) != 0) {
        LOG_ERR("mq_bind(%d) error: retcode=%d", Q_TSK, ret);
        return -1;
    }

    /*�����쳣����¿�����ǰ�����Ľ���*/
    /*for (i=0; i<gShm.pShmList->tpTask.nItems; i++) {
        if (gShm.ptpTask[i].pid > 0) {
            kill(gShm.ptpTask[i].pid, SIGKILL);
            gShm.ptpTask[i].pid = 0;
            time(&gShm.ptpTask[i].stopTime);
        }
    }*/

    /*�����ӽ����˳��ź�*/
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
        LOG_MSG("�յ�����[%d]�����ı���", q);
        if (hasLogLevel(DEBUG)) tpHeadDebug((TPHEAD *)&tpMsg.head);
        if (hasLogLevel(DEBUG)) LOG_HEX(tpMsg.body, tpMsg.head.bodyLen);

        if (tpShmRetouch() != 0) {
            LOG_ERR("ˢ�¹����ڴ�ָ�����");
            continue;
        }

        switch (tpMsg.head.msgType) {
            case MSGSYS_START: /*��������*/
            case MSGSYS_STOP:  /*ֹͣ����*/
                tpMsg.body[tpMsg.head.bodyLen] = 0;
                taskId = atoi(tpMsg.body);
                if (taskId <= 0) { /*δָ��IDʱ������������*/
                    ptpTask = gShm.ptpTask;
                    total = gShm.pShmList->tpTask.nItems;
                } else { /*����ָ��ID����*/
                    if (!(ptpTask = tpTaskShmGet(taskId, NULL))) {
                        snprintf(result, sizeof(result), "������[%d]����", taskId);
                        tpTaskResp(mqh, q, clazz, result, 1);
                        continue;
                    }
                    total = 1;
                }
                break;
            default:
                snprintf(result, sizeof(result), "����Ĳ���ȷ");
                tpTaskResp(mqh, q, clazz, result, 1);
                continue;
        }

        /*�����ӽ����˳��ź�*/
        /*��ִ����ͣ��������н���ʱ���Ჶ׽�ӽ����˳��ź�*/
        signal(SIGCLD, SIG_IGN);

        time(&t);
        switch (tpMsg.head.msgType) {
            case MSGSYS_START: /*��������*/
                for (i=0,n=0; i<total; i++) {
                    if (ptpTask[i].pid > 0 && kill(ptpTask[i].pid, 0) == 0) {
                        snprintf(result, sizeof(result), "����[%d]��������״̬", ptpTask[i].taskId); 
                        tpTaskResp(mqh, q, clazz, result, 0); 
                        continue;
                    }                        
                    if ((ptpTask[i].pid = tpTaskRun(&ptpTask[i])) <= 0) {
                        ptpTask[i].pid = 0;
                        ptpTask[i].startTime = 0;
                        snprintf(result, sizeof(result), "��������[%d]ʧ��", ptpTask[i].taskId);
                        tpTaskResp(mqh, q, clazz, result, 0);                            
                    }
                    time(&ptpTask[i].startTime);
                    n++;                    
                }
                /*����������״̬*/
                if (n>0) {
                    sleep(1);
                    for (i=0; i<total; i++) {
                        if (ptpTask[i].pid > 0 && ptpTask[i].startTime >= t) {
                            if (kill(ptpTask[i].pid, 0) == -1) {
                                ptpTask[i].pid = 0;
                                ptpTask[i].startTime = 0;
                                snprintf(result, sizeof(result), "��������[%d]ʧ��", ptpTask[i].taskId);
                            } else {
                                snprintf(result, sizeof(result), "��������[%d]�ɹ�", ptpTask[i].taskId);
                            }
                            tpTaskResp(mqh, q, clazz, result, 0); 
                        }
                    }
                }
                tpTaskResp(mqh, q, clazz, "", 1);
                break;
            case MSGSYS_STOP: /*ֹͣ����*/
                for (i=0,n=0; i<total; i++) {
                    if (ptpTask[i].pid <= 0 || kill(ptpTask[i].pid, 0) == -1) {
                        ptpTask[i].pid = 0;
                        snprintf(result, sizeof(result), "����[%d]����ֹͣ״̬", ptpTask[i].taskId); 
                        tpTaskResp(mqh, q, clazz, result, 0); 
                        continue;
                    }
                    kill(ptpTask[i].pid, SIGKILL);
                    time(&ptpTask[i].stopTime);
                    n++;
                }
                /*����������״̬*/
                if (n>0) {
                    sleep(1);
                    for (i=0; i<total; i++) {
                        if (ptpTask[i].pid > 0 && ptpTask[i].stopTime >= t) {
                            snprintf(result, sizeof(result), "ֹͣ����[%d]�ɹ�", ptpTask[i].taskId);
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

        /*�ָ��ӽ����˳��ź�����*/
        signal(SIGCLD, tpTaskRest);
    }    
}

/***************************************************************
 * �����������
 **************************************************************/
int tpTaskRun(TPTASK *ptpTask)
{
    int    i,j,p;
    pid_t  pid;
    char   pathFile[256];
    char   *argv[20],args[21][256];

    /*�����������*/
    strcpy(pathFile, ptpTask->pathFile);
    for (i=0, j=0, p=0; pathFile[p]; p++) {
        if (pathFile[p] == ' ') {
            if (j == 0) continue;
            if (i >= 20) {
                LOG_ERR("���������಻�ܳ���20��");
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
        LOG_ERR("δ��������ִ���ļ�");
        return -1;
    }
   
    if (args[0][0] != '/') { /*֧�����·��*/
        snprintf(pathFile, sizeof(pathFile), "%s/bin/%s", getenv("TPBUS_HOME"), args[0]);
        strcpy(args[0], pathFile);
    } 

    if ((access(args[0], F_OK) == -1) || (access(args[0], X_OK) == -1)) { 
        LOG_ERR("����ִ���ļ�[%s]�����ڻ��޿�ִ��Ȩ��", args[0]);
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
 * �Զ����� �������(SIGCLD�źŴ������)
 **************************************************************/
void tpTaskRest(int sig)
{
    int i,status;
    pid_t pid;
    TPTASK *ptpTask = NULL;

    while ((pid = waitpid(0, &status, WNOHANG)) > 0) {
        LOG_MSG("�������[pid=%ld]�˳��ź�", pid);

        if (tpShmRetouch() != 0) {
            LOG_ERR("ˢ�¹����ڴ�ָ�����");
            continue;
        }

        for (i=0; i<gShm.pShmList->tpTask.nItems; i++) {
            if (pid == gShm.ptpTask[i].pid) {
                ptpTask = &gShm.ptpTask[i];
                break;
            }
        }
        if (!ptpTask) {
            LOG_ERR("���������[pid=%ld]��¼", pid);
            continue;
        }
        if (ptpTask->maxRestNum <= 0) {
            ptpTask->pid = 0;
            time(&ptpTask->stopTime);
            continue;
        }
        if (ptpTask->restedNum >= ptpTask->maxRestNum) {
            LOG_MSG("�������[ID=%d]�Ѵﵽ�����������", ptpTask->taskId);
            continue;
        }
        LOG_MSG("�Զ������������[ID=%d]", ptpTask->taskId);
        ptpTask->restedNum++;
        time(&ptpTask->startTime);
        if ((ptpTask->pid = tpTaskRun(ptpTask)) <= 0) {
            LOG_ERR("�����������[ID=%d]ʧ��", ptpTask->taskId);
            ptpTask->pid = 0;
            time(&ptpTask->stopTime);            
        }        
    }
}

/***************************************************************
 * ��������ִ�н����Ϣ
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
        if (flag) { /*��������ִ�н���*/
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

