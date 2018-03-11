/**************************************************************
 * TCPͨѶͬ�������ӿͻ��˳���
 *************************************************************/
#include "queue.h"
#include "tpbus.h"

static TPTASK m_tpTask;

char m_bindAddr[IPADDRLEN+1];  /*���ض�IP��ַ*/
char m_peerAddr[IPADDRLEN+1];  /*�����IP��ַ*/
int  m_peerPort;               /*����������˿�*/
int  m_lenlen;                 /*ͨѶͷ����*/
int  m_timeout;                /*��ʱʱ��*/
int  m_selectus;               /*�ɶ������ʱ��*/
int  m_nMaxThds;               /*��󲢳���*/
int  m_srcQ;                   /*Ŀ�����*/

struct sock_info {
    int  sockfd;
    time_t time;
    TPHEAD head;
    TAILQ_ENTRY(sock_info) entry;
};
TAILQ_HEAD(, sock_info) m_tq;   /*�����׽��ֶ���*/
TAILQ_HEAD(, sock_info) m_sq;   /*�ɶ��׽��ֶ���*/

static struct sock_info *m_socks[FD_SETSIZE];

static int   tpParamInit();
static void *tpRequestThread(void *arg);
static void *tpResponseThread(void *arg);

/**************************************************************
 * ������ 
 *************************************************************/
int main(int argc, char *argv[])
{
    int i,j,ret,maxfd;
    fd_set rset;
    time_t t;
    struct timeval tval;
    struct sock_info *pSock,*next;

    /*��������в���*/
    if (argc != 2) {
        printf("Usage: %s <����ID>\n", argv[0]);
        return -1;
    }

    /*��ʼ������*/
    if (tpTaskInit(atoi(argv[1]), "tpTcpC", &m_tpTask) != 0) {
        printf("��ʼ�����̳���");
        return -1;
    }

    /*��ʼ�����ò���*/
    if (tpParamInit() != 0) {
        LOG_ERR("��ʼ�����ò�������");
        return -1;
    }

    /*��ʼ������*/
    if ((ret = mq_init()) != 0) {
        LOG_ERR("mq_init() error: retcode=%d", ret);
        LOG_ERR("��ʼ�����г���");
        return -1;
    }

    TAILQ_INIT(&m_tq);
    TAILQ_INIT(&m_sq);

    /*�����������̳߳�*/
    for (i=0; i<m_nMaxThds; i++) {
        if (tpThreadCreate(tpRequestThread, NULL) != 0) {
            LOG_ERR("�����߳�ʧ��: %s", strerror(errno));
            return -1;
        }
    }
    LOG_MSG("������Ӧ�����̳߳�...[ok]");

    /*������Ӧ�����̳߳�*/
    for (i=0; i<m_nMaxThds; i++) {
        if (tpThreadCreate(tpResponseThread, NULL) != 0) {
            LOG_ERR("�����߳�ʧ��: %s", strerror(errno));
            return -1;
        }
    }
    LOG_MSG("������Ӧ�����̳߳�...[ok]");

    tval.tv_sec  = 0;
    tval.tv_usec = m_selectus;

    while (1) {
        time(&t);
        j = 0;
        maxfd = -1;
        FD_ZERO(&rset);

        TAILQ_LOCK(&m_tq);
        while (TAILQ_EMPTY(&m_tq)) TAILQ_WAIT(&m_tq);
        if (TAILQ_EMPTY(&m_tq)) {
            TAILQ_UNLOCK(&m_tq);
            continue;
        }

        pSock = TAILQ_FIRST(&m_tq);
        while (pSock) {
            next = TAILQ_NEXT(pSock, entry);
            if ((t - pSock->time) > m_timeout) {
                TAILQ_REMOVE(&m_tq, pSock, entry);
                close(pSock->sockfd);
                free(pSock);
                pSock = next;
                continue;
            }
            FD_SET(pSock->sockfd, &rset);
            if (pSock->sockfd > maxfd) maxfd = pSock->sockfd;
            m_socks[j++] = pSock;
            pSock = next;
        }
        TAILQ_UNLOCK(&m_tq);

        if (maxfd < 0) {usleep(m_selectus); continue;}
        if ((ret = select(maxfd+1, &rset, NULL, NULL, &tval)) > 0) {
            for (i=0; i<j; i++) {
                pSock = m_socks[i];
                if (FD_ISSET(pSock->sockfd, &rset)) { 
                    TAILQ_LOCK(&m_tq);
                    TAILQ_REMOVE(&m_tq, pSock, entry);
                    TAILQ_UNLOCK(&m_tq);

                    /*д��ɶ��׽��ֶ���*/
                    TAILQ_LOCK(&m_sq);
                    TAILQ_INSERT_TAIL(&m_sq, pSock, entry);
                    TAILQ_UNLOCK(&m_sq);
                    TAILQ_SIGNAL(&m_sq);
                }
            }
        }
    }
}

