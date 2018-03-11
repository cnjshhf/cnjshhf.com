/**************************************************************
 * ����ת�������������ڶ����Դ����ת���ص�����ʱ
 *************************************************************/
#include "queue.h"
#include "tpbus.h"

TPTASK m_tpTask;

int  m_des_q;                   /*Ŀ�����*/
int  m_src_8583;                /*�ַ���8583���Ķ���*/     /*mod by xmdwen 2014-11-4 20:10:40*/
int  m_src_xml;                 /*�ַ���xml ���Ķ���*/     /*mod by xmdwen 2014-11-4 20:10:40*/
int  m_src_nvl;                 /*�ַ���nvl ���Ķ���*/     /*mod by xmdwen 2014-11-4 20:10:40*/
int  m_nMaxThds;                /*��󲢷���*/

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
    if (tpTaskInit(atoi(argv[1]), "tpAdaptDist", &m_tpTask) != 0) {
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
    if (tpGetParamInt(m_tpTask.params, "src_8583", &m_src_8583) != 0) m_src_8583 = Q_SVR;
    LOG_MSG("����[src_8583]=[%d]", m_src_8583);

    if (tpGetParamInt(m_tpTask.params, "src_xml",  &m_src_xml) != 0)  m_src_xml  = Q_SVR;
    LOG_MSG("����[src_xml]=[%d]", m_src_xml);
    
    if (tpGetParamInt(m_tpTask.params, "src_nvl",  &m_src_nvl) != 0)  m_src_nvl  = Q_SVR;
    LOG_MSG("����[src_nvl]=[%d]", m_src_nvl);

    if (tpGetParamInt(m_tpTask.params, "des_q", &m_des_q) != 0) m_des_q = Q_SVR;
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

        /* �������󷽣��Ѿ������س�ʼ���˱���ͷ���ڲ�ͨѶ */
        if (m_src_8583 == q || m_src_xml == q || m_src_nvl == q) {
            if ((ret = mq_post(mqh, NULL, m_des_q, 0, NULL, (char *)&tpMsg, msgLen, 0)) != 0) {
                LOG_ERR("mq_post() error: retcode=%d", ret);
                LOG_ERR("д����[%d]����",m_des_q);
            }
            LOG_MSG("ת������[%d]==>[%d]...[ok]", q,m_des_q);
            continue;        
        }
        else if (m_des_q == q) {
#if 0
            /* ��ʼ������ͷ */
            if (tpNewHead(m_tpTask.bindQ, &tpMsg.head) != 0) {
                LOG_ERR("��ʼ������ͷ����");
                continue;
            }
            LOG_MSG("��ʼ������ͷ...[ok]");
            if (hasLogLevel(DEBUG)) tpHeadDebug(&tpMsg.head);
#endif
            /* ���ݱ��ĸ�ʽ��ѡ��Դ���� BEGIN */
            LOG_MSG("����������:[%.4s]", tpMsg.body);

            if (!strncmp(tpMsg.body, "<?xml", 5)) {
                q = m_src_xml;
            }
            else if (tpMsg.body[0] == 0x29 && tpMsg.body[1] == 0x01) {
                q = m_src_8583;
            }
            else {
                q = m_src_nvl;
            }
            /* ѡ��Դ���н��� END */
            
            tpMsg.head.bodyLen = msgLen;
            if ((ret = mq_post(mqh, NULL, q, 0, NULL, (char *)&tpMsg, msgLen + sizeof(TPHEAD), 0)) != 0) {
                LOG_ERR("mq_post() error: retcode=%d", ret);
                LOG_ERR("д����[%d]����",q);
            }
            LOG_MSG("ת������[%d]==>[%d]...[ok]", m_des_q, q);
            continue;        
        }
        else {
            LOG_ERR("������Դ����[%d]�Ƿ� ",q);
            continue;
        }
    }
}

