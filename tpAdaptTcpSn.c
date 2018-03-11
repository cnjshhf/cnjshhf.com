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
int  m_TPS;

int   tpParamInit();
void *tpProcessThread(void *arg);

/**************************************************************
 * 主函数 
 *************************************************************/
int main(int argc, char *argv[])
{
    int  ret,sockfd;
    int  *pInt,peerPort;
    char peerAddr[IPADDRLEN+1];

    /*检查命令行参数*/
    if (argc != 2) {
        printf("Usage: %s <任务ID>\n", argv[0]);
        return -1;
    }

    /*初始化进程*/
    if (tpTaskInit(argv[1], "tpTcpS", &m_ptpTask) != 0) {
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

    /*初始化侦听端口*/
    if ((sockfd = tcp_listen(m_bindAddr, m_bindPort)) == -1) {
        LOG_ERR("初始化侦听端口[%s:%d]出错", m_bindAddr, m_bindPort);
        return -1;
    }

    /*设置最大并发数*/
    tpThreadSetMax(m_nMaxThds);

    while (1) {
        if ((pInt = (int *)malloc(sizeof(int))) == NULL) {
            LOG_ERR("malloc() error: %s", strerror(errno));
            return -1;
        }

        if ((*pInt = tcp_accept(sockfd, peerAddr, &peerPort)) == -1) {
            LOG_ERR("接收客户端连接请求出错");
            return -1;
        }
        LOG_MSG("接受客户端[%s:%d]的连接请求", peerAddr, peerPort);

        while (tpThreadChkMax()) usleep(1000); /*1ms*/
        if (tpThreadCreate(tpProcessThread, pInt) != 0) {
            LOG_ERR("创建处理线程失败: %s", strerror(errno));
            return -1;
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

    if (tpGetParamInt(m_ptpTask->params, "nTPS", &m_TPS) != 0) m_TPS = 5;

    return 0;
}

/***************************************************************
 * 处理线程
 **************************************************************/
void *tpProcessThread(void *arg)
{
    int  ret,msgLen,sockfd;
    char logPrefix[11];
    TPMSG tpMsg;
    MQHDL *mqh=NULL;

    sockfd = *((int *)arg);
 
    /*通讯设置*/
    if (set_sock_linger(sockfd, 1, 1) == -1) {
        LOG_ERR("设置连接关闭逗留时间出错");
        goto THD_END;
    }

    /*接收报文*/
    if ((msgLen = tcp_recv(sockfd, tpMsg.body, 0, m_lenlen, m_timeout)) <= 0) {
        LOG_ERR("tcp_recv() error: %s", strerror(errno));
        LOG_ERR("接收客户端报文出错");
        goto THD_END;
    }
    LOG_MSG("收到客户端发来的报文");
    if (hasLogLevel(DEBUG)) LOG_HEX(tpMsg.body, msgLen);

    /* 初始化报文头 */
    if (tpNewHead(m_ptpTask->bindQ, &tpMsg.head) != 0) {
        LOG_ERR("初始化报文头出错");
        goto THD_END;
    }
    tpMsg.head.bodyLen = msgLen;

    tpSetLogPrefix(tpMsg.head.tpId, logPrefix);

    LOG_MSG("初始化报文头...[ok]");
    if (hasLogLevel(DEBUG)) tpHeadDebug(&tpMsg.head);

    /*绑定队列*/
    if ((ret = mq_bind(m_ptpTask->bindQ, &mqh)) != 0) {
        LOG_ERR("mq_bind() error: retcode=%d", ret);
        LOG_ERR("绑定队列[%d]出错", m_ptpTask->bindQ);
        goto THD_END;
    }
/*
	if (tpCall(mqh, &tpMsg, m_timeout) != 0) {
        LOG_ERR("tpCall() error");
        goto THD_END;
	}
*/
    while(1) {
        tpNewHead(m_ptpTask->bindQ, &tpMsg.head);
        tpMsg.head.bodyLen = msgLen; 
	if (tpSend(mqh, &tpMsg) != 0) {
            LOG_ERR("tpCall() error");
            goto THD_END;
	}
        usleep(1000000/m_TPS);
    }

    if (hasLogLevel(DEBUG)) tpHeadDebug(&tpMsg.head);
    if (hasLogLevel(DEBUG)) LOG_HEX(tpMsg.body, tpMsg.head.bodyLen);

    /*发送报文*/
    if ((ret = tcp_send(sockfd, tpMsg.body, tpMsg.head.bodyLen, m_lenlen, 0)) == -1) {
        LOG_ERR("向客户端发送报文出错");
        goto THD_END;
    }
    LOG_MSG("发送报文到客户端...[ok]");

THD_END:
    close(sockfd);
    if (mqh) mq_detach(mqh);
    free(arg);
    tpThreadExit();
    return NULL;
}

