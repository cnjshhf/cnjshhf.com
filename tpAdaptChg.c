/**************************************************************
 * 报文转发适配器，用于来源报文请求和响应公用一个通讯链路时
 *************************************************************/
#include "queue.h"
#include "tpbus.h"

TPTASK m_tpTask;


int  m_src_q;                  /*来源队列*/
int  m_des_q;                  /*目的队列*/         /*add by dwxiong 2014-11-2 14:43:20*/
int  m_nMaxThds;               /*最大并发数*/
int  m_tphead;                 /*是否初始化报文头*/ /*add by dwxiong 2015-9-18 15:11:24*/

int   tpParamInit();
void *tpProcessThread(void *arg);

/**************************************************************
 * 主函数 
 *************************************************************/
int main(int argc, char *argv[])
{
    int  i,ret;

    /*检查命令行参数*/
    if (argc != 2) {
        printf("Usage: %s <任务ID>\n", argv[0]);
        return -1;
    }

    /*初始化进程*/
    if (tpTaskInit(atoi(argv[1]), "tpAdaptChg", &m_tpTask) != 0) {
        printf("初始化进程出错\n");
        return -1;
    }
    
    /*初始化配置参数*/
    if (tpParamInit() != 0) {
        LOG_ERR("初始化配置参数出错");
        return -1;
    }

    /*初始化队列*/
    if ((ret = mq_init()) != 0) {
        LOG_ERR("mq_init() error: retcode=%d", ret);
        LOG_ERR("初始化队列出错");
        return -1;
    }
    
    /*初始化工作线程池*/
    for (i=0; i<m_nMaxThds; i++) {
        if (tpThreadCreate(tpProcessThread, NULL) != 0) {
            LOG_ERR("创建处理线程失败: %s", strerror(errno));
            return -1;
        }
    }
    
    while (1) {
        sleep(100);
    }
}

/***************************************************************
 * 初始化配置参数
 **************************************************************/
int tpParamInit()
{
    if (tpGetParamInt(m_tpTask.params, "src_q", &m_src_q) != 0) m_src_q = Q_SVR;
    LOG_MSG("参数[src_q]=[%d]", m_src_q);
    
    if (tpGetParamInt(m_tpTask.params, "des_q", &m_des_q) != 0) m_des_q = Q_SVR;
    LOG_MSG("参数[des_q]=[%d]", m_des_q);

    if (tpGetParamInt(m_tpTask.params, "tphead", &m_tphead) != 0) m_tphead = 1;
    LOG_MSG("参数[tphead]=[%d]", m_tphead);
    
    if (tpGetParamInt(m_tpTask.params, "nMaxThds", &m_nMaxThds) != 0) m_nMaxThds = 5;
    LOG_MSG("参数[nMaxThds]=[%d]", m_nMaxThds);

    return 0;
}

/***************************************************************
 * 处理线程
 **************************************************************/
void *tpProcessThread(void *arg)
{
    int    ret,msgLen;
    int    q;
    TPMSG tpMsg;
    MQHDL  *mqh = NULL;

    /*绑定队列*/
    if ((ret = mq_bind(m_tpTask.bindQ, &mqh)) != 0) {
        LOG_ERR("mq_bind() error: retcode=%d", ret);
        LOG_ERR("绑定队列[%d]出错", m_tpTask.bindQ);
        tpThreadExit();
        return NULL;        
    }

    while (1) {
        msgLen = sizeof(TPMSG);
        memset(&tpMsg, 0x00, sizeof(tpMsg));
        /*读待处理队列*/
        if ((ret = mq_pend(mqh, NULL, &q, NULL, NULL, (char *)&tpMsg, &msgLen, 0)) != 0) {
            LOG_ERR("mq_pend() error: retcode=%d", ret);
            LOG_ERR("读队列[%d]出错", m_tpTask.bindQ);
            mq_detach(mqh);
            tpThreadExit();
            return NULL;        
        }

        LOG_MSG("收到来自队列[%d]的报文", q);
        if (hasLogLevel(DEBUG)) tpHeadDebug(&tpMsg.head);
        if (hasLogLevel(DEBUG)) LOG_HEX(tpMsg.body, tpMsg.head.bodyLen);

        /* if (m_src_q == q) { */           /*mod by dwxiong 2014-11-8 13:42:34*/
        if (m_src_q == q || Q_REV == q ) {
            if ((ret = mq_post(mqh, NULL, m_des_q, 0, NULL, (char *)&tpMsg, msgLen, 0)) != 0) {
                LOG_ERR("mq_post() error: retcode=%d", ret);
                LOG_ERR("写队列[%d]出错",m_des_q);
            }
            LOG_MSG("转发报文[%d]==>[%d]...[ok]", q,m_des_q);
            continue;        
        }
        else if (m_des_q == q) {
            /* 初始化报文头 */
            if(m_tphead == 1)
            {
                if (tpNewHead(m_tpTask.bindQ, &tpMsg.head) != 0) {
                    LOG_ERR("初始化报文头出错");
                    continue;
                }
                LOG_MSG("初始化报文头...[ok]");
            }   
            /* 不初始化报文头 */
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
                LOG_ERR("写队列[%d]出错",m_src_q);
            }
            LOG_MSG("转发报文[%d]==>[%d]...[ok]", q, m_src_q);
            continue;        
        }
        else {
            LOG_ERR("报文来源队列[%d]非法 ",q);
            continue;
        }
    }
}

