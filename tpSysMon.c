/***************************************************************
 * ϵͳ��س���
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
 * ������
 **************************************************************/
int main(int argc, char *argv[])
{
    int  i,ret;
    MQHDL *mqh;

    /*��������в���*/
    if (argc != 2) {
        printf("Usage: %s <����ID>\n", argv[0]);
        return -1;
    }

    /*��ʼ������*/
    /*if (tpTaskInit(TSK_SYSMON, "tpSysMon", NULL) != 0) { */
    if (tpTaskInit(atoi(argv[1]), "tpSysMon", NULL) != 0) {
        LOG_ERR("��ʼ�����̳���");
        return -1;
    }
    setLogCallBack(NULL);

    /*��黷������*/
    if (!getenv("TPBUS_WEBHOST")) {
        LOG_ERR("δ���û�������[TPBUS_WEBHOST]");
        return -1;
    }
    strcpy(m_webHost, getenv("TPBUS_WEBHOST"));

    /*������ϵͳ*/
    if (getSysName(m_uname) != 0) {
        LOG_ERR("�޷����ϵͳ����");
        return -1;
    }

    /*��ʼ��UDP�����˿�*/
    if ((m_sock = udp_init(NULL, gShm.ptpConf->sysMonPort)) == -1) {
        LOG_ERR("��ʼ��UDP�����˿ڳ���: %s", strerror(errno));
        return -1;
    }

    /*��ʼ������*/
    if ((ret = mq_init()) != 0) {
        LOG_ERR("mq_init() error: retcode=%d", ret);
        LOG_ERR("��ʼ�����г���");
        return -1;
    }

    /*�󶨶���*/
    if ((ret = mq_bind(Q_MON, &mqh)) != 0) {
        LOG_ERR("mq_bind() error: retcode=%d", ret);
        LOG_ERR("�󶨶���[%d]����", Q_MON);
        return -1;
    }

    /*����ϵͳ״̬����߳�*/
    if (tpThreadCreate(tpSysMonThread, NULL) != 0) {
        LOG_ERR("����ϵͳ״̬����߳�ʧ��: %s", strerror(errno));
        return -1;
    }

    pthread_mutex_init(&m_mtx, NULL);

    /*������ر��Ĵ����̳߳�*/
    for (i=0; i<gShm.ptpConf->nMonProcs; i++) {
        if (tpThreadCreate(tpSysMonThread2, NULL) != 0) {
            LOG_ERR("������ر��Ĵ����߳�ʧ��: %s", strerror(errno));
            return -1;
        }
    }

    while (1) {
        sleep(1);

        /*��Ⱥ�ַ�������״̬(TODO)*/
    }

    return 0;
}

/***************************************************************
 * ��ȡϵͳ����(uname����)
 **************************************************************/
static int getSysName(char *result)
{
    struct utsname buf;

    if (uname(&buf) != 0) return -1;
    strcpy(result, buf.sysname);
    return 0;
}

/***************************************************************
 * ��ȡϵͳ״̬��Ϣ(vmstat����)
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
 * ��ȡ����״̬��Ϣ(ps����)
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
        strcpy(result, "δ����");
    } else {
        if (ptpTask->pid > 0) {
            strcpy(result, "������");
        } else {
            strcpy(result, "��ֹͣ");
        }
    }
    return result;
}

/***************************************************************
 * ϵͳ״̬����߳�
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

    /*��ʼ��UDP�׽���(����ת��״̬��Ϣ)*/
    if ((sockfd = udp_init(NULL, 0)) == -1) {
        LOG_ERR("��ʼ��UDP�׽��ֳ���: %s", strerror(errno));
        goto THD_END;
    }

    while (1) {
        sleep(gShm.ptpConf->ntsSysMon);

        if (tpShmRetouch() != 0) {
            LOG_ERR("ˢ�¹����ڴ�ָ�����");
            continue;
        }

        /*SYS*/
        /*����ID|SYS|pi|po|us|sy|id|wa*/
        if (get_vm_stat(&vmStat) == 0) {
            sprintf(buf, "%d&SYS&%d&%d&%d&%d&%d&%d",
                    gShm.pShmHead->hostId,
                    vmStat.pi, vmStat.po,
                    vmStat.us, vmStat.sy, vmStat.id, vmStat.wa);

            LOG_DBG("%s", buf);
            udp_send(sockfd, buf, strlen(buf), m_webHost, gShm.ptpConf->webMonPort);
        }

        /*TSK*/
        /*����ID|TSK|����ID|��������|�󶨶���|ִ�г���|���̺�|%CPU|%MEM|��ͣ����|��ͣʱ��|״̬*/
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
        /*����ID|SRV|�������|��������|���������|��ǰ������|��ǰ������*/
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
        /*����ID|MQM|����ID|������|д������|д��ʱ��|д�����|��������|����ʱ��|��������|��������|��������*/
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
 * ��ȡ���״�������Ϣ
 **************************************************************/
static char *getStatus(int status, char *result)
{
    result[0] = 0;
    switch (status) {
        case TP_OK:
            strcpy(result, "��������");
            break;
        case TP_TIMEOUT:
            strcpy(result, "����ʱ");
            break;
        case TP_REVOK:
            strcpy(result, "��������");
            break;
        case TP_REVFAIL:
        case TP_SAFFAIL:
            strcpy(result, "����ʧ��");
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
 * ��ر��Ĵ����߳�
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

    /*��ʼ��UDP�׽���(����ת����ر���)*/
    if ((sockfd = udp_init(NULL, 0)) == -1) {
        LOG_ERR("��ʼ��UDP�׽��ֳ���: %s", strerror(errno));
        goto THD_END;
    }

    tpCtxSetTSD(&tpCtx);
    tpCtx.msgCtx = &msgCtx;
    tpPreBufInit(&tpCtx.preBuf, NULL);

    /*ѭ�����ա�����ת����ر���*/
    while (1) {
        pthread_mutex_lock(&m_mtx);
        msgLen = udp_recv(m_sock, (char *)ptpMsg, sizeof(TPMSG), NULL, NULL);
        pthread_mutex_unlock(&m_mtx);

        if (msgLen <= 0) {
            LOG_ERR("���ռ�ر��ĳ���: %s", strerror(errno));
            goto THD_END;
        }
        if (msgLen != (ptpHead->bodyLen + sizeof(TPHEAD))) {
            LOG_ERR("���ռ�ر��Ĳ�����");
            continue;
        }

        if (tpShmRetouch() != 0) {
            LOG_ERR("ˢ�¹����ڴ�ָ�����");
            continue;
        }

        switch (ptpHead->msgType) {
            case MSGMON_EVT:
                /*����ID|EVT|�¼���Ϣ*/
                ptpMsg->body[ptpHead->bodyLen] = 0;
                sprintf(buf, "%d&EVT&%s", gShm.pShmHead->hostId, ptpMsg->body);
                LOG_DBG("%s", buf);
                udp_send(sockfd, buf, strlen(buf), m_webHost, gShm.ptpConf->webMonPort);
                break;
            case MSGAPP:
            case MSGREV:
            case MSGSYS_TIMEOUT:
                /*����ID|MON|��ˮ��|����ʱ��|�������|����˵�|�������|Ŀ��˵�|��ʱ|״̬|...*/
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

                if (ptpHead->monFlag) { /*�Զ�����*/
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
                                LOG_ERR("��������[%s]���ʽ����", ptpMoni->id);
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

