/**************************************************************
 * TCP通讯同步短连接服务端程序
 *************************************************************/
#include "tpbus.h"

TPTASK *m_ptpTask;

int  m_orgQ;                   /*客户端队列*/

MQHDL *mqh=NULL;
int  sockfd, m_csock;

int   tpParamInit();
void  tpQuit(int);

/**************************************************************
 * 主函数 
 *************************************************************/
int main(int argc, char *argv[])
{
    int  ret;
    int  q;
    int  msgLen;
    char logPrefix[21];
    char clazz[MSGCLZLEN+1];
    TPMSG tpMsg;

    /*检查命令行参数*/
    if (argc != 2) {
        printf("Usage: %s <任务ID>\n", argv[0]);
        return -1;
    }

    /*初始化进程*/
    if (tpTaskInit(argv[1], "tpFront", &m_ptpTask) != 0) {
        printf("初始化进程出错\n");
        return -1;
    }

    /*初始化配置参数*/
    if (tpParamInit() != 0) {
        LOG_ERR("初始化配置参数出错");
        return -1;
    }
   
    /*设置退出信号*/
    signal(SIGTERM, tpQuit);
  
    /*初始化队列*/
    if ((ret = mq_init()) != 0) {
        LOG_ERR("mq_init() error: retcode=%d", ret);
        LOG_ERR("初始化队列出错");
        return -1;
    }
    
    /*绑定队列*/
    if ((ret = mq_bind(m_ptpTask->bindQ, &mqh)) != 0) {
        LOG_ERR("mq_bind() error: retcode=%d", ret);
        LOG_ERR("绑定队列[%d]出错", m_ptpTask->bindQ);
        return -1;
    }
    for (;;){
        msgLen = sizeof(TPMSG);
        if ((ret = mq_pend(mqh, NULL, &q, NULL, NULL, (char *)&tpMsg, &msgLen, 0)) != 0) {
            LOG_ERR("mq_pend() error: retcode=%d", ret);
            tpQuit(-1);
        }

        if (q != Q_SVR){
            /*接收客户端报文*/
            LOG_MSG("收到客户端发来的报文");
            msgLen -= sizeof(TPHEAD);
            if (hasLogLevel(DEBUG)) LOG_HEX(tpMsg.body, msgLen);
            	
            /* 初始化报文头 */
            if (tpNewHead(m_ptpTask->bindQ, &tpMsg.head) != 0) {
                LOG_ERR("初始化报文头出错");
                continue;
            }
            tpMsg.head.bodyLen = msgLen;
            
            tpSetLogPrefix(tpMsg.head.tpId, logPrefix);
            
            LOG_MSG("初始化报文头...[ok]");
            if (hasLogLevel(DEBUG)) tpHeadDebug(&tpMsg.head);
            
            /*写主控队列*/
            snprintf(clazz, sizeof(clazz), "%d", tpMsg.head.tpId); 
            msgLen = tpMsg.head.bodyLen + sizeof(TPHEAD);
            if ((ret = mq_post(mqh, NULL, Q_SVR, 0, clazz, (char *)&tpMsg, msgLen, 0)) != 0) {
                LOG_ERR("mq_post() error: retcode=%d", ret);
                LOG_ERR("写主控队列[%d]出错", Q_SVR);
                continue;
            }
            LOG_MSG("写主控队列[%d]...[ok]", Q_SVR); 
        } 
        else{
            LOG_MSG("收到主控发来的报文");
            if (hasLogLevel(DEBUG)) tpHeadDebug(&tpMsg.head);
            if (hasLogLevel(DEBUG)) LOG_HEX(tpMsg.body, tpMsg.head.bodyLen);

            /*写客户端队列*/
            snprintf(clazz, sizeof(clazz), "%d", tpMsg.head.tpId); 
            msgLen = tpMsg.head.bodyLen + sizeof(TPHEAD);
            if ((ret = mq_post(mqh, NULL, m_orgQ, 0, clazz, (char *)&tpMsg, msgLen, 0)) != 0) {
                LOG_ERR("mq_post() error: retcode=%d", ret);
                LOG_ERR("写客户端队列[%d]出错", m_orgQ);
                continue;
            }
            
            LOG_MSG("发送报文到客户端队列...[ok]");
        }            	        
    }
}

/***************************************************************
 * 初始化配置参数
 **************************************************************/
int tpParamInit()
{
    if (tpGetParamInt(m_ptpTask->params, "orgQ", &m_orgQ) != 0) m_orgQ = 20;
    LOG_MSG("参数[orgQ]=[%d]", m_orgQ);
 
    return 0;
}

/******************************************************************************
 * 函数: tpQuit
 * 功能: 进程退出函数
 ******************************************************************************/
void tpQuit(int sig)
{
  signal(SIGTERM, SIG_IGN);

  if (mqh) mq_detach(mqh);
  exit(sig);
}

