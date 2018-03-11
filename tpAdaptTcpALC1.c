/**************************************************************
 * TCP通讯同步短连接客户端程序
 *************************************************************/
#include "queue.h"
#include "tpbus.h"

#define QUEUE_MON    21

TPTASK m_tpTask;  /*任务配置*/

char m_bindAddr[IPADDRLEN+1];  /*本地端IP地址*/
char m_peerAddr[IPADDRLEN+1];  /*服务端IP地址*/
int  m_peerPort;               /*服务端侦听端口*/
int  m_lenlen;                 /*通讯头长度*/
int  m_timeout;                /*超时时间*/
int  m_nMaxThds;               /*最大并程数*/
int  m_sleeptime;
int  m_chg_q;

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
    char  msgbuff[1024*32];
    TPMSG tpMsg;
   
    
    /*检查命令行参数*/
    if (argc != 2) {
        printf("Usage: %s <任务ID>\n", argv[0]);
        return -1;
    }

    /*初始化进程*/
    if (tpTaskInit(atoi(argv[1]), "tpTcpALC", &m_tpTask) != 0) {
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
    if ((ret = mq_bind(m_tpTask.bindQ, &mqh)) != 0) {
        LOG_ERR("mq_bind() error: retcode=%d", ret);
        LOG_ERR("绑定队列[%d]出错", m_tpTask.bindQ);
        return -1;
    }
    
    for (;;){
    	  /*通讯连接*/
        if ((sockfd = tcp_connect(m_bindAddr, m_peerAddr, m_peerPort, 0)) == -1) {
            LOG_ERR("tcp_connect(%s,%d) error", m_peerAddr, m_peerPort);
            LOG_ERR("初始化通讯连接出错");
            /*
            if (mqh) mq_detach(mqh);
            return -1;*/
            sleep(m_sleeptime);
            continue;
        }
        LOG_MSG("建立与服务端[%s:%d]的通讯连接...[ok]", m_peerAddr, m_peerPort);
        
        /*通讯设置*/
        if (set_sock_linger(sockfd, 1, 1) == -1) {
            LOG_ERR("设置连接关闭逗留时间出错");
            tpQuit(-1);
        }

        for (;;){        
            msgLen = sizeof(TPMSG);
            if ((ret = mq_pend(mqh, NULL, &q, NULL, NULL, (char *)&tpMsg, &msgLen,m_timeout)) != 0) {
                if (ret == 1010){  /*超时*/
                    ret = tcp_send(sockfd, "0000", 4, 0, 0);
                    LOG_MSG("send test msg 0000");
                    if (ret == -1) {
                        LOG_ERR("向服务端发送报文出错");
                        close(sockfd);
                        break;
                    }
                    continue;
                }
                    
                LOG_ERR("mq_pend() error: retcode=%d", ret);
                tpQuit(-1);
            }
            
            /*if ( (q != Q_SVR) && (q != m_chg_q) ) continue; 只接收主控报文*/

            tpSetLogPrefix(tpMsg.head.tpId, logPrefix);

            LOG_MSG("收到[%d]发来的报文",q);
            if (hasLogLevel(DEBUG)) tpHeadDebug(&tpMsg.head);
            if (hasLogLevel(DEBUG)) LOG_HEX(tpMsg.body, tpMsg.head.bodyLen);

            /*发送报文*/
            memset(msgbuff,0x00,sizeof(msgbuff));
            sprintf(msgbuff, "%4.4d", tpMsg.head.bodyLen);
            memcpy(msgbuff+4, tpMsg.body, tpMsg.head.bodyLen);
            if ((ret = tcp_send(sockfd, msgbuff, tpMsg.head.bodyLen + 4, 0, 0)) == -1) {
                LOG_ERR("向服务端发送报文出错");
                close(sockfd);
                break;
            }
            
             LOG_MSG("发送报文到服务端[%s:%d]...[ok]",m_peerAddr, m_peerPort);

            if ( (tpMsg.body[3] == '1') || (tpMsg.body[3] == '6') ) {
                /*add by xujun 20140103 for tran_mon*/
                if (tpMsg.body[3] == '1') tpMsg.body[3] = '0';
                if (tpMsg.body[3] == '6') tpMsg.body[3] = '5';
                if (mq_post(mqh, NULL, QUEUE_MON, 0, NULL, tpMsg.body, tpMsg.head.bodyLen, 0)) {
                    LOG_ERR("write queue_mon error,trancode=[%4.4s]",tpMsg.body);
                }
                else {
                    LOG_MSG("write queue_mon ok,trancode=[%4.4s]",tpMsg.body);
                    if (hasLogLevel(DEBUG)) LOG_HEX(tpMsg.body, tpMsg.head.bodyLen);
                }
            }
        }
    }
}

/***************************************************************
 * 初始化配置参数
 **************************************************************/
int tpParamInit()
{
    if (tpGetParamString(m_tpTask.params, "bindAddr", m_bindAddr) != 0) strcpy(m_bindAddr, "127.0.0.1");
    LOG_MSG("参数[bindAddr]=[%s]", m_bindAddr);

    if (tpGetParamString(m_tpTask.params, "peerAddr", m_peerAddr) != 0) {
        LOG_ERR("读取参数[peerAddr]出错");
        return -1;
    }
    LOG_MSG("参数[peerAddr]=[%s]", m_peerAddr);

    if (tpGetParamInt(m_tpTask.params, "peerPort", &m_peerPort) != 0) {
        LOG_ERR("读取参数[peerPort]出错");
        return -1;
    }
    LOG_MSG("参数[peerPort]=[%d]", m_peerPort);
  
    if (tpGetParamInt(m_tpTask.params, "lenlen", &m_lenlen) != 0) m_lenlen = 4;
    LOG_MSG("参数[lenlen]=[%d]", m_lenlen);

    if (tpGetParamInt(m_tpTask.params, "timeout", &m_timeout) != 0) m_timeout = 60;
    LOG_MSG("参数[timeout]=[%d]", m_timeout);

    if (tpGetParamInt(m_tpTask.params, "nMaxThds", &m_nMaxThds) != 0) m_nMaxThds = 5;
    LOG_MSG("参数[nMaxThds]=[%d]", m_nMaxThds);

    if (tpGetParamInt(m_tpTask.params, "chg_q", &m_chg_q) != 0) m_chg_q = Q_SVR;
    LOG_MSG("参数[chg_q]=[%d]", m_chg_q);

    if (tpGetParamInt(m_tpTask.params, "sleeptime", &m_sleeptime) != 0) m_sleeptime = 60;
    LOG_MSG("参数[sleeptime]=[%d]", m_sleeptime);

    return 0;
}

void tpQuit(int sig)
{
	  signal(SIGTERM, SIG_IGN);
    close(sockfd);
    if (mqh) mq_detach(mqh);
    exit(sig);
}

