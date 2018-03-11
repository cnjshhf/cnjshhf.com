/**************************************************************
 * TCPͨѶͬ�������ӷ���˳���
 *************************************************************/
#include "queue.h"
#include "tpbus.h"

static TPTASK m_tpTask;

static char m_bindAddr[IPADDRLEN+1];  /*�����IP��ַ*/
static int  m_bindPort;               /*����������˿�*/
static int  m_lenlen;                 /*ͨѶͷ����*/
static int  m_timeout;                /*��ʱʱ��*/
static int  m_sleepts;                /*��ʱ�����������ʱ��*/ 
static int  m_nMaxThds;               /*�̳߳�Ԥ���߳���*/
static int  m_monQ;                   /*���׼��ת������*/      /*add by xmdwen @2014-12-10 15:42:26*/
static int  m_desQ;                   /*ת������        */      /*add by xmdwen @2016-10-31 16:33:00*/
static int  m_logLen;                 /*��ӡ��־���� ����̫����־ */

static char m_ign_ip1[IPADDRLEN+1];   /* ��־����ip */
static char m_ign_ip2[IPADDRLEN+1];

struct sock_info {
    int    sockfd;
    char   peerAddr[IPADDRLEN+1];
    int    peerPort;
    int    tpId;
    time_t time;
    TAILQ_ENTRY(sock_info) entry;
};
TAILQ_HEAD(, sock_info) m_tq;  /*�����׽��ֶ���*/
TAILQ_HEAD(, sock_info) m_wq;  /*�ȴ��׽��ֶ���*/

static int   tpParamInit();
static void *tpAcceptThread(void *arg);
static void *tpRequestThread(void *arg);
static void *tpResponseThread(void *arg);
static int tcp_recv_h(int sockfd, void *buf, int len, int lenlen, int timeout);

/**************************************************************
 * ������ 
 *************************************************************/
