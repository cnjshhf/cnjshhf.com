/**************************************************************
 * TCP通讯同步短连接服务端程序
 *************************************************************/
#include "tpbus.h"

TPTASK *m_ptpTask;

char m_bindAddr[IPADDRLEN+1];  /*服务端IP地址*/
int  m_bindPort;               /*服务端侦听端口*/
int  m_lenlen;                 /*通讯头长度*/
int  m_timeout;                /*超时时间*/
int  m_nMaxThds;               /*最大并发数*/
MQHDL *mqh=NULL;
int  sockfd, m_csock;

int   tpParamInit();
void *tpProcessThread(void *arg);
void  tpQuit(int);

/**************************************************************
 * 主函数 
 *************************************************************/
int main(int argc, char *argv[])
{
    int  ret;
    int  peerPort;
    char peerAddr[IPADDRLEN+1];
    int  msgLen;
    char logPrefix[21];
    char clazz[MSGCLZLEN+1];
    time_t t;
    TPMSG tpMsg;
    

    /*检查命令行参数*/
    if (argc != 2) {
        printf("Usage: %s <任务ID>\n", argv[0]);
        return -1;
    }

    /*初始化进程*/
    if (tpTaskInit(argv[1], "tpTcpSLS", &m_ptpTask) != 0) {
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

    /*初始化侦听端口*/
    if ((sockfd = tcp_listen(m_bindAddr, m_bindPort)) == -1) {
        LOG_ERR("初始化侦听端口[%s:%d]出错", m_bindAddr, m_bindPort);
        return -1;
    }

    for (;;){
        if ((m_csock = tcp_accept(sockfd, peerAddr, &peerPort)) == -1) {
            LOG_ERR("接收客户端连接请求出错");
            close(sockfd);
            if (mqh) mq_detach(mqh);
            return -1;
        }
        
        LOG_MSG("接受客户端[%s:%d]的连接请求", peerAddr, peerPort);
        
        /*通讯设置*/
        if (set_sock_linger(sockfd, 1, 1) == -1) {
            LOG_ERR("设置连接关闭逗留时间出错");
            close(m_csock);
            close(sockfd);
            if (mqh) mq_detach(mqh);
            return -1;
        }
        
        for (;;){
            /*接收报文*/
            memset(&tpMsg, 0x00, sizeof(tpMsg));
            if ((msgLen = tcp_recv(m_csock, tpMsg.body, 0, m_lenlen, m_timeout)) <= 0) {
                LOG_ERR("tcp_recv() error: %s", strerror(errno));
                LOG_ERR("接收客户端报文出错");
                close(m_csock);
                break;
            }
            LOG_MSG("收到客户端发来的报文");
            
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
            
            /*读绑定队列*/
            msgLen = sizeof(TPMSG);
            if ((ret = mq_pend(mqh, NULL, NULL, NULL, clazz, (char *)&tpMsg, &msgLen, m_timeout)) != 0) {
                LOG_ERR("mq_pend() error: retcode=%d", ret);
                LOG_ERR("读绑定队列[%d]出错", m_ptpTask->bindQ);
                continue;
            }
            LOG_MSG("收到主控返回报文");
            if (hasLogLevel(DEBUG)) tpHeadDebug(&tpMsg.head);
            if (hasLogLevel(DEBUG)) LOG_HEX(tpMsg.body, tpMsg.head.bodyLen);
            
            /*发送报文*/
            if ((ret = tcp_send(sockfd, tpMsg.body, tpMsg.head.bodyLen, m_lenlen, 0)) == -1) {
                LOG_ERR("向客户端发送报文出错");
                close(m_csock);  
                break;
            }
            LOG_MSG("发送报文到客户端...[ok]");
        }  
    }  
}

/***************************************************************
 * 初始化配置参数
 **************************************************************/
int tpParamInit()
{
    if (tpGetParamString(m_ptpTask->params, "bindAddr", m_bindAddr) != 0) {
        LOG_ERR("读取参数[bindAddr]出错");
        return -1;
    }
    LOG_MSG("参数[bindAddr]=[%s]", m_bindAddr);

    if (tpGetParamInt(m_ptpTask->params, "bindPort", &m_bindPort) != 0) {
        LOG_ERR("读取参数[bindPort]出错");
        return -1;
    }
    LOG_MSG("参数[bindPort]=[%d]", m_bindPort);
  
    if (tpGetParamInt(m_ptpTask->params, "lenlen", &m_lenlen) != 0) m_lenlen = 4;
    LOG_MSG("参数[lenlen]=[%d]", m_lenlen);

    if (tpGetParamInt(m_ptpTask->params, "timeout", &m_timeout) != 0) m_timeout = 60;
    LOG_MSG("参数[timeout]=[%d]", m_timeout);

    if (tpGetParamInt(m_ptpTask->params, "nMaxThds", &m_nMaxThds) != 0) m_nMaxThds = 5;
    LOG_MSG("参数[nMaxThds]=[%d]", m_nMaxThds);

    return 0;
}

/******************************************************************************
 * 函数: tpQuit
 * 功能: 进程退出函数
 ******************************************************************************/
void tpQuit(int sig)
{
  signal(SIGTERM, SIG_IGN);
  close(sockfd);
  close(m_csock);
  if (mqh) mq_detach(mqh);
  exit(sig);
}