/***************************************************************
 * ��ʼ�����ò���
 **************************************************************/
int tpParamInit()
{
    if (tpGetParamString(m_tpTask.params, "bindAddr", m_bindAddr) != 0) strcpy(m_bindAddr, "localhost");
    LOG_MSG("����[bindAddr]=[%s]", m_bindAddr);

    if (tpGetParamString(m_tpTask.params, "peerAddr", m_peerAddr) != 0) {
        LOG_ERR("��ȡ����[peerAddr]����");
        return -1;
    }
    LOG_MSG("����[peerAddr]=[%s]", m_peerAddr);

    if (tpGetParamInt(m_tpTask.params, "peerPort", &m_peerPort) != 0) {
        LOG_ERR("��ȡ����[peerPort]����");
        return -1;
    }
    LOG_MSG("����[peerPort]=[%d]", m_peerPort);
  
    if (tpGetParamInt(m_tpTask.params, "lenlen", &m_lenlen) != 0) m_lenlen = 4;
    LOG_MSG("����[lenlen]=[%d]", m_lenlen);

    if (tpGetParamInt(m_tpTask.params, "timeout", &m_timeout) != 0) m_timeout = 60;
    LOG_MSG("����[timeout]=[%d]", m_timeout);

    if (tpGetParamInt(m_tpTask.params, "selectus", &m_selectus) != 0) m_selectus = 100;
    LOG_MSG("����[selectus]=[%d]", m_selectus);

    if (tpGetParamInt(m_tpTask.params, "nMaxThds", &m_nMaxThds) != 0) m_nMaxThds = 5;
    LOG_MSG("����[nMaxThds]=[%d]", m_nMaxThds);

    if (tpGetParamInt(m_tpTask.params, "srcQ", &m_srcQ) != 0) m_srcQ = Q_SVR;
    LOG_MSG("����[srcQ]=[%d]", m_srcQ);

    return 0;
}
/***************************************************************
 * ���շ�����Ӧ(���������ͨѶ����)
 **************************************************************/
int tpRecv_N(MQHDL *mqh, TPMSG *ptpMsg)
{
    int  q,ret,msgLen;
    char clazz[MSGCLZLEN+1];

    msgLen = sizeof(TPMSG);
    if ((ret = mq_pend(mqh, NULL, &q, NULL, clazz, (char *)ptpMsg, &msgLen, 0)) != 0) {
        LOG_ERR("mq_pend() error: retcode=%d", ret);
        LOG_ERR("���󶨶���[%d]����", mqh->qid);
        return -1;
    }
    if (q == m_srcQ) {
        LOG_MSG("�յ��������س���ı���");
    }
    else if(q == Q_REV)
    {
        LOG_MSG("�յ������������ı���");
    }
    else {
        LOG_ERR("�յ����Զ���[%d]�ķǷ�����", q);
        return -1;
    }

    return 0;
}
/***************************************************************
 * �������߳�
 **************************************************************/
