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

int   tpParamInit();

/**************************************************************
 * 主函数 
 *************************************************************/
int main(int argc, char *argv[])
{
    int  ret,sockfd, sockfdacp;
    int  *pInt,peerPort;
    char peerAddr[IPADDRLEN+1];
    int  iTime,iTflag;
    /*检查命令行参数*/
    if (argc != 2) {
        printf("Usage: %s <任务ID>\n", argv[0]);
        return -1;
    }

    /*初始化进程*/
    if (tpTaskInit(argv[1], "tpTcpAS", &m_ptpTask) != 0) {
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
	/*设置空链路间隔时间*/
	iTime=300;/*秒*/
    while (1) {
        if ((sockfdacp = tcp_accept(sockfd, peerAddr, &peerPort)) == -1) {
            LOG_ERR("接收客户端连接请求出错");
		continue;
        }
        LOG_MSG("接受客户端[%s:%d]的连接请求", peerAddr, peerPort);
	if(fork()==0)
	{
       	  while (1){
		 if(ret=tpProcess(sockfdacp)!=0){
			close(sockfdacp); 
			exit(1);	
		}
          }
    	}else
	{
			close(sockfdacp); 
			continue;
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

/***************************************************************
 * 处理函数
 **************************************************************/
int tpProcess(int  sockfdin)
{
    int  ret,msgLen,sockfd;
    char logPrefix[21];
    char clazz[MSGCLZLEN+1];
    time_t t;
    TPMSG tpMsg;
    MQHDL *mqh=NULL;

    sockfd = sockfdin;
 
    /*通讯设置*/
    if (set_sock_linger(sockfd, 1, 1) == -1) {
        LOG_ERR("设置连接关闭逗留时间出错");
        goto THD_END;
    }
    /*接收报文*/
    if ((msgLen = tcp_recv(sockfd, tpMsg.body, 0, m_lenlen, m_timeout)) <=0) {
        LOG_ERR("tcp_recv() error: %s", strerror(errno));
        LOG_ERR("接收客户端报文出错");
	if(msgLen ==0) {
        LOG_ERR("tcp_recv() msgLen=0");
		return 0;
		}
        goto THD_END;
    }
    LOG_MSG("收到客户端发来的报文");
    if (hasLogLevel(DEBUG)) LOG_HEX(tpMsg.body, msgLen);

    /* 初始化报文头 */
    if (tpNewHead(m_ptpTask->bindQ, &tpMsg.head) != 0) {
        LOG_ERR("初始化报文头出错");
        goto THD_END;
    }

    tpSetLogPrefix(tpMsg.head.tpId, logPrefix);

    LOG_MSG("初始化报文头...[ok]");
    if (hasLogLevel(DEBUG)) tpHeadDebug(&tpMsg.head);

    /*绑定队列*/
    if ((ret = mq_bind(m_ptpTask->bindQ, &mqh)) != 0) {
        LOG_ERR("mq_bind() error: retcode=%d", ret);
        LOG_ERR("绑定队列[%d]出错", m_ptpTask->bindQ);
        goto THD_END;
    }

    /*写主控队列*/
    snprintf(clazz, sizeof(clazz), "%d", tpMsg.head.tpId); 
    msgLen = tpMsg.head.bodyLen + sizeof(TPHEAD);
    if ((ret = mq_post(mqh, NULL, Q_SVR, 0, clazz, (char *)&tpMsg, msgLen, 0)) != 0) {
        LOG_ERR("mq_post() error: retcode=%d", ret);
        LOG_ERR("写主控队列[%d]出错", Q_SVR);
        goto THD_END;
    }
    LOG_MSG("写主控队列[%d]...[ok]", Q_SVR);

    /*读绑定队列*/
    msgLen = sizeof(TPMSG);
    if ((ret = mq_pend(mqh, NULL, NULL, NULL, clazz, (char *)&tpMsg, &msgLen, m_timeout)) != 0) {
        LOG_ERR("mq_pend() error: retcode=%d", ret);
        LOG_ERR("读绑定队列[%d]出错", m_ptpTask->bindQ);
        goto THD_END;
    }
    LOG_MSG("收到主控返回报文");
    if (hasLogLevel(DEBUG)) tpHeadDebug(&tpMsg.head);
    if (hasLogLevel(DEBUG)) LOG_HEX(tpMsg.body, tpMsg.head.bodyLen);

THD_END:
    if (mqh) mq_detach(mqh);
	return -1;
}

