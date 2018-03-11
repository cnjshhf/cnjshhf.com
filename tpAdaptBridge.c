/**************************************************************
 * ����ת��������
 * ��Դ�����������Ӧ����һ��ͨѶ��· 
 * ������Ϣͣ��ʱ��
 *************************************************************/
#include "queue.h"
#include "tpbus.h"

TPTASK m_tpTask;

int  m_src_q;                  /*��Դ����*/
int  m_src_time;               /*src ->��des,live time*/   /*add by dwxiong 2014-11-2 14:43:20*/
int  m_des_q;                  /*Ŀ�Ķ���*/                /*add by dwxiong 2014-11-2 14:43:20*/
int  m_des_time;               /*src ->��des,live time*/   /*add by dwxiong 2014-11-2 14:43:20*/
int  m_chg_srcQ;               /*src ->��des,replace srcQ*//*add by dwxiong 2014-11-2 14:43:20*/
int  m_nMaxThds;               /*��󲢷���*/

int   tpParamInit();
void *tpProcessThread(void *arg);
int tpSend3(MQHDL *mqh, TPMSG *ptpMsg, int mq, int livetime,int flag);

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
    
    if (tpGetParamInt(m_tpTask.params, "src_time", &m_src_time) != 0) m_src_time = 0;
    LOG_MSG("����MSG (src->des) livetime[src_time]=[%d]", m_src_time);
    
    if (tpGetParamInt(m_tpTask.params, "des_q", &m_des_q) != 0) m_des_q = Q_SVR;
    LOG_MSG("����[des_q]=[%d]", m_des_q);

    if (tpGetParamInt(m_tpTask.params, "des_time", &m_des_time) != 0) m_des_time = 0;
    LOG_MSG("����MSG (des->src) livetime[des_time]=[%d]", m_des_time);
    
    if (tpGetParamInt(m_tpTask.params, "chg_srcQ", &m_chg_srcQ) != 0) m_chg_srcQ = 0;
    LOG_MSG("����chg_srcQ (srcQ->desQ)[chg_srcQ]=[%d]", m_chg_srcQ);
    
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
        if (m_src_q == q ) {
            if( m_chg_srcQ == 1 )           
                tpMsg.head.orgQ = m_tpTask.bindQ;
            /*
            if ((ret = mq_post(mqh, NULL, m_des_q, 0, NULL, (char *)&tpMsg, msgLen, m_src_time)) != 0) {
                LOG_ERR("mq_post() error: retcode=%d", ret);
                LOG_ERR("д����[%d]����",m_des_q);
            }
            */      /* del by dwxiong 2014-11-21 14:48:00*/
                        
            if ((ret = tpSend3(mqh, &tpMsg, m_des_q,m_src_time,1)) != 0) {
                LOG_ERR("tpSend2() error: retcode=%d", ret);
                LOG_ERR("д����[%d]����",m_des_q);
            }
                        
            LOG_MSG("ת������[%d]==>[%d]...[ok]", q,m_des_q);
            continue;        
        }
        else if (m_des_q == q || Q_REV == q ) {
            /* ��ʼ������ͷ */
           
            if( m_chg_srcQ == 1 )           
                tpMsg.head.desQ = m_src_q;
            
            if (hasLogLevel(DEBUG)) tpHeadDebug(&tpMsg.head);

            tpMsg.head.bodyLen = msgLen - sizeof(TPHEAD);
            
            /*
            if ((ret = mq_post(mqh, NULL, m_src_q, 0, NULL, (char *)&tpMsg, msgLen, m_des_time)) != 0) {
                LOG_ERR("mq_post() error: retcode=%d", ret);
                LOG_ERR("д����[%d]����",m_src_q);
            }
            
            */      /* del by dwxiong 2014-11-21 14:48:00*/
            
            if ((ret = tpSend3(mqh, &tpMsg, m_src_q,m_des_time,1) ) != 0) {
                LOG_ERR("tpSend2() error: retcode=%d", ret);
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


/***************************************************************
 * ���ͷ�������(���������ͨѶ����)
 * mod by xmdwen @2014-11-21 08:54:46
 **************************************************************/
int tpSend3(MQHDL *mqh, TPMSG *ptpMsg, int mq, int livetime,int flag)
{
    int   i,ret,msgLen;
    char *rqm,clazz[MSGCLZLEN+1];

    i = (ptpMsg->head.tpId/100000000)%10;
    rqm = (i==0)? NULL : gShm.ptpConf->hosts[i-1].qm;
    clazz[0] = 0;
    if (flag) snprintf(clazz, sizeof(clazz), "%d", ptpMsg->head.tpId);
    msgLen = ptpMsg->head.bodyLen + sizeof(TPHEAD);
    if ((ret = mq_post(mqh, rqm, mq, 0, clazz, (char *)ptpMsg, msgLen, livetime)) != 0) {
        LOG_ERR("mq_post() error: retcode=%d", ret);
        LOG_ERR("д����[%d]����", mq);
        return -1;
    }
    /* LOG_MSG("д���ض���[%d]...[ok]", mq); */
    return 0;
}
