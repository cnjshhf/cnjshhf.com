/**************************************************************
 * TCPͨѶͬ�������ӷ���˳���
 *************************************************************/
#include "tpbus.h"

TPTASK *m_ptpTask;

char m_bindAddr[IPADDRLEN+1];  /*�����IP��ַ*/
int  m_bindPort;               /*����������˿�*/
int  m_lenlen;                 /*ͨѶͷ����*/
int  m_timeout;                /*��ʱʱ��*/
int  m_nMaxThds;               /*��󲢷���*/
MQHDL *mqh=NULL;
int  sockfd, m_csock;

int   tpParamInit();
void *tpProcessThread(void *arg);
void  tpQuit(int);

/**************************************************************
 * ������ 
 *************************************************************/
int main(int argc, char *argv[])
{
    int  ret;
    int  peerPort;
    char peerAddr[IPADDRLEN+1];
    int  msgLen;
    char logPrefix[21];
    char clazz[MSGCLZLEN+1];
    time_t t;
    TPMSG tpMsg;
    

    /*��������в���*/
    if (argc != 2) {
        printf("Usage: %s <����ID>\n", argv[0]);
        return -1;
    }

    /*��ʼ������*/
    if (tpTaskInit(argv[1], "tpTcpSLS", &m_ptpTask) != 0) {
        printf("��ʼ�����̳���\n");
        return -1;
    }

    /*��ʼ�����ò���*/
    if (tpParamInit() != 0) {
        LOG_ERR("��ʼ�����ò�������");
        return -1;
    }
   
    /*�����˳��ź�*/
    signal(SIGTERM, tpQuit);
  
    /*��ʼ������*/
    if ((ret = mq_init()) != 0) {
        LOG_ERR("mq_init() error: retcode=%d", ret);
        LOG_ERR("��ʼ�����г���");
        return -1;
    }
    
    /*�󶨶���*/
    if ((ret = mq_bind(m_ptpTask->bindQ, &mqh)) != 0) {
        LOG_ERR("mq_bind() error: retcode=%d", ret);
        LOG_ERR("�󶨶���[%d]����", m_ptpTask->bindQ);
        return -1;
    }

    /*��ʼ�������˿�*/
    if ((sockfd = tcp_listen(m_bindAddr, m_bindPort)) == -1) {
        LOG_ERR("��ʼ�������˿�[%s:%d]����", m_bindAddr, m_bindPort);
        return -1;
    }

    for (;;){
        if ((m_csock = tcp_accept(sockfd, peerAddr, &peerPort)) == -1) {
            LOG_ERR("���տͻ��������������");
            close(sockfd);
            if (mqh) mq_detach(mqh);
            return -1;
        }
        
        LOG_MSG("���ܿͻ���[%s:%d]����������", peerAddr, peerPort);
        
        /*ͨѶ����*/
        if (set_sock_linger(sockfd, 1, 1) == -1) {
            LOG_ERR("�������ӹرն���ʱ�����");
            close(m_csock);
            close(sockfd);
            if (mqh) mq_detach(mqh);
            return -1;
        }
        
        for (;;){
            /*���ձ���*/
            memset(&tpMsg, 0x00, sizeof(tpMsg));
            if ((msgLen = tcp_recv(m_csock, tpMsg.body, 0, m_lenlen, m_timeout)) <= 0) {
                LOG_ERR("tcp_recv() error: %s", strerror(errno));
                LOG_ERR("���տͻ��˱��ĳ���");
                close(m_csock);
                break;
            }
            LOG_MSG("�յ��ͻ��˷����ı���");
            
            if (hasLogLevel(DEBUG)) LOG_HEX(tpMsg.body, msgLen);
            
            /* ��ʼ������ͷ */
            if (tpNewHead(m_ptpTask->bindQ, &tpMsg.head) != 0) {
                LOG_ERR("��ʼ������ͷ����");
                continue;
            }
            tpMsg.head.bodyLen = msgLen;
            
            tpSetLogPrefix(tpMsg.head.tpId, logPrefix);
            
            LOG_MSG("��ʼ������ͷ...[ok]");
            if (hasLogLevel(DEBUG)) tpHeadDebug(&tpMsg.head);
            
            /*д���ض���*/
            snprintf(clazz, sizeof(clazz), "%d", tpMsg.head.tpId); 
            msgLen = tpMsg.head.bodyLen + sizeof(TPHEAD);
            if ((ret = mq_post(mqh, NULL, Q_SVR, 0, clazz, (char *)&tpMsg, msgLen, 0)) != 0) {
                LOG_ERR("mq_post() error: retcode=%d", ret);
                LOG_ERR("д���ض���[%d]����", Q_SVR);
                continue;
            }
            LOG_MSG("д���ض���[%d]...[ok]", Q_SVR);
            
            /*���󶨶���*/
            msgLen = sizeof(TPMSG);
            if ((ret = mq_pend(mqh, NULL, NULL, NULL, clazz, (char *)&tpMsg, &msgLen, m_timeout)) != 0) {
                LOG_ERR("mq_pend() error: retcode=%d", ret);
                LOG_ERR("���󶨶���[%d]����", m_ptpTask->bindQ);
                continue;
            }
            LOG_MSG("�յ����ط��ر���");
            if (hasLogLevel(DEBUG)) tpHeadDebug(&tpMsg.head);
            if (hasLogLevel(DEBUG)) LOG_HEX(tpMsg.body, tpMsg.head.bodyLen);
            
            /*���ͱ���*/
            if ((ret = tcp_send(sockfd, tpMsg.body, tpMsg.head.bodyLen, m_lenlen, 0)) == -1) {
                LOG_ERR("��ͻ��˷��ͱ��ĳ���");
                close(m_csock);  
                break;
            }
            LOG_MSG("���ͱ��ĵ��ͻ���...[ok]");
        }  
    }  
}

/***************************************************************
 * ��ʼ�����ò���
 **************************************************************/
int tpParamInit()
{
    if (tpGetParamString(m_ptpTask->params, "bindAddr", m_bindAddr) != 0) {
        LOG_ERR("��ȡ����[bindAddr]����");
        return -1;
    }
    LOG_MSG("����[bindAddr]=[%s]", m_bindAddr);

    if (tpGetParamInt(m_ptpTask->params, "bindPort", &m_bindPort) != 0) {
        LOG_ERR("��ȡ����[bindPort]����");
        return -1;
    }
    LOG_MSG("����[bindPort]=[%d]", m_bindPort);
  
    if (tpGetParamInt(m_ptpTask->params, "lenlen", &m_lenlen) != 0) m_lenlen = 4;
    LOG_MSG("����[lenlen]=[%d]", m_lenlen);

    if (tpGetParamInt(m_ptpTask->params, "timeout", &m_timeout) != 0) m_timeout = 60;
    LOG_MSG("����[timeout]=[%d]", m_timeout);

    if (tpGetParamInt(m_ptpTask->params, "nMaxThds", &m_nMaxThds) != 0) m_nMaxThds = 5;
    LOG_MSG("����[nMaxThds]=[%d]", m_nMaxThds);

    return 0;
}

/******************************************************************************
 * ����: tpQuit
 * ����: �����˳�����
 ******************************************************************************/
void tpQuit(int sig)
{
  signal(SIGTERM, SIG_IGN);
  close(sockfd);
  close(m_csock);
  if (mqh) mq_detach(mqh);
  exit(sig);
}
