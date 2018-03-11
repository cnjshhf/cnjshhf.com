/***************************************************************
 * 系统监控程序
 **************************************************************/
#include "tpbus.h"
#include <sys/utsname.h>

struct vm_stat_t {
    int us;
    int sy;
    int id;
    int wa;
    int pi;
    int po;
};

struct ps_stat_t {
    pid_t pid;
    float cpu;
    float mem;
};

struct mq_stat_t {
    int  qid;
    int  conn_num;
    long send_num;
    char send_time[9];
    char send_pid[11];
    long recv_num;
    char recv_time[9];
    char recv_pid[11];
    long pend_num;
    long dead_num;
};

static int m_sock;
static char m_uname[65];
static char m_webHost[65];
static pthread_mutex_t m_mtx;

static int getSysName(char *result);
static int get_vm_stat(struct vm_stat_t *vmStat);
static int get_ps_stat(pid_t pid, struct ps_stat_t *psStat);
static char *getTaskFile(TPTASK *ptpTask, char *result);
static char *getTaskTime(TPTASK *ptpTask, char *result);
static char *getTaskStat(TPTASK *ptpTask, char *result);
static char *getStatus(int status, char *result);

static void *tpSysMonThread(void *arg);
static void *tpSysMonThread2(void *arg);

/***************************************************************
 * 主函数
 **************************************************************/
int main(int argc, char *argv[])
{
    int  i,ret;
    MQHDL *mqh;

    /*检查命令行参数*/
    if (argc != 2) {
        printf("Usage: %s <任务ID>\n", argv[0]);
        return -1;
    }

    /*初始化进程*/
    /*if (tpTaskInit(TSK_SYSMON, "tpSysMon", NULL) != 0) { */
    if (tpTaskInit(atoi(argv[1]), "tpSysMon", NULL) != 0) {
        LOG_ERR("初始化进程出错");
        return -1;
    }
    setLogCallBack(NULL);

    /*检查环境变量*/
    if (!getenv("TPBUS_WEBHOST")) {
        LOG_ERR("未设置环境变量[TPBUS_WEBHOST]");
        return -1;
    }
    strcpy(m_webHost, getenv("TPBUS_WEBHOST"));

    /*检查操作系统*/
    if (getSysName(m_uname) != 0) {
        LOG_ERR("无法获得系统名称");
        return -1;
    }

    /*初始化UDP侦听端口*/
    if ((m_sock = udp_init(NULL, gShm.ptpConf->sysMonPort)) == -1) {
        LOG_ERR("初始化UDP侦听端口出错: %s", strerror(errno));
        return -1;
    }

    /*初始化队列*/
    if ((ret = mq_init()) != 0) {
        LOG_ERR("mq_init() error: retcode=%d", ret);
        LOG_ERR("初始化队列出错");
        return -1;
    }

    /*绑定队列*/
    if ((ret = mq_bind(Q_MON, &mqh)) != 0) {
        LOG_ERR("mq_bind() error: retcode=%d", ret);
        LOG_ERR("绑定队列[%d]出错", Q_MON);
        return -1;
    }

    /*创建系统状态监控线程*/
    if (tpThreadCreate(tpSysMonThread, NULL) != 0) {
        LOG_ERR("创建系统状态监控线程失败: %s", strerror(errno));
        return -1;
    }

    pthread_mutex_init(&m_mtx, NULL);

    /*创建监控报文处理线程池*/
    for (i=0; i<gShm.ptpConf->nMonProcs; i++) {
        if (tpThreadCreate(tpSysMonThread2, NULL) != 0) {
            LOG_ERR("创建监控报文处理线程失败: %s", strerror(errno));
            return -1;
        }
    }

    while (1) {
        sleep(1);

        /*向集群分发机报告状态(TODO)*/
    }

    return 0;
}

/***************************************************************
 * 获取系统名称(uname命令)
 **************************************************************/
static int getSysName(char *result)
{
    struct utsname buf;

    if (uname(&buf) != 0) return -1;
    strcpy(result, buf.sysname);
    return 0;
}

/***************************************************************
 * 获取系统状态信息(vmstat命令)
 **************************************************************/
