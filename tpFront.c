/**************************************************************
 * TCPͨѶͬ�������ӷ���˳���
 *************************************************************/
#include "tpbus.h"

TPTASK *m_ptpTask;

int  m_orgQ;                   /*�ͻ��˶���*/

MQHDL *mqh=NULL;
int  sockfd, m_csock;

int   tpParamInit();
void  tpQuit(int);

/**************************************************************
 * ������ 
 *************************************************************/
int main(int argc, char *argv[])
{
    int  ret;
    int  q;
    int  msgLen;
    char logPrefix[21];
    char clazz[MSGCLZLEN+1];
    TPMSG tpMsg;

    /*��������в���*/
    if (argc != 2) {
        printf("Usage: %s <����ID>\n", argv[0]);
        return -1;
    }

    /*��ʼ������*/
    if (tpTaskInit(argv[1], "tpFront", &m_ptpTask) != 0) {
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
    for (;;){
        msgLen = sizeof(TPMSG);
        if ((ret = mq_pend(mqh, NULL, &q, NULL, NULL, (char *)&tpMsg, &msgLen, 0)) != 0) {
            LOG_ERR("mq_pend() error: retcode=%d", ret);
            tpQuit(-1);
        }

        if (q != Q_SVR){
            /*���տͻ��˱���*/
            LOG_MSG("�յ��ͻ��˷����ı���");
            msgLen -= sizeof(TPHEAD);
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
        } 
        else{
            LOG_MSG("�յ����ط����ı���");
            if (hasLogLevel(DEBUG)) tpHeadDebug(&tpMsg.head);
            if (hasLogLevel(DEBUG)) LOG_HEX(tpMsg.body, tpMsg.head.bodyLen);

            /*д�ͻ��˶���*/
            snprintf(clazz, sizeof(clazz), "%d", tpMsg.head.tpId); 
            msgLen = tpMsg.head.bodyLen + sizeof(TPHEAD);
            if ((ret = mq_post(mqh, NULL, m_orgQ, 0, clazz, (char *)&tpMsg, msgLen, 0)) != 0) {
                LOG_ERR("mq_post() error: retcode=%d", ret);
                LOG_ERR("д�ͻ��˶���[%d]����", m_orgQ);
                continue;
            }
            
            LOG_MSG("���ͱ��ĵ��ͻ��˶���...[ok]");
        }            	        
    }
}

/***************************************************************
 * ��ʼ�����ò���
 **************************************************************/
int tpParamInit()
{
    if (tpGetParamInt(m_ptpTask->params, "orgQ", &m_orgQ) != 0) m_orgQ = 20;
    LOG_MSG("����[orgQ]=[%d]", m_orgQ);
 
    return 0;
}

/******************************************************************************
 * ����: tpQuit
 * ����: �����˳�����
 ******************************************************************************/
void tpQuit(int sig)
{
  signal(SIGTERM, SIG_IGN);

  if (mqh) mq_detach(mqh);
  exit(sig);
}