int main(int argc, char *argv[])
{
    int i,ret;
    time_t t;
    struct sock_info *pSock,*next;

    /*��������в���*/
    if (argc != 2) {
        printf("Usage: %s <����ID>\n", argv[0]);
        return -1;
    }

    /*��ʼ������*/
    if (tpTaskInit(atoi(argv[1]), "tpTcpS", &m_tpTask) != 0) {
        printf("��ʼ�����̳���\n");
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
    TAILQ_INIT(&m_wq);

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

    /*�������������߳�*/
    if (tpThreadCreate(tpAcceptThread, NULL) != 0) {
        LOG_ERR("�����߳�ʧ��: %s", strerror(errno));
        return -1;
    }
    LOG_MSG("�������������߳�...[ok]");

    /*������ʱ����*/
    while (1) {
        time(&t);
        TAILQ_LOCK(&m_wq);
        /** del by xgh 20161222 :���²��ֵ��� �޷���������
        while (TAILQ_EMPTY(&m_wq)) TAILQ_WAIT(&m_wq);
        if (TAILQ_EMPTY(&m_wq)) {
            TAILQ_UNLOCK(&m_wq);
            continue;
        }
       **/

        pSock = TAILQ_FIRST(&m_wq);
        while (pSock) {
            next = TAILQ_NEXT(pSock, entry);
            if ((t - pSock->time) > m_timeout) {
                TAILQ_REMOVE(&m_wq, pSock, entry);
                close(pSock->sockfd);
                free(pSock);
            }
            pSock = next;
        }
        TAILQ_UNLOCK(&m_wq);
        sleep(m_sleepts);
    }
}

/***************************************************************
 * ��ʼ�����ò���
 **************************************************************/
int tpParamInit()
{
    if (tpGetParamString(m_tpTask.params, "bindAddr", m_bindAddr) != 0) {
        LOG_ERR("��ȡ����[bindAddr]����");
        return -1;
    }
    LOG_MSG("����[bindAddr]=[%s]", m_bindAddr);

    if (tpGetParamInt(m_tpTask.params, "bindPort", &m_bindPort) != 0) {
        LOG_ERR("��ȡ����[bindPort]����");
        return -1;
    }
    LOG_MSG("����[bindPort]=[%d]", m_bindPort);
  
    if (tpGetParamInt(m_tpTask.params, "lenlen", &m_lenlen) != 0) m_lenlen = 4;
    LOG_MSG("����[lenlen]=[%d]", m_lenlen);

    if (tpGetParamInt(m_tpTask.params, "timeout", &m_timeout) != 0) m_timeout = 60;
    LOG_MSG("����[timeout]=[%d]", m_timeout);

    if (tpGetParamInt(m_tpTask.params, "sleepts", &m_sleepts) != 0) m_sleepts = 10;
    LOG_MSG("����[sleepts]=[%d]", m_sleepts);

    if (tpGetParamInt(m_tpTask.params, "nMaxThds", &m_nMaxThds) != 0) m_nMaxThds = 5;
    LOG_MSG("����[nMaxThds]=[%d]", m_nMaxThds);

    if (tpGetParamInt(m_tpTask.params, "monQ", &m_monQ) != 0) m_monQ = -1;
    LOG_MSG("����[monQ]=[%d]", m_monQ);
   
    if (tpGetParamInt(m_tpTask.params, "desQ", &m_desQ) != 0) m_desQ = Q_SVR;
    LOG_MSG("����[desQ]=[%d]", m_desQ);
    
    if (tpGetParamInt(m_tpTask.params, "logLen", &m_logLen) != 0) m_logLen = 300;
    LOG_MSG("����[logLen]=[%d]", m_logLen);
    
    
    if (tpGetParamString(m_tpTask.params, "ign_ip1", m_ign_ip1) != 0) strcpy(m_ign_ip1,"127.0.0.1");
    LOG_MSG("����[ign_ip1]=[%s]", m_ign_ip1);

    if (tpGetParamString(m_tpTask.params, "ign_ip2", m_ign_ip2) != 0) strcpy(m_ign_ip2,"127.0.0.1");
    LOG_MSG("����[ign_ip2]=[%s]", m_ign_ip2);

    return 0;
}

/***************************************************************
 * ���������߳�
 **************************************************************/
void *tpAcceptThread(void *arg)
{
    int    sockfd;
    struct sock_info *pSock;

    /*��ʼ�������˿�*/
    if ((sockfd = tcp_listen(m_bindAddr, m_bindPort)) == -1) {
        LOG_ERR("��ʼ�������˿�[%s:%d]����", m_bindAddr, m_bindPort);
        tpThreadExit();
        return NULL;
    }

    while (1) {
        /*�����׽�����Դ*/
        if (!(pSock = (struct sock_info*)malloc(sizeof(struct sock_info)))) {
            LOG_ERR("malloc() error: %s", strerror(errno));
            LOG_ERR("�����׽�����Դ����");
            sleep(1);
            continue;
        }
        memset(pSock, 0, sizeof(struct sock_info));

        /*������������*/
        pSock->sockfd = tcp_accept(sockfd, pSock->peerAddr, &pSock->peerPort);
        if (pSock->sockfd == -1) {
            LOG_ERR("���ܿͻ��������������");
            tpThreadExit();
            return NULL;
        }
        /* ������־�������� */
        if (strcmp(pSock->peerAddr, m_ign_ip1) != 0 && strcmp(pSock->peerAddr, m_ign_ip2) !=0) {
            LOG_MSG("���ܿͻ���[%s:%d]����������[sockfd=%d]", pSock->peerAddr, pSock->peerPort, pSock->sockfd);
        } 
        set_sock_linger(pSock->sockfd, 1, 1);

        /*д�������׽��ֶ���*/
        TAILQ_LOCK(&m_tq);
        TAILQ_INSERT_TAIL(&m_tq, pSock, entry);
        TAILQ_UNLOCK(&m_tq);
        TAILQ_SIGNAL(&m_tq);
    }
}

/***************************************************************
 * �������߳�
 **************************************************************/
void *tpRequestThread(void *arg)
{
    int  ret,msgLen;
    char logPrefix[21];
    MQHDL *mqh;
    TPMSG tpMsg;
    time_t t, t1, t2;
    struct sock_info *pSock;
    char log_buf[MAXMSGLEN];
    int log_len;

    int sk_f = 0;         /* ��־���˱�ʶ */

    /*�󶨶���*/
    if ((ret = mq_bind(m_tpTask.bindQ, &mqh)) != 0) {
        LOG_ERR("mq_bind() error: retcode=%d", ret);
        LOG_ERR("�󶨶���[%d]����", m_tpTask.bindQ);
        tpThreadExit();
        return NULL;        
    }

    while (1) {
        /*��ȡ�����׽���*/
        TAILQ_LOCK(&m_tq);
        while (TAILQ_EMPTY(&m_tq)) TAILQ_WAIT(&m_tq);
        if (TAILQ_EMPTY(&m_tq)) {
            TAILQ_UNLOCK(&m_tq);
            continue;
        }
        pSock = TAILQ_FIRST(&m_tq);
        TAILQ_REMOVE(&m_tq, pSock, entry);
        TAILQ_UNLOCK(&m_tq);

         time(&t1);
        /*����������  udpated by fox 2016/12/29 */
        LOG_MSG("tcp_recv_h [%s:%d]sockfd [%d]",pSock->peerAddr, pSock->peerPort ,pSock->sockfd);
        if((msgLen = tcp_recv_h(pSock->sockfd, tpMsg.body, 0, m_lenlen, 0)) <= 0)
        {
          if(strcmp(pSock->peerAddr, m_ign_ip1) && strcmp(pSock->peerAddr, m_ign_ip2))
          {
            LOG_DBG("[RECV] tcp_recv return [%d]", msgLen);
            if (msgLen != -2)
              LOG_ERR("���տͻ��������ĳ���");
            else
              LOG_MSG("�ͻ��˶Ͽ�����");
          }
          close(pSock->sockfd);
          free(pSock);
          continue;
        }
        time(&t2);
        if( t2 - t1 > 1){
            LOG_MSG("tcp_recv_h ���ܱ��ĳ���1��:��ʼʱ��[%d]->����ʱ��[%d][%d][%s:%d] sockfd [%d]", t1, t2, pSock->peerAddr, pSock->peerPort ,pSock->sockfd );
        }

        /* ��ʼ������ͷ */
        if (tpNewHead(m_tpTask.bindQ, &tpMsg.head) != 0) {
            LOG_ERR("��ʼ������ͷ����");
            close(pSock->sockfd);
            free(pSock);
            continue;
        }
        tpMsg.head.bodyLen = msgLen;
        /*tpMsg.head.corrId = pSock->sockfd;*/

        if(tpMsg.head.bodyLen>m_logLen)
            log_len = m_logLen;
        else
            log_len = tpMsg.head.bodyLen;
        memcpy(log_buf, tpMsg.body, log_len);
        log_buf[log_len] = 0x00;

        tpSetLogPrefix(tpMsg.head.tpId, logPrefix);
        LOG_MSG("���տͻ���������...[ok][sockfd=%d][%d][%s]",pSock->sockfd, tpMsg.head.bodyLen, log_buf);

        /*д��ȴ��׽��ֶ���*/
        time(&t);
        TAILQ_LOCK(&m_wq);
        pSock->tpId = tpMsg.head.tpId;
        pSock->time = t;
        TAILQ_INSERT_TAIL(&m_wq, pSock, entry);
        TAILQ_UNLOCK(&m_wq);

        /*���ͽ��׼�ض��� add by xmdwen @2014-12-10 15:54:59 bgn*/
        if( m_monQ >= 0)
        {
            if (ret= mq_post(mqh, NULL, m_monQ, 0, NULL, (char *)&tpMsg, sizeof(TPMSG), 0)) {
                LOG_ERR("write queue_mon[%d] error[%d]",m_monQ,ret);
            }
            LOG_MSG("write queue_mon[%d] OK",m_monQ);
        }
        /*���ͽ��׼�ض��� add by xmdwen @2014-12-10 15:54:59 end*/
        
        /*����TPBUS����*/
        if (tpSend1(mqh, &tpMsg, m_desQ, 0) != 0) {
            LOG_ERR("����TPBUS�������");
            TAILQ_LOCK(&m_wq);
            TAILQ_REMOVE(&m_wq, pSock, entry);
            TAILQ_UNLOCK(&m_wq);
            close(pSock->sockfd);
            free(pSock);
            continue;
        }
        LOG_MSG("����TPBUS����...[ok]");	
    }
}

/***************************************************************
 * ��Ӧ�����߳�
 **************************************************************/
void *tpResponseThread(void *arg)
{
    int  ret;
    char logPrefix[21];
    MQHDL *mqh;
    TPMSG tpMsg;
    struct sock_info *pSock;
    char log_buf[MAXMSGLEN];
    int log_len;

    /*�󶨶���*/
    if ((ret = mq_bind(m_tpTask.bindQ, &mqh)) != 0) {
        LOG_ERR("mq_bind() error: retcode=%d", ret);
        LOG_ERR("�󶨶���[%d]����", m_tpTask.bindQ);
        tpThreadExit();
        return NULL;        
    }

    while (1) {
        /*����TPBUS����*/
        if (tpRecv1(mqh, &tpMsg, m_desQ, 0, 0) != 0) {
            LOG_ERR("����TPBUS���س���");
            mq_detach(mqh);
            tpThreadExit();
            return NULL;
        }
        tpSetLogPrefix(tpMsg.head.tpId, logPrefix);
        LOG_MSG("����TPBUS����...[ok]");

        TAILQ_LOCK(&m_wq);
        pSock = TAILQ_FIRST(&m_wq);
        while (pSock) {
            /*if (pSock->sockfd == tpMsg.head.corrId && pSock->tpId == tpMsg.head.tpId) break;*/
            if (pSock->tpId == tpMsg.head.tpId) break;
            pSock = TAILQ_NEXT(pSock, entry);
        }
        if (pSock) TAILQ_REMOVE(&m_wq, pSock, entry);
        TAILQ_UNLOCK(&m_wq);

        if (!pSock) {
            LOG_ERR("�ٵ���Ӧ(tpId=[%d]): ������ʱ�ѹر�", tpMsg.head.tpId);
            continue;
        } 

        if(tpMsg.head.bodyLen>m_logLen)
            log_len = m_logLen;
        else
            log_len = tpMsg.head.bodyLen;
        memcpy(log_buf, tpMsg.body, log_len);
        log_buf[log_len] = 0x00;

        if (tcp_send(pSock->sockfd, tpMsg.body, tpMsg.head.bodyLen, m_lenlen, 0) == -1) {
            LOG_ERR("tcp_send() error: %s", strerror(errno));
            LOG_ERR("���Ϳͻ�����Ӧ���ĳ���[sockfd=%d]", pSock->sockfd);
        } else {
            LOG_MSG("���Ϳͻ�����Ӧ����...[ok][sockfd=%d][%d][%s]", pSock->sockfd, tpMsg.head.bodyLen, log_buf);
            
            /*���ͽ��׼�ض��� add by xmdwen @2014-12-10 15:54:59 bgn*/
            if( m_monQ >= 0)
            {
                if (ret= mq_post(mqh, NULL, m_monQ, 0, NULL, (char *)&tpMsg, sizeof(TPMSG), 0)) {
                    LOG_ERR("write queue_mon[%d] error[%d]",m_monQ,ret);
                }
                LOG_MSG("write queue_mon[%d] OK",m_monQ);
            }
            /*���ͽ��׼�ض��� add by xmdwen @2014-12-10 15:54:59 end*/
        }
        close(pSock->sockfd);
        free(pSock);
    }
}

/****************************************************************
 * ������Ϣ����(��ָ��Ԥ������)
 ****************************************************************/
int tcp_recv_h(int sockfd, void *buf, int len, int lenlen, int timeout)
{
    char lenbuf[11];
    int    nread,flags;
    int    nsecs = timeout;

	char   tbuf[1+1];
	int    nget;

    if (lenlen > 0) {
        if (lenlen >= sizeof(lenbuf)) lenlen = sizeof(lenbuf) - 1;
		/* no consider of EINTR */
        nget = recv(sockfd, tbuf, 1, MSG_PEEK);
        if (nget == -1 && errno == EINTR)
            nget = recv(sockfd, tbuf, 1, MSG_PEEK);
		/* 2 times try to get 1 byte for testing recieve */
        if (tcp_read(sockfd, lenbuf, lenlen, timeout) != 0)
        {
            if (0 == nget)	/* 0 byte recv,means no byte been sent */
                return -2;
            else			/* 1 byte recv succ,means sent error */
                return -1;
        }
        lenbuf[lenlen] = 0;
        len = atoi(lenbuf);
        if (tcp_read(sockfd, buf, len, timeout) != 0) return -1;
        return len;
    } else {
        if (len > 0) {
            if (tcp_read(sockfd, buf, len, timeout) != 0) return -1;
            return len;
        }

        if (timeout <= 0) {
            while (1) {
                if ((nread = read(sockfd, buf, SORCVBUFSZ)) == -1) {
                    if (errno == EINTR) continue;
                    return -1;
                } else if (nread == 0) {
                    errno = ECONNRESET;
                    return -1;
                } else {
                    return nread;
                }
            }
        } else {
            flags = fcntl(sockfd, F_GETFL, 0);
            fcntl(sockfd, F_SETFL, flags | O_NONBLOCK); 
            /* ���÷�����ģʽ */
            while (nsecs > 0) {
                if (readable(sockfd, 1)) {
                    if ((nread = read(sockfd, buf, SORCVBUFSZ)) == -1) {
                        return -1;
                    } else if (nread == 0) {
                        errno = ECONNRESET;
                        return -1;
                    } else {
                        fcntl(sockfd, F_SETFL, flags);
                        return nread;
                    }
                }
                nsecs--;
            }
            fcntl(sockfd, F_SETFL, flags);
            errno = ETIMEDOUT;
            return -1;
        }
    }
}


/***************************************************************
 * ���ͷ�������(���������ͨѶ����)
 **************************************************************/
int tpSend1(MQHDL *mqh, TPMSG *ptpMsg, int m_desq, int flag)
{
    int   i,ret,msgLen;
    char *rqm,clazz[MSGCLZLEN+1];

    i = (ptpMsg->head.tpId/100000000)%10;
    rqm = (i==0)? NULL : gShm.ptpConf->hosts[i-1].qm;
    clazz[0] = 0;
    if (flag) snprintf(clazz, sizeof(clazz), "%d", ptpMsg->head.tpId);
    msgLen = ptpMsg->head.bodyLen + sizeof(TPHEAD);
    if ((ret = mq_post(mqh, rqm, m_desq, 0, clazz, (char *)ptpMsg, msgLen, 0)) != 0) {
        LOG_ERR("mq_post() error: retcode=%d", ret);
        LOG_ERR("д���ض���[%d]����", m_desq);
        return -1;
    }
    /**
    LOG_MSG("д���ض���[%d]...[ok]", m_desq);
    **/
    return 0;
}

/***************************************************************
 * ���շ�����Ӧ(���������ͨѶ����)
 **************************************************************/
int tpRecv1(MQHDL *mqh, TPMSG *ptpMsg, int m_desq, int timeout, int flag)
{
    int  q,ret,msgLen;
    char clazz[MSGCLZLEN+1];

    clazz[0] = 0;
    if (flag) snprintf(clazz, sizeof(clazz), "%d", ptpMsg->head.tpId);
    msgLen = sizeof(TPMSG);
    if ((ret = mq_pend(mqh, NULL, &q, NULL, clazz, (char *)ptpMsg, &msgLen, timeout)) != 0) {
        LOG_ERR("mq_pend() error: retcode=%d", ret);
        LOG_ERR("���󶨶���[%d]����", mqh->qid);
        return -1;
    }
    if (q == m_desq) {
        /**
        LOG_MSG("�յ��������س���ı���");
        **/
        return 0;
    }
    else if(q == Q_REV)
    {
        /**
        LOG_MSG("�յ������������ı���");
        **/
        return 0;
    }
    else {
        LOG_ERR("�յ����Զ���[%d]�ķǷ�����", q);
        return -1;
    }
    return 0;
}