static int get_vm_stat(struct vm_stat_t *vmStat)
{
    FILE *pp;
    int  a[17];
    char buf[256];
 
    if (!(pp = popen("vmstat", "r"))) return -1;
    while (fgets(buf, sizeof(buf), pp) != NULL) {
        if (!isdigit(*strTrim(buf))) continue;
        sscanf(buf, "%d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d",
              &a[0], &a[1], &a[2], &a[3], &a[4], &a[5], &a[6], &a[7], &a[8],
              &a[9], &a[10], &a[11], &a[12], &a[13], &a[14], &a[15], &a[16]);
        if (strcmp("AIX", m_uname) == 0) {
            vmStat->pi = a[5];
            vmStat->po = a[6];
            vmStat->us = a[13];
            vmStat->sy = a[14];
            vmStat->id = a[15];
            vmStat->wa = a[16];
            pclose(pp);
            return 0;
        } else if (strcmp("Linux", m_uname) == 0) {
            vmStat->pi = a[6];
            vmStat->po = a[7];
            vmStat->us = a[12];
            vmStat->sy = a[13];
            vmStat->id = a[14];
            vmStat->wa = a[15];
            pclose(pp);
            return 0;
        } else {
            break;
        }
    }
    pclose(pp);
    return -1;
}

/***************************************************************
 * 获取进程状态信息(ps命令)
 **************************************************************/
static int get_ps_stat(pid_t pid, struct ps_stat_t *psStat)
{
    FILE *pp;
    char buf[256];

    sprintf(buf, "ps -u `whoami` -u pid,pcpu,pmem |grep %d", pid);
    if (!(pp = popen(buf, "r"))) return -1;
    while (fgets(buf, sizeof(buf), pp) != NULL) {
        sscanf(buf, "%d %f %f", &psStat->pid, &psStat->cpu, &psStat->mem);
        if (pid == psStat->pid) {
            pclose(pp);
            return 0;
        }
    }
    pclose(pp);
    return -1;
}

/***************************************************************
 * getTaskFile
 **************************************************************/
static char *getTaskFile(TPTASK *ptpTask, char *result)
{
    int i;

    for (i=strlen(ptpTask->pathFile); i>0 && ptpTask->pathFile[i-1] != '/'; i--) {}
    strcpy(result, ptpTask->pathFile+i);
    return result;
}

/***************************************************************
 * getTaskTime
 **************************************************************/
static char *getTaskTime(TPTASK *ptpTask, char *result)
{
    if (!ptpTask->startTime && !ptpTask->stopTime) {
        strcpy(result, "--/--/--|--:--:--");
    } else {
        if (ptpTask->pid > 0) {
            getTimeString(ptpTask->startTime, "yy/MM/dd|hh:mm:ss", result);
        } else {
            getTimeString(ptpTask->stopTime, "yy/MM/dd|hh:mm:ss", result);
        }
    }
    return result;
}

/***************************************************************
 * getTaskStat
 **************************************************************/
static char *getTaskStat(TPTASK *ptpTask, char *result)
{
    if (!ptpTask->startTime && !ptpTask->stopTime) {
        strcpy(result, "未启动");
    } else {
        if (ptpTask->pid > 0) {
            strcpy(result, "运行中");
        } else {
            strcpy(result, "已停止");
        }
    }
    return result;
}

/***************************************************************
 * 系统状态监控线程
 **************************************************************/
