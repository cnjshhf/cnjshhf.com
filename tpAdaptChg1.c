/**************************************************************
 * ����ת����������������Դ�����������Ӧ����һ��ͨѶ��·ʱ
 *************************************************************/
#include "queue.h"
#include "tpbus.h"

TPTASK m_tpTask;

int  m_src_q;                  /*��Դ����*/
int  m_des_q;                  /*����*/
int  m_nMaxThds;               /*��󲢷���*/

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

    if (tpGetParamInt(m_tpTask.params, "des_q", &m_des_q) != 0) m_src_q = Q_SVR;
    LOG_MSG("����[des_q]=[%d]", m_des_q);

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

        if (Q_SVR == q) {
            if ((ret = mq_post(mqh, NULL, m_des_q, 0, NULL, (char *)&tpMsg, msgLen, 0)) != 0) {
                LOG_ERR("mq_post() error: retcode=%d", ret);
                LOG_ERR("д����[%d]����",m_des_q);
            }
            LOG_MSG("ת������[%d]==>[%d]...[ok]", Q_SVR,m_des_q);
            continue;        
        }
        else if (m_src_q == q) {
            /* ��ʼ������ͷ */
            if (tpNewHead(m_tpTask.bindQ, &tpMsg.head) != 0) {
                LOG_ERR("��ʼ������ͷ����");
                continue;
            }
            LOG_MSG("��ʼ������ͷ...[ok]");
            if (hasLogLevel(DEBUG)) tpHeadDebug(&tpMsg.head);

            tpMsg.head.bodyLen = msgLen;
            if ((ret = mq_post(mqh, NULL, Q_SVR, 0, NULL, (char *)&tpMsg, msgLen + sizeof(TPHEAD), 0)) != 0) {
                LOG_ERR("mq_post() error: retcode=%d", ret);
                LOG_ERR("д����[%d]����",Q_SVR);
            }
            LOG_MSG("ת������[%d]==>[%d]...[ok]", m_src_q, Q_SVR);
            continue;        
        }
        else {
            LOG_ERR("������Դ����[%d]�Ƿ� ",q);
            continue;
        }
    }
}

