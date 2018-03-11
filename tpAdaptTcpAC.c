/**************************************************************
 * TCPͨѶ�첽���ӿͻ��˳���
 *************************************************************/
#include "tpbus.h"

TPTASK *m_ptpTask;  /*��������*/

char m_bindAddr[IPADDRLEN+1];  /*���ض�IP��ַ*/
char m_peerAddr[IPADDRLEN+1];  /*�����IP��ַ*/
int  m_peerPort;               /*����������˿�*/
int  m_lenlen;                 /*ͨѶͷ����*/
int  m_timeout;                /*��ʱʱ��*/
int  m_nMaxThds;               /*��󲢳���*/

int   tpParamInit();

/**************************************************************
 * ������ 
 *************************************************************/
int main(int argc, char *argv[])
{
    int q,ret,msgLen;
    MQHDL *mqh;
    int sockfdout;
    TPMSG *ptpMsg;

    /*��������в���*/
    if (argc != 2) {
        printf("Usage: %s <����ID>\n", argv[0]);
        return -1;
    }
    /*��ʼ������*/
    if (tpTaskInit(argv[1], argv[0], &m_ptpTask) != 0) {
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

    /*�󶨶���*/
    if ((ret = mq_bind(m_ptpTask->bindQ, &mqh)) != 0) {
        LOG_ERR("mq_bind() error: retcode=%d", ret);
        LOG_ERR("�󶨶���[%d]����", m_ptpTask->bindQ);
        return -1;
    }
    
        if ((ptpMsg = (TPMSG *)malloc(sizeof(TPMSG))) == NULL) {
            LOG_ERR("malloc() error: %s", strerror(errno));
            return -1;
        }
    /*ͨѶ����*/
	while(1){
        LOG_ERR("tcp_connect(%s,%d)1 ", m_peerAddr, m_peerPort);
    if ((sockfdout = tcp_connect(m_bindAddr, m_peerAddr, m_peerPort, 0)) == -1) {
        LOG_ERR("tcp_connect(%s,%d)2 ", m_peerAddr, m_peerPort);
        LOG_ERR("��ʼ��ͨѶ���ӳ���");
	sleep(5);
	continue;
    }
    LOG_MSG("����������[%s:%d]��ͨѶ����...[ok]", m_peerAddr, m_peerPort);
	ret=0;
    while (ret==0) {
        
        msgLen = sizeof(TPMSG);
        if ((ret = mq_pend(mqh, NULL, &q, NULL, NULL, (char *)ptpMsg, &msgLen, m_timeout)) != 0) {
            LOG_ERR("mq_pend() error: retcode=%d", ret);
		if(ret==1010) {
    		if ((ret = tcp_send(sockfdout, "0000", 0, 4, 10)) == -1) {
		LOG_ERR("send err");
		break;
		}
		LOG_ERR("send 0000");
		continue;
	}
            return -1;
     }
        if (q != Q_SVR) continue; /*ֻ�������ر���*/
	if(ret=tpProcess(ptpMsg,sockfdout)!=0 ){
            LOG_ERR("tpProocess() error: retcode=%d", ret);
    	}
   
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

    if (tpGetParamInt(m_ptpTask->params, "peerPort", &m_peerPort) != 0) {
        LOG_ERR("��ȡ����[peerPort]����");
        return -1;
    }
    LOG_MSG("����[peerPort]=[%d]", m_peerPort);
  
    if (tpGetParamInt(m_ptpTask->params, "lenlen", &m_lenlen) != 0) m_lenlen = 4;

    if (tpGetParamInt(m_ptpTask->params, "timeout", &m_timeout) != 0) m_timeout = 60;
    LOG_MSG("����[timeout]=[%d]", m_timeout);

    if (tpGetParamInt(m_ptpTask->params, "nMaxThds", &m_nMaxThds) != 0) m_nMaxThds = 5;
    LOG_MSG("����[nMaxThds]=[%d]", m_nMaxThds);

    return 0;
}

/***************************************************************
 * �����߳�
 **************************************************************/
int tpProcess( TPMSG *ptpMsg,int sockfd )
{
    int ret,msgLen;
    char logPrefix[21];
    MQHDL *mqh = NULL;
/*
    TPMSG *ptpMsg = (TPMSG *)arg;
*/
    tpSetLogPrefix(ptpMsg->head.tpId, logPrefix);

    LOG_MSG("�յ����ط����ı���");
    if (hasLogLevel(DEBUG)) tpHeadDebug(&ptpMsg->head);
    if (hasLogLevel(DEBUG)) LOG_HEX(ptpMsg->body, ptpMsg->head.bodyLen);

    /*���ͱ���*/
    if ((ret = tcp_send(sockfd, ptpMsg->body, ptpMsg->head.bodyLen, m_lenlen, 0)) == -1) {
        LOG_ERR("�����˷��ͱ��ĳ���");
        return -2;;
    }
    LOG_MSG("���ͱ��ĵ������...[ok]");

	return 0;
    /*���ձ���*/
    if ((ptpMsg->head.bodyLen = tcp_recv(sockfd, ptpMsg->body, 0, m_lenlen, m_timeout)) == -1) {
        LOG_ERR("tcp_recv() error: %s", strerror(errno));
        LOG_ERR("���շ���˱��ĳ���");
        goto THD_END;
    }
    LOG_MSG("�յ�����˷��ر���");
    if (hasLogLevel(DEBUG)) LOG_HEX(ptpMsg->body, ptpMsg->head.bodyLen);

    /*�󶨶���*/
    if ((ret = mq_bind(m_ptpTask->bindQ, &mqh)) != 0) {
        LOG_ERR("mq_bind() error: retcode=%d", ret);
        LOG_ERR("�󶨶���[%d]����", m_ptpTask->bindQ);
        goto THD_END;
    }

    /*д���ض���*/
    msgLen = ptpMsg->head.bodyLen + sizeof(TPHEAD);
    if ((ret = mq_post(mqh, NULL, Q_SVR, 0, NULL, (char *)ptpMsg, msgLen, 0)) != 0) {
        LOG_ERR("mq_post() error: retcode=%d", ret);
        LOG_ERR("д���ض���[%d]����", Q_SVR);
        goto THD_END;
    }
    LOG_MSG("д���ض���[%d]...[ok]", Q_SVR);
	return 0;
THD_END:
    if (mqh) mq_detach(mqh);
	return -1;
}