void *tpSysMonThread(void *arg)
{
    FILE *pp;
    int  i,h,sockfd;
    char buf[256];
    char taskFile[41];
    char taskTime[21];
    char taskStat[11];
    struct vm_stat_t vmStat;
    struct ps_stat_t psStat;
    struct mq_stat_t mqStat;

    /*初始化UDP套接字(用于转发状态信息)*/
    if ((sockfd = udp_init(NULL, 0)) == -1) {
        LOG_ERR("初始化UDP套接字出错: %s", strerror(errno));
        goto THD_END;
    }

    while (1) {
        sleep(gShm.ptpConf->ntsSysMon);

        if (tpShmRetouch() != 0) {
            LOG_ERR("刷新共享内存指针出错");
            continue;
        }

        /*SYS*/
        /*主机ID|SYS|pi|po|us|sy|id|wa*/
        if (get_vm_stat(&vmStat) == 0) {
            sprintf(buf, "%d&SYS&%d&%d&%d&%d&%d&%d",
                    gShm.pShmHead->hostId,
                    vmStat.pi, vmStat.po,
                    vmStat.us, vmStat.sy, vmStat.id, vmStat.wa);

            LOG_DBG("%s", buf);
            udp_send(sockfd, buf, strlen(buf), m_webHost, gShm.ptpConf->webMonPort);
        }

        /*TSK*/
        /*主机ID|TSK|任务ID|任务名称|绑定队列|执行程序|进程号|%CPU|%MEM|启停日期|启停时间|状态*/
        for (i=0; i<gShm.pShmList->tpTask.nItems; i++) {
            if (get_ps_stat(gShm.ptpTask[i].pid, &psStat) != 0) continue;
            sprintf(buf, "%d&TSK&%d&%s&%d&%s&%d&%2.1f&%2.1f&%s&%s",
                    gShm.pShmHead->hostId,
                    gShm.ptpTask[i].taskId,
                    gShm.ptpTask[i].taskName,
                    gShm.ptpTask[i].bindQ,
                    getTaskFile(&gShm.ptpTask[i], taskFile),
                    gShm.ptpTask[i].pid,
                    psStat.cpu,
                    psStat.mem,
                    getTaskTime(&gShm.ptpTask[i], taskTime),
                    getTaskStat(&gShm.ptpTask[i], taskStat));

            LOG_DBG("%s", buf);
            udp_send(sockfd, buf, strlen(buf), m_webHost, gShm.ptpConf->webMonPort);
        }

        /*SRV*/
        /*主机ID|SRV|服务代码|服务名称|最大请求数|当前请求数|当前冲正数*/
        for (h=1; h<=HASH_SIZE; h++) {
            i = h;
            while (i>0 && gShm.ptpServ[i-1].servCode[0]) {
                sprintf(buf, "%d&SRV&%s&%s&%d&%ld&%ld",
                        gShm.pShmHead->hostId,
                        gShm.ptpServ[i-1].servCode,
                        gShm.ptpServ[i-1].servName,
                        gShm.ptpServ[i-1].nMaxReq,
                        gShm.ptpServ[i-1].nActive,
                        gShm.ptpServ[i-1].nReving);

                LOG_DBG("%s", buf);
                udp_send(sockfd, buf, strlen(buf), m_webHost, gShm.ptpConf->webMonPort);
                i = gShm.ptpServ[i-1].next;
            }
        }

        /*MQM*/
        /*主机ID|MQM|队列ID|连接数|写入条数|写入时间|写入进程|读出条数|读出时间|读出进程|滞留条数|蒸发条数*/
        if (!(pp = popen("mqm status -q", "r"))) continue;
        fgets(buf, sizeof(buf), pp); /*Omit title-line*/
        while (fgets(buf, sizeof(buf), pp) != NULL) {
            if (memcmp(buf, "--", 2) == 0) continue; /*Omit separate-line*/
            sscanf(buf, "%d %d %ld %s %s %ld %s %s %ld %ld",
                   &mqStat.qid, &mqStat.conn_num,
                   &mqStat.send_num, mqStat.send_time, mqStat.send_pid,
                   &mqStat.recv_num, mqStat.recv_time, mqStat.recv_pid,
                   &mqStat.pend_num, &mqStat.dead_num);
            sprintf(buf, "%d&MQM&%d&%d&%ld&%s&%s&%ld&%s&%s&%ld&%ld",
                   gShm.pShmHead->hostId,
                   mqStat.qid, mqStat.conn_num,
                   mqStat.send_num, mqStat.send_time, mqStat.send_pid,
                   mqStat.recv_num, mqStat.recv_time, mqStat.recv_pid,
                   mqStat.pend_num, mqStat.dead_num);
                   
            LOG_DBG("%s", buf);
            udp_send(sockfd, buf, strlen(buf), m_webHost, gShm.ptpConf->webMonPort);
        }
        pclose(pp);
    }
 
THD_END:
    tpThreadExit();
    return NULL;
}

/***************************************************************
 * 获取交易处理结果信息
 **************************************************************/
static char *getStatus(int status, char *result)
{
    result[0] = 0;
    switch (status) {
        case TP_OK:
            strcpy(result, "正常结束");
            break;
        case TP_TIMEOUT:
            strcpy(result, "处理超时");
            break;
        case TP_REVOK:
            strcpy(result, "冲正结束");
            break;
        case TP_REVFAIL:
        case TP_SAFFAIL:
            strcpy(result, "冲正失败");
            break;
        case TP_BUSY:
        case TP_INVAL:
        case TP_ERROR:
        case TP_RUNING:
        case TP_REVING:
            break;
        default:
            break;
    }
    return result;
}

