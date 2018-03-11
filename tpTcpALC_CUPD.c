/**************************************************************
 * TCPͨѶͬ�������ӿͻ��˳���
 *************************************************************/
#include "tpbus.h"

TPTASK *m_ptpTask;  /*��������*/

char m_bindAddr[IPADDRLEN+1];  /*���ض�IP��ַ*/
char m_peerAddr[IPADDRLEN+1];  /*�����IP��ַ*/
int  m_peerPort;               /*����������˿�*/
int  m_lenlen;                 /*ͨѶͷ����*/
int  m_timeout;                /*��ʱʱ��*/
int  m_nMaxThds;               /*��󲢳���*/
MQHDL *mqh;
int   sockfd;

int   tpParamInit();
void *tpProcessThread(void *arg);
void  tpQuit(int);

/**************************************************************
 * ������ 
 *************************************************************/
int main(int argc, char *argv[])
{
    int   q,ret,msgLen;
    char  logfname[200];
    char  logPrefix[21];
    TPMSG tpMsg;
   
    
    /*��������в���*/
    if (argc != 2) {
        printf("Usage: %s <����ID>\n", argv[0]);
        return -1;
    }

    /*��ʼ������*/
    sprintf(logfname, "tpTcpALC_CUPD_%s",argv[1]);
    if (tpTaskInit(argv[1], logfname, &m_ptpTask) != 0) {
        printf("��ʼ�����̳���");
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
    
    for (;;){
    	  /*ͨѶ����*/
        if ((sockfd = tcp_connect(m_bindAddr, m_peerAddr, m_peerPort, 0)) == -1) {
            LOG_ERR("tcp_connect(%s,%d) error", m_peerAddr, m_peerPort);
            LOG_ERR("��ʼ��ͨѶ���ӳ���");
            if (mqh) mq_detach(mqh);
            return -1;
        }
        LOG_MSG("����������[%s:%d]��ͨѶ����...[ok]", m_peerAddr, m_peerPort);
        
        /*ͨѶ����*/
        if (set_sock_linger(sockfd, 1, 1) == -1) {
            LOG_ERR("�������ӹرն���ʱ�����");
            tpQuit(-1);
        }

        for (;;){        
            msgLen = sizeof(TPMSG);
            if ((ret = mq_pend(mqh, NULL, &q, NULL, NULL, (char *)&tpMsg, &msgLen, 60)) != 0) {
                if (ret == 1010){  /*��ʱ*/
                    ret = tcp_send(sockfd, "0000", 4, 0, 0);
                    continue;
                }
                    
                LOG_ERR("mq_pend() error: retcode=%d", ret);
                tpQuit(-1);
            }
            
            tpSetLogPrefix(tpMsg.head.tpId, logPrefix);

            LOG_MSG("�յ����ط����ı���");
            if (hasLogLevel(DEBUG)) tpHeadDebug(&tpMsg.head);
            if (hasLogLevel(DEBUG)) LOG_HEX(tpMsg.body, tpMsg.head.bodyLen);

            /*���ͱ���*/
            if ((ret = tcp_send(sockfd, tpMsg.body, tpMsg.head.bodyLen, m_lenlen, 0)) == -1) {
                LOG_ERR("�����˷��ͱ��ĳ���");
                close(sockfd);
                break;
            }
            
             LOG_MSG("���ͱ��ĵ������...[ok]");
        }
    }
}

/***************************************************************
 * ��ʼ�����ò���
 **************************************************************/
int tpParamInit()
{
    if (tpGetParamString(m_ptpTask->params, "bindAddr", m_bindAddr) != 0) strcpy(m_bindAddr, "localhost");
    LOG_MSG("����[bindAddr]=[%s]", m_bindAddr);

    if (tpGetParamString(m_ptpTask->params, "peerAddr", m_peerAddr) != 0) {
        LOG_ERR("��ȡ����[peerAddr]����");
        return -1;
    }
    LOG_MSG("����[peerAddr]=[%s]", m_peerAddr);

    if (tpGetParamInt(m_ptpTask->params, "peerPort", &m_peerPort) != 0) {
        LOG_ERR("��ȡ����[peerPort]����");
        return -1;
    }
    LOG_MSG("����[peerPort]=[%d]", m_peerPort);
  
    if (tpGetParamInt(m_ptpTask->params, "lenlen", &m_lenlen) != 0) m_lenlen = 4;
    LOG_MSG("����[lenlen]=[%d]", m_lenlen);

    if (tpGetParamInt(m_ptpTask->params, "timeout", &m_timeout) != 0) m_timeout = 60;
    LOG_MSG("����[timeout]=[%d]", m_timeout);

    if (tpGetParamInt(m_ptpTask->params, "nMaxThds", &m_nMaxThds) != 0) m_nMaxThds = 5;
    LOG_MSG("����[nMaxThds]=[%d]", m_nMaxThds);

    return 0;
}

void tpQuit(int sig)
{
	  signal(SIGTERM, SIG_IGN);
    close(sockfd);
    if (mqh) mq_detach(mqh);
    exit(sig);
}

