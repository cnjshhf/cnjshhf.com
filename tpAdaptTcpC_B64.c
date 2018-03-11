/**************************************************************
 * TCPͨѶͬ�������ӿͻ��˳���      ���� <-> Base64����
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
    if (tpTaskInit(atoi(argv[1]), "tpTcpC_B64", &m_tpTask) != 0) {
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
    /* 2014/12/24 dc �޸ĳ��� for Base64���뱨�� */
    char    msgbuf[MAXMSGLEN/3*4+12];        /*add by dwxiong 2014-11-7 19:43:05 @ֱ������.�������յ�*/

    char    b64msg[MAXMSGLEN/3*4];
    int     b64len;

    /*�󶨶���*/
    if ((ret = mq_bind(m_tpTask.bindQ, &mqh)) != 0) {
        LOG_ERR("mq_bind() error: retcode=%d", ret);
        LOG_ERR("�󶨶���[%d]����", m_tpTask.bindQ);
        tpThreadExit();
        return NULL;
    }

    while (1) {
        /*����TPBUS����*/
        if (tpAccept(mqh, &tpMsg) != 0) {
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
        /*if (tcp_send(sockfd, b64msg, b64len, m_lenlen, 0) == -1) {
            LOG_ERR("���ͷ���������ĳ���");
            close(sockfd);
            continue;
        }*/

        /* 2014/12/24 add by dc */
        /* ����תBase64���� */
        LOG_MSG("����->Base64����");
        b64len = Base64Encode(b64msg, tpMsg.body, tpMsg.head.bodyLen);
        if (b64len == -1)
        {
            LOG_ERR("Base64�������");
            close(sockfd);
            continue;
        }
        /*add by dwxiong 2014-11-7 19:43:05 @ֱ������.�������յ�  bgn*/
        /* Base64�����޸� */
        snprintf(msgbuf, sizeof(msgbuf), "%*.*d", m_lenlen, m_lenlen, b64len);
        memcpy(msgbuf+m_lenlen,b64msg,b64len+1);
        /* msgbuf[MAXMSGLEN-1] = '\0'; */
        if ((ret = tcp_send(sockfd, msgbuf, b64len+m_lenlen, -1, 0)) == -1) {
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

    char    b64msg[MAXMSGLEN/3*4];
    int     b64len;

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
        /* 2014/12/24 add by dc */
        /* Base64����ת���� */
        if ((b64len = tcp_recv(pSock->sockfd, b64msg, 0, m_lenlen, 0)) == -1) {
            LOG_ERR("tcp_recv() error: %s", strerror(errno));
            LOG_ERR("���շ���˷��ر��ĳ���");
        } else {
            LOG_MSG("���շ���˷��ر���...[ok]");
            if (hasLogLevel(DEBUG)) LOG_HEX(b64msg, b64len);

            LOG_MSG("Base64����->����");
            tpMsg.head.bodyLen = Base64Decode(tpMsg.body, b64msg, b64len);
            if (tpMsg.head.bodyLen == -1)
            {
                LOG_ERR("Base64�������");
                continue;
            }
            if (hasLogLevel(DEBUG)) LOG_HEX(tpMsg.body, tpMsg.head.bodyLen);

            /*����TPBUS��Ӧ*/
            if (tpReturn(mqh, &tpMsg) != 0) {
               LOG_ERR("����TPBUS��Ӧ����");
            } else {
               LOG_MSG("����TPBUS��Ӧ...[ok]");
            }
        }
        close(pSock->sockfd);
        free(pSock);
    }
}