/***************************************************************
 * 监控报文处理线程
 **************************************************************/
void *tpSysMonThread2(void *arg)
{
    int  i,j;
    int  sockfd,msgLen,tmpLen;
    char buf[MAXMSGLEN],tmpBuf[MAXVALLEN+1];
    char szTime[9],szStat[21];
    TPCTX  tpCtx;
    MSGCTX msgCtx;
    TPMSG  *ptpMsg  = &tpCtx.tpMsg;
    TPHEAD *ptpHead = &ptpMsg->head;
    TPMONI *ptpMoni;

    /*初始化UDP套接字(用于转发监控报文)*/
    if ((sockfd = udp_init(NULL, 0)) == -1) {
        LOG_ERR("初始化UDP套接字出错: %s", strerror(errno));
        goto THD_END;
    }

    tpCtxSetTSD(&tpCtx);
    tpCtx.msgCtx = &msgCtx;
    tpPreBufInit(&tpCtx.preBuf, NULL);

    /*循环接收、处理、转发监控报文*/
    while (1) {
        pthread_mutex_lock(&m_mtx);
        msgLen = udp_recv(m_sock, (char *)ptpMsg, sizeof(TPMSG), NULL, NULL);
        pthread_mutex_unlock(&m_mtx);

        if (msgLen <= 0) {
            LOG_ERR("接收监控报文出错: %s", strerror(errno));
            goto THD_END;
        }
        if (msgLen != (ptpHead->bodyLen + sizeof(TPHEAD))) {
            LOG_ERR("接收监控报文不完整");
            continue;
        }

        if (tpShmRetouch() != 0) {
            LOG_ERR("刷新共享内存指针出错");
            continue;
        }

        switch (ptpHead->msgType) {
            case MSGMON_EVT:
                /*主机ID|EVT|事件信息*/
                ptpMsg->body[ptpHead->bodyLen] = 0;
                sprintf(buf, "%d&EVT&%s", gShm.pShmHead->hostId, ptpMsg->body);
                LOG_DBG("%s", buf);
                udp_send(sockfd, buf, strlen(buf), m_webHost, gShm.ptpConf->webMonPort);
                break;
            case MSGAPP:
            case MSGREV:
            case MSGSYS_TIMEOUT:
                /*主机ID|MON|流水号|请求时间|服务代码|请求端点|请求代码|目标端点|耗时|状态|...*/
                sprintf(buf, "%d&MON&%d&%s&%s&%d&%s&%d&%d&%s",
                        gShm.pShmHead->hostId,
                        ptpHead->tpId,
                        getTimeString(ptpHead->beginTime, "hh:mm:ss", szTime),
                        ptpHead->servCode,
                        ptpHead->orgQ,
                        ptpHead->tranCode,
                        ptpHead->desQ,
                        ptpHead->milliTime,
                        getStatus(ptpHead->status, szStat));

                if (ptpHead->monFlag) { /*自定义监控*/
                    while (1) {
                        if (tpMsgCtxUnpack(tpCtx.msgCtx, ptpMsg->body, ptpHead->bodyLen) != 0) {
                            LOG_ERR("tpMsgCtxUnpack() error");
                            break;
                        }
                        if ((i = tpServShmGet(ptpHead->servCode, NULL)) <= 0) {
                            LOG_ERR("tpServShmGet(%s) error", ptpHead->servCode);
                            break;
                        }

                        for (j=0; j<gShm.ptpServ[i-1].monItems; j++) {
                            ptpMoni = &gShm.ptpMoni[j+gShm.ptpServ[i-1].monIndex];
                            tmpLen = tpFunExec(ptpMoni->express, tmpBuf, sizeof(tmpBuf));
                            if (tmpLen < 0) {
                                LOG_ERR("计算监控域[%s]表达式出错", ptpMoni->id);
                            }
                            if (tmpLen > 0) {
                                sprintf(buf+strlen(buf), "&%s=%s", ptpMoni->id, tmpBuf); 
                            }
                        }
                    }
                }
                LOG_DBG("%s", buf);
                udp_send(sockfd, buf, strlen(buf), m_webHost, gShm.ptpConf->webMonPort);
                break;
            default:
                break;
        }
    }

THD_END:
    if (sockfd != -1) close(sockfd);
    tpThreadExit();
    return NULL;    
}

