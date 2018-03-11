/**************************************************************
 * TCPͨѶͬ�������ӿͻ��˳���
 *************************************************************/
#include "queue.h"
#include "tpbus.h"

TPTASK m_tpTask;  /*��������*/

char m_bindAddr[IPADDRLEN+1];  /*���ض�IP��ַ*/
char m_peerAddr[IPADDRLEN+1];  /*�����IP��ַ*/
int  m_peerPort;               /*����������˿�*/
int  m_lenlen;                 /*ͨѶͷ����*/
int  m_timeout;                /*��ʱʱ��*/
int  m_nMaxThds;               /*��󲢳���*/
int  m_sleeptime;
int  m_chg_q;

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
    if (tpTaskInit(atoi(argv[1]), "tpTcpALC", &m_tpTask) != 0) {
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
    if ((ret = mq_bind(m_tpTask.bindQ, &mqh)) != 0) {
        LOG_ERR("mq_bind() error: retcode=%d", ret);
        LOG_ERR("�󶨶���[%d]����", m_tpTask.bindQ);
        return -1;
    }
    
    for (;;){
    	  /*ͨѶ����*/
        if ((sockfd = tcp_connect(m_bindAddr, m_peerAddr, m_peerPort, 0)) == -1) {
            LOG_ERR("tcp_connect(%s,%d) error", m_peerAddr, m_peerPort);
            LOG_ERR("��ʼ��ͨѶ���ӳ���");
            /*
            if (mqh) mq_detach(mqh);
            return -1;*/
            sleep(m_sleeptime);
            continue;
        }
        LOG_MSG("����������[%s:%d]��ͨѶ����...[ok]", m_peerAddr, m_peerPort);
        
        /*ͨѶ����*/
        if (set_sock_linger(sockfd, 1, 1) == -1) {
            LOG_ERR("�������ӹرն���ʱ�����");
            tpQuit(-1);
        }

        for (;;){        
            msgLen = sizeof(TPMSG);
            if ((ret = mq_pend(mqh, NULL, &q, NULL, NULL, (char *)&tpMsg, &msgLen,m_timeout)) != 0) {
                if (ret == 1010){  /*��ʱ*/
                    ret = tcp_send(sockfd, "0000", 4, 0, 0);
                    tpSetLogPrefix(0, logPrefix);           /*add by dwxiong 2014-11-5 14:32:37*/
                    LOG_MSG("send test msg 0000 [%s:%d]...[ok]",m_peerAddr, m_peerPort);
                    if (ret == -1) {
                        LOG_ERR("�����˷��ͱ��ĳ���");
                        close(sockfd);
                        break;
                    }
                    continue;
                }
                    
                LOG_ERR("mq_pend() error: retcode=%d", ret);
                tpQuit(-1);
            }
            
            LOG_MSG("q = %d",q);
            if ( (q != Q_SVR) && (q != m_chg_q) ) continue; /*ֻ�������ر���*/

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
            
             LOG_MSG("���ͱ��ĵ������[%s:%d]...[ok]",m_peerAddr, m_peerPort);
        }
    }
}

/***************************************************************
 * ��ʼ�����ò���
 **************************************************************/
int tpParamInit()
{
    if (tpGetParamString(m_tpTask.params, "bindAddr", m_bindAddr) != 0) strcpy(m_bindAddr, "127.0.0.1");
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

    if (tpGetParamInt(m_tpTask.params, "nMaxThds", &m_nMaxThds) != 0) m_nMaxThds = 5;
    LOG_MSG("����[nMaxThds]=[%d]", m_nMaxThds);

    if (tpGetParamInt(m_tpTask.params, "chg_q", &m_chg_q) != 0) m_chg_q = Q_SVR;
    LOG_MSG("����[chg_q]=[%d]", m_chg_q);

    if (tpGetParamInt(m_tpTask.params, "sleeptime", &m_sleeptime) != 0) m_sleeptime = 60;
    LOG_MSG("����[sleeptime]=[%d]", m_sleeptime);

    return 0;
}

void tpQuit(int sig)
{
	  signal(SIGTERM, SIG_IGN);
    close(sockfd);
    if (mqh) mq_detach(mqh);
    exit(sig);
}

