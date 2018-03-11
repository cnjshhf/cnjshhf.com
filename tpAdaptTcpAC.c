/**************************************************************
 * TCP通讯异步连接客户端程序
 *************************************************************/
#include "tpbus.h"

TPTASK *m_ptpTask;  /*任务配置*/

char m_bindAddr[IPADDRLEN+1];  /*本地端IP地址*/
char m_peerAddr[IPADDRLEN+1];  /*服务端IP地址*/
int  m_peerPort;               /*服务端侦听端口*/
int  m_lenlen;                 /*通讯头长度*/
int  m_timeout;                /*超时时间*/
int  m_nMaxThds;               /*最大并程数*/

int   tpParamInit();

/**************************************************************
 * 主函数 
 *************************************************************/
int main(int argc, char *argv[])
{
    int q,ret,msgLen;
    MQHDL *mqh;
    int sockfdout;
    TPMSG *ptpMsg;

    /*检查命令行参数*/
    if (argc != 2) {
        printf("Usage: %s <任务ID>\n", argv[0]);
        return -1;
    }
    /*初始化进程*/
    if (tpTaskInit(argv[1], argv[0], &m_ptpTask) != 0) {
        printf("初始化进程出错");
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

    /*绑定队列*/
    if ((ret = mq_bind(m_ptpTask->bindQ, &mqh)) != 0) {
        LOG_ERR("mq_bind() error: retcode=%d", ret);
        LOG_ERR("绑定队列[%d]出错", m_ptpTask->bindQ);
        return -1;
    }
    
        if ((ptpMsg = (TPMSG *)malloc(sizeof(TPMSG))) == NULL) {
            LOG_ERR("malloc() error: %s", strerror(errno));
            return -1;
        }
    /*通讯连接*/
	while(1){
        LOG_ERR("tcp_connect(%s,%d)1 ", m_peerAddr, m_peerPort);
    if ((sockfdout = tcp_connect(m_bindAddr, m_peerAddr, m_peerPort, 0)) == -1) {
        LOG_ERR("tcp_connect(%s,%d)2 ", m_peerAddr, m_peerPort);
        LOG_ERR("初始化通讯连接出错");
	sleep(5);
	continue;
    }
    LOG_MSG("建立与服务端[%s:%d]的通讯连接...[ok]", m_peerAddr, m_peerPort);
	ret=0;
    while (ret==0) {
        
        msgLen = sizeof(TPMSG);
        if ((ret = mq_pend(mqh, NULL, &q, NULL, NULL, (char *)ptpMsg, &msgLen, m_timeout)) != 0) {
            LOG_ERR("mq_pend() error: retcode=%d", ret);
		if(ret==1010) {
    		if ((ret = tcp_send(sockfdout, "0000", 0, 4, 10)) == -1) {
		LOG_ERR("send err");
		break;
		}
		LOG_ERR("send 0000");
		continue;
	}
            return -1;
     }
        if (q != Q_SVR) continue; /*只接收主控报文*/
	if(ret=tpProcess(ptpMsg,sockfdout)!=0 ){
            LOG_ERR("tpProocess() error: retcode=%d", ret);
    	}
   
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

    if (tpGetParamInt(m_ptpTask->params, "peerPort", &m_peerPort) != 0) {
        LOG_ERR("读取参数[peerPort]出错");
        return -1;
    }
    LOG_MSG("参数[peerPort]=[%d]", m_peerPort);
  
    if (tpGetParamInt(m_ptpTask->params, "lenlen", &m_lenlen) != 0) m_lenlen = 4;

    if (tpGetParamInt(m_ptpTask->params, "timeout", &m_timeout) != 0) m_timeout = 60;
    LOG_MSG("参数[timeout]=[%d]", m_timeout);

    if (tpGetParamInt(m_ptpTask->params, "nMaxThds", &m_nMaxThds) != 0) m_nMaxThds = 5;
    LOG_MSG("参数[nMaxThds]=[%d]", m_nMaxThds);

    return 0;
}

/***************************************************************
 * 处理线程
 **************************************************************/
int tpProcess( TPMSG *ptpMsg,int sockfd )
{
    int ret,msgLen;
    char logPrefix[21];
    MQHDL *mqh = NULL;
/*
    TPMSG *ptpMsg = (TPMSG *)arg;
*/
    tpSetLogPrefix(ptpMsg->head.tpId, logPrefix);

    LOG_MSG("收到主控发来的报文");
    if (hasLogLevel(DEBUG)) tpHeadDebug(&ptpMsg->head);
    if (hasLogLevel(DEBUG)) LOG_HEX(ptpMsg->body, ptpMsg->head.bodyLen);

    /*发送报文*/
    if ((ret = tcp_send(sockfd, ptpMsg->body, ptpMsg->head.bodyLen, m_lenlen, 0)) == -1) {
        LOG_ERR("向服务端发送报文出错");
        return -2;;
    }
    LOG_MSG("发送报文到服务端...[ok]");

	return 0;
    /*接收报文*/
    if ((ptpMsg->head.bodyLen = tcp_recv(sockfd, ptpMsg->body, 0, m_lenlen, m_timeout)) == -1) {
        LOG_ERR("tcp_recv() error: %s", strerror(errno));
        LOG_ERR("接收服务端报文出错");
        goto THD_END;
    }
    LOG_MSG("收到服务端返回报文");
    if (hasLogLevel(DEBUG)) LOG_HEX(ptpMsg->body, ptpMsg->head.bodyLen);

    /*绑定队列*/
    if ((ret = mq_bind(m_ptpTask->bindQ, &mqh)) != 0) {
        LOG_ERR("mq_bind() error: retcode=%d", ret);
        LOG_ERR("绑定队列[%d]出错", m_ptpTask->bindQ);
        goto THD_END;
    }

    /*写主控队列*/
    msgLen = ptpMsg->head.bodyLen + sizeof(TPHEAD);
    if ((ret = mq_post(mqh, NULL, Q_SVR, 0, NULL, (char *)ptpMsg, msgLen, 0)) != 0) {
        LOG_ERR("mq_post() error: retcode=%d", ret);
        LOG_ERR("写主控队列[%d]出错", Q_SVR);
        goto THD_END;
    }
    LOG_MSG("写主控队列[%d]...[ok]", Q_SVR);
	return 0;
THD_END:
    if (mqh) mq_detach(mqh);
	return -1;
}

