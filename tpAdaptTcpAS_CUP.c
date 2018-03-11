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

int   tpParamInit();

/**************************************************************
 * ������ 
 *************************************************************/
int main(int argc, char *argv[])
{
    int  ret,sockfd, sockfdacp;
    int  *pInt,peerPort;
    char peerAddr[IPADDRLEN+1];
    int  iTime,iTflag;
    /*��������в���*/
    if (argc != 2) {
        printf("Usage: %s <����ID>\n", argv[0]);
        return -1;
    }

    /*��ʼ������*/
    if (tpTaskInit(argv[1], "tpTcpAS", &m_ptpTask) != 0) {
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

    /*��ʼ�������˿�*/
    if ((sockfd = tcp_listen(m_bindAddr, m_bindPort)) == -1) {
        LOG_ERR("��ʼ�������˿�[%s:%d]����", m_bindAddr, m_bindPort);
        return -1;
    }
	/*���ÿ���·���ʱ��*/
	iTime=300;/*��*/
    while (1) {
        if ((sockfdacp = tcp_accept(sockfd, peerAddr, &peerPort)) == -1) {
            LOG_ERR("���տͻ��������������");
		continue;
        }
        LOG_MSG("���ܿͻ���[%s:%d]����������", peerAddr, peerPort);
	if(fork()==0)
	{
       	  while (1){
		 if(ret=tpProcess(sockfdacp)!=0){
			close(sockfdacp); 
			exit(1);	
		}
          }
    	}else
	{
			close(sockfdacp); 
			continue;
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

/***************************************************************
 * ������
 **************************************************************/
int tpProcess(int  sockfdin)
{
    int  ret,msgLen,sockfd;
    char logPrefix[21];
    char clazz[MSGCLZLEN+1];
    time_t t;
    TPMSG tpMsg;
    MQHDL *mqh=NULL;

    sockfd = sockfdin;
 
    /*ͨѶ����*/
    if (set_sock_linger(sockfd, 1, 1) == -1) {
        LOG_ERR("�������ӹرն���ʱ�����");
        goto THD_END;
    }
    /*���ձ���*/
    if ((msgLen = tcp_recv(sockfd, tpMsg.body, 0, m_lenlen, m_timeout)) <=0) {
        LOG_ERR("tcp_recv() error: %s", strerror(errno));
        LOG_ERR("���տͻ��˱��ĳ���");
	if(msgLen ==0) {
        LOG_ERR("tcp_recv() msgLen=0");
		return 0;
		}
        goto THD_END;
    }
    LOG_MSG("�յ��ͻ��˷����ı���");
    if (hasLogLevel(DEBUG)) LOG_HEX(tpMsg.body, msgLen);

    /* ��ʼ������ͷ */
    if (tpNewHead(m_ptpTask->bindQ, &tpMsg.head) != 0) {
        LOG_ERR("��ʼ������ͷ����");
        goto THD_END;
    }

    tpSetLogPrefix(tpMsg.head.tpId, logPrefix);

    LOG_MSG("��ʼ������ͷ...[ok]");
    if (hasLogLevel(DEBUG)) tpHeadDebug(&tpMsg.head);

    /*�󶨶���*/
    if ((ret = mq_bind(m_ptpTask->bindQ, &mqh)) != 0) {
        LOG_ERR("mq_bind() error: retcode=%d", ret);
        LOG_ERR("�󶨶���[%d]����", m_ptpTask->bindQ);
        goto THD_END;
    }

    /*д���ض���*/
    snprintf(clazz, sizeof(clazz), "%d", tpMsg.head.tpId); 
    msgLen = tpMsg.head.bodyLen + sizeof(TPHEAD);
    if ((ret = mq_post(mqh, NULL, Q_SVR, 0, clazz, (char *)&tpMsg, msgLen, 0)) != 0) {
        LOG_ERR("mq_post() error: retcode=%d", ret);
        LOG_ERR("д���ض���[%d]����", Q_SVR);
        goto THD_END;
    }
    LOG_MSG("д���ض���[%d]...[ok]", Q_SVR);

    /*���󶨶���*/
    msgLen = sizeof(TPMSG);
    if ((ret = mq_pend(mqh, NULL, NULL, NULL, clazz, (char *)&tpMsg, &msgLen, m_timeout)) != 0) {
        LOG_ERR("mq_pend() error: retcode=%d", ret);
        LOG_ERR("���󶨶���[%d]����", m_ptpTask->bindQ);
        goto THD_END;
    }
    LOG_MSG("�յ����ط��ر���");
    if (hasLogLevel(DEBUG)) tpHeadDebug(&tpMsg.head);
    if (hasLogLevel(DEBUG)) LOG_HEX(tpMsg.body, tpMsg.head.bodyLen);

THD_END:
    if (mqh) mq_detach(mqh);
	return -1;
}