void *tpRequestThread(void *arg)
{
    int     ret,sockfd;
    char    logPrefix[21];
    MQHDL   *mqh;
    TPMSG   tpMsg;
    time_t  t;
    struct  sock_info *pSock;
    char    msgbuf[MAXMSGLEN];        /*add by dwxiong 2014-11-7 19:43:05 @ֱ������.�������յ�*/

    /*�󶨶���*/
    if ((ret = mq_bind(m_tpTask.bindQ, &mqh)) != 0) {
        LOG_ERR("mq_bind() error: retcode=%d", ret);
        LOG_ERR("�󶨶���[%d]����", m_tpTask.bindQ);
        tpThreadExit();
        return NULL;
    }

    while (1) {
        /*����TPBUS����*/
        if (tpRecv_N(mqh, &tpMsg) != 0) {
            LOG_ERR("����TPBUS�������");
            mq_detach(mqh);
            tpThreadExit();
            return NULL;
        }
        tpSetLogPrefix(tpMsg.head.tpId, logPrefix);
        LOG_MSG("�յ�TPBUS����");
        if (hasLogLevel(DEBUG)) tpHeadDebug(&tpMsg.head);
        if (hasLogLevel(DEBUG)) LOG_HEX(tpMsg.body, tpMsg.head.bodyLen);

        /*��ʼ��ͨѶ����*/
        if ((sockfd = tcp_connect(m_bindAddr, m_peerAddr, m_peerPort, 0)) == -1) {
            LOG_ERR("tcp_connect(%s,%d) error", m_peerAddr, m_peerPort);
            LOG_ERR("���ӷ����[%s:%d]����", m_peerAddr, m_peerPort);
            continue;
        }
        LOG_MSG("���ӷ����[%s:%d]...[ok]", m_peerAddr, m_peerPort);
        set_sock_linger(sockfd, 1, 1);

        /*���ͷ����������*/
        /*if (tcp_send(sockfd, tpMsg.body, tpMsg.head.bodyLen, m_lenlen, 0) == -1) {
            LOG_ERR("���ͷ���������ĳ���");
            close(sockfd);
            continue;
        }*/
        /*add by dwxiong 2014-11-7 19:43:05 @ֱ������.�������յ�  bgn*/
        memset(msgbuf,0x00,sizeof(msgbuf));
        snprintf(msgbuf, sizeof(msgbuf), "%*.*d", m_lenlen, m_lenlen, tpMsg.head.bodyLen);
        memcpy(msgbuf+m_lenlen,tpMsg.body,tpMsg.head.bodyLen);
        msgbuf[MAXMSGLEN-1] = '\0';
        if ((ret = tcp_send(sockfd, msgbuf, tpMsg.head.bodyLen+m_lenlen, -1, 0)) == -1) {
            LOG_ERR("�����˷��ͱ��ĳ���");
            close(sockfd);
            continue;
        }
        /*add by dwxiong 2014-11-7 19:43:05 @ֱ������.�������յ�  end*/
        
        if (hasLogLevel(DEBUG)) LOG_HEX(msgbuf, tpMsg.head.bodyLen+m_lenlen);
        
        LOG_MSG("���ͷ����������...[ok]");

        /*�����׽�����Դ*/
        if (!(pSock = (struct sock_info*)malloc(sizeof(struct sock_info)))) {
            LOG_ERR("malloc() error: %s", strerror(errno));
            LOG_ERR("�����׽�����Դ����");
            close(sockfd);
            sleep(1);
            continue;
        }
        memset(pSock, 0, sizeof(struct sock_info));

        /*д�������׽��ֶ���*/
        time(&t);
        TAILQ_LOCK(&m_tq);
        pSock->sockfd = sockfd;
        pSock->time = t;
        memcpy(&pSock->head, &tpMsg.head, sizeof(TPHEAD));
        TAILQ_INSERT_TAIL(&m_tq, pSock, entry);
        TAILQ_SIGNAL(&m_tq);
        TAILQ_UNLOCK(&m_tq);
    }
}

/***************************************************************
 * ��Ӧ�����߳�
 **************************************************************/
