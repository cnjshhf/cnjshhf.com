/**************************************************************
 * TCP通讯同步短连接客户端程序
 *************************************************************/
#include "tpbus.h"

TPTASK *m_ptpTask;  /*任务配置*/

char m_bindAddr[IPADDRLEN+1];  /*本地端IP地址*/
char m_peerAddr[IPADDRLEN+1];  /*服务端IP地址*/
int  m_peerPort;               /*服务端侦听端口*/
int  m_lenlen;                 /*通讯头长度*/
int  m_timeout;                /*超时时间*/
int  m_nMaxThds;               /*最大并程数*/
MQHDL *mqh;
int   sockfd;

int   tpParamInit();
void *tpProcessThread(void *arg);
void  tpQuit(int);

/**************************************************************
 * 主函数 
 *************************************************************/
int main(int argc, char *argv[])
{
    int   q,ret,msgLen;
    char  logfname[200];
    char  logPrefix[21];
    TPMSG tpMsg;
   
    
    /*检查命令行参数*/
    if (argc != 2) {
        printf("Usage: %s <任务ID>\n", argv[0]);
        return -1;
    }

    /*初始化进程*/
    sprintf(logfname, "tpTcpALC_CUPD_%s",argv[1]);
    if (tpTaskInit(argv[1], logfname, &m_ptpTask) != 0) {
        printf("初始化进程出错");
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
    	  /*通讯连接*/
        if ((sockfd = tcp_connect(m_bindAddr, m_peerAddr, m_peerPort, 0)) == -1) {
            LOG_ERR("tcp_connect(%s,%d) error", m_peerAddr, m_peerPort);
            LOG_ERR("初始化通讯连接出错");
            if (mqh) mq_detach(mqh);
            return -1;
        }
        LOG_MSG("建立与服务端[%s:%d]的通讯连接...[ok]", m_peerAddr, m_peerPort);
        
        /*通讯设置*/
        if (set_sock_linger(sockfd, 1, 1) == -1) {
            LOG_ERR("设置连接关闭逗留时间出错");
            tpQuit(-1);
        }

        for (;;){        
            msgLen = sizeof(TPMSG);
            if ((ret = mq_pend(mqh, NULL, &q, NULL, NULL, (char *)&tpMsg, &msgLen, 60)) != 0) {
                if (ret == 1010){  /*超时*/
                    ret = tcp_send(sockfd, "0000", 4, 0, 0);
                    continue;
                }
                    
                LOG_ERR("mq_pend() error: retcode=%d", ret);
                tpQuit(-1);
            }
            
            tpSetLogPrefix(tpMsg.head.tpId, logPrefix);

            LOG_MSG("收到主控发来的报文");
            if (hasLogLevel(DEBUG)) tpHeadDebug(&tpMsg.head);
            if (hasLogLevel(DEBUG)) LOG_HEX(tpMsg.body, tpMsg.head.bodyLen);

            /*发送报文*/
            if ((ret = tcp_send(sockfd, tpMsg.body, tpMsg.head.bodyLen, m_lenlen, 0)) == -1) {
                LOG_ERR("向服务端发送报文出错");
                close(sockfd);
                break;
            }
            
             LOG_MSG("发送报文到服务端...[ok]");
        }
    }
}

/***************************************************************
 * 初始化配置参数
 **************************************************************/
int tpParamInit()
{
    if (tpGetParamString(m_ptpTask->params, "bindAddr", m_bindAddr) != 0) strcpy(m_bindAddr, "localhost");
    LOG_MSG("参数[bindAddr]=[%s]", m_bindAddr);

    if (tpGetParamString(m_ptpTask->params, "peerAddr", m_peerAddr) != 0) {
        LOG_ERR("读取参数[peerAddr]出错");
        return -1;
    }
    LOG_MSG("参数[peerAddr]=[%s]", m_peerAddr);

    if (tpGetParamInt(m_ptpTask->params, "peerPort", &m_peerPort) != 0) {
        LOG_ERR("读取参数[peerPort]出错");
        return -1;
    }
    LOG_MSG("参数[peerPort]=[%d]", m_peerPort);
  
    if (tpGetParamInt(m_ptpTask->params, "lenlen", &m_lenlen) != 0) m_lenlen = 4;
    LOG_MSG("参数[lenlen]=[%d]", m_lenlen);

    if (tpGetParamInt(m_ptpTask->params, "timeout", &m_timeout) != 0) m_timeout = 60;
    LOG_MSG("参数[timeout]=[%d]", m_timeout);

    if (tpGetParamInt(m_ptpTask->params, "nMaxThds", &m_nMaxThds) != 0) m_nMaxThds = 5;
    LOG_MSG("参数[nMaxThds]=[%d]", m_nMaxThds);

    return 0;
}

void tpQuit(int sig)
{
	  signal(SIGTERM, SIG_IGN);
    close(sockfd);
    if (mqh) mq_detach(mqh);
    exit(sig);
}

