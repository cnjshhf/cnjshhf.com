/**************************************************************
 * ����ת����������������Դ�����������Ӧ����һ��ͨѶ��·ʱ
 *************************************************************/
#include "queue.h"
#include "tpbus.h"

TPTASK m_tpTask;


int  m_src_q;                  /*��Դ����*/
int  m_des_q;                  /*Ŀ�Ķ���*/         /*add by dwxiong 2014-11-2 14:43:20*/
int  m_nMaxThds;               /*��󲢷���*/
int  m_tphead;                 /*�Ƿ��ʼ������ͷ*/ /*add by dwxiong 2015-9-18 15:11:24*/

int   tpParamInit();
void *tpProcessThread(void *arg);

/**************************************************************
 * ������ 
 *************************************************************/
int main(int argc, char *argv[])
{
    int  i,ret;

    /*��������в���*/
    if (argc != 2) {
        printf("Usage: %s <����ID>\n", argv[0]);
        return -1;
    }

    /*��ʼ������*/
    if (tpTaskInit(atoi(argv[1]), "tpAdaptChg", &m_tpTask) != 0) {
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
    
    /*��ʼ�������̳߳�*/
    for (i=0; i<m_nMaxThds; i++) {
        if (tpThreadCreate(tpProcessThread, NULL) != 0) {
            LOG_ERR("���������߳�ʧ��: %s", strerror(errno));
            return -1;
        }
    }
    
    while (1) {
        sleep(100);
    }
}

/***************************************************************
 * ��ʼ�����ò���
 **************************************************************/
int tpParamInit()
{
    if (tpGetParamInt(m_tpTask.params, "src_q", &m_src_q) != 0) m_src_q = Q_SVR;
    LOG_MSG("����[src_q]=[%d]", m_src_q);
    
    if (tpGetParamInt(m_tpTask.params, "des_q", &m_des_q) != 0) m_des_q = Q_SVR;
    LOG_MSG("����[des_q]=[%d]", m_des_q);

    if (tpGetParamInt(m_tpTask.params, "tphead", &m_tphead) != 0) m_tphead = 1;
    LOG_MSG("����[tphead]=[%d]", m_tphead);
    
    if (tpGetParamInt(m_tpTask.params, "nMaxThds", &m_nMaxThds) != 0) m_nMaxThds = 5;
    LOG_MSG("����[nMaxThds]=[%d]", m_nMaxThds);

    return 0;
}

/***************************************************************
 * �����߳�
 **************************************************************/
void *tpProcessThread(void *arg)
{
    int    ret,msgLen;
    int    q;
    TPMSG tpMsg;
    MQHDL  *mqh = NULL;

    /*�󶨶���*/
    if ((ret = mq_bind(m_tpTask.bindQ, &mqh)) != 0) {
        LOG_ERR("mq_bind() error: retcode=%d", ret);
        LOG_ERR("�󶨶���[%d]����", m_tpTask.bindQ);
        tpThreadExit();
        return NULL;        
    }

    while (1) {
        msgLen = sizeof(TPMSG);
        memset(&tpMsg, 0x00, sizeof(tpMsg));
        /*�����������*/
        if ((ret = mq_pend(mqh, NULL, &q, NULL, NULL, (char *)&tpMsg, &msgLen, 0)) != 0) {
            LOG_ERR("mq_pend() error: retcode=%d", ret);
            LOG_ERR("������[%d]����", m_tpTask.bindQ);
            mq_detach(mqh);
            tpThreadExit();
            return NULL;        
        }

        LOG_MSG("�յ����Զ���[%d]�ı���", q);
        if (hasLogLevel(DEBUG)) tpHeadDebug(&tpMsg.head);
        if (hasLogLevel(DEBUG)) LOG_HEX(tpMsg.body, tpMsg.head.bodyLen);

        /* if (m_src_q == q) { */           /*mod by dwxiong 2014-11-8 13:42:34*/
        if (m_src_q == q || Q_REV == q ) {
            if ((ret = mq_post(mqh, NULL, m_des_q, 0, NULL, (char *)&tpMsg, msgLen, 0)) != 0) {
                LOG_ERR("mq_post() error: retcode=%d", ret);
                LOG_ERR("д����[%d]����",m_des_q);
            }
            LOG_MSG("ת������[%d]==>[%d]...[ok]", q,m_des_q);
            continue;        
        }
        else if (m_des_q == q) {
            /* ��ʼ������ͷ */
            if(m_tphead == 1)
            {
                if (tpNewHead(m_tpTask.bindQ, &tpMsg.head) != 0) {
                    LOG_ERR("��ʼ������ͷ����");
                    continue;
                }
                LOG_MSG("��ʼ������ͷ...[ok]");
            }   
            /* ����ʼ������ͷ */
            else
            {
                tpMsg.head.orgQ = m_src_q;
                tpMsg.head.desQ = m_tpTask.bindQ;
                tpMsg.head.msgType = MSGAPP;
            }
                
            if (hasLogLevel(DEBUG)) tpHeadDebug(&tpMsg.head);

            tpMsg.head.bodyLen = msgLen - sizeof(TPHEAD);
            if ((ret = mq_post(mqh, NULL, m_src_q, 0, NULL, (char *)&tpMsg, msgLen, 0)) != 0) {
                LOG_ERR("mq_post() error: retcode=%d", ret);
                LOG_ERR("д����[%d]����",m_src_q);
            }
            LOG_MSG("ת������[%d]==>[%d]...[ok]", q, m_src_q);
            continue;        
        }
        else {
            LOG_ERR("������Դ����[%d]�Ƿ� ",q);
            continue;
        }
    }
}