void *tpResponseThread(void *arg)
{
    int ret;
    char logPrefix[21];
    MQHDL *mqh;
    TPMSG tpMsg;
    struct sock_info *pSock;

    /*�󶨶���*/
    if ((ret = mq_bind(m_tpTask.bindQ, &mqh)) != 0) {
        LOG_ERR("mq_bind() error: retcode=%d", ret);
        LOG_ERR("�󶨶���[%d]����", m_tpTask.bindQ);
        tpThreadExit();
        return NULL;        
    }

    while (1) {
        /*��ȡ�ɶ��׽���*/
        TAILQ_LOCK(&m_sq);
        while (TAILQ_EMPTY(&m_sq)) TAILQ_WAIT(&m_sq);
        if (TAILQ_EMPTY(&m_sq)) {
            TAILQ_UNLOCK(&m_sq);
            continue;
        }
        pSock = TAILQ_FIRST(&m_sq);
        TAILQ_REMOVE(&m_sq, pSock, entry);
        TAILQ_UNLOCK(&m_sq);

        memcpy(&tpMsg.head, &pSock->head, sizeof(TPHEAD));
        tpSetLogPrefix(tpMsg.head.tpId, logPrefix);
        
        /*���շ���˷��ر���*/
        if ((tpMsg.head.bodyLen = tcp_recv(pSock->sockfd, tpMsg.body, 0, m_lenlen, 0)) == -1) {
            LOG_ERR("tcp_recv() error: %s", strerror(errno));
            LOG_ERR("���շ���˷��ر��ĳ���");
        } else {
            LOG_MSG("���շ���˷��ر���...[ok]");
            if (hasLogLevel(DEBUG)) LOG_HEX(tpMsg.body, tpMsg.head.bodyLen);

            /*����TPBUS��Ӧ*/
            if (tpReturn1(mqh, &tpMsg, m_srcQ) != 0) {
               LOG_ERR("����TPBUS��Ӧ����");
            } else {
               LOG_MSG("����TPBUS��Ӧ...[ok]");
            }
        }
        close(pSock->sockfd);
        free(pSock);
    }
}
/***************************************************************
 * ���ط�����Ӧ(���ڷ����ͨѶ����)
 **************************************************************/
int tpReturn1(MQHDL *mqh, TPMSG *ptpMsg, int desQ)
{
    char *rqm;
    int i,j,ret,msgLen;

    msgLen = ptpMsg->head.bodyLen + sizeof(TPHEAD);

    if (ptpMsg->head.tpId) {
        i = (ptpMsg->head.tpId/100000000)%10;
        rqm = (i==0)? NULL : gShm.ptpConf->hosts[i-1].qm;
        if ((ret = mq_post(mqh, rqm, desQ, 0, NULL, (char *)ptpMsg, msgLen, 0)) != 0) {
            LOG_ERR("mq_post() error: retcode=%d", ret);
            LOG_ERR("д���ض���[%d]����", desQ);
            return -1;
        }
    } else {
        if (!gShm.ptpConf->dispatch[0]) { /*�Ǽ�Ⱥģʽ*/
            if ((ret = mq_post(mqh, NULL, desQ, 0, NULL, (char *)ptpMsg, msgLen, 0)) != 0) {
                LOG_ERR("mq_post() error: retcode=%d", ret);
                LOG_ERR("д���ض���[%d]����", desQ);
                return -1;
            }
        } else { /*��Ⱥģʽ*/
            for (i=0,j=0; i<NMAXHOSTS; i++) {
                if (!gShm.ptpConf->hosts[i].id) continue;
                if (!gShm.ptpConf->hosts[i].status) continue;
                rqm = gShm.ptpConf->hosts[i].qm;
                if ((ret = mq_post(mqh, rqm, desQ, 0, NULL, (char *)ptpMsg, msgLen, 0)) != 0) {
                    LOG_ERR("mq_post() error: retcode=%d", ret);
                    LOG_ERR("д���ض���[%d]����", desQ);
                    return -1;
                }
                j++;
            }
            if (j==0) {
                LOG_ERR("û���ҵ����õķ�����");
                return -1;
            }
        }
    }
    LOG_MSG("д���ض���[%d]...[ok]", desQ);
    return 0;
}

