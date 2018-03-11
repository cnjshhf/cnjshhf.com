/**************************************************************
 * TCP通讯同步短连接服务端程序
 *************************************************************/
#include "tpbus.h"

TPTASK m_tpTask;

char m_bindAddr[IPADDRLEN+1];  /*服务端IP地址*/
int  m_bindPort;               /*服务端侦听端口*/
int  m_lenlen;                 /*通讯头长度*/
int  m_timeout;                /*超时时间*/
int  m_nMaxThds;               /*最大并发数*/
int  m_des_q;

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
    int  q_des;
    char alTranCode[5];
    char logPrefix[21];
    char clazz[MSGCLZLEN+1];
    char logfname[200];
    time_t t;
    TPMSG tpMsg;
    FILE  *fp;
    char  filename[200];

    /*检查命令行参数*/
    if (argc != 2) {
        printf("Usage: %s <任务ID>\n", argv[0]);
        return -1;
    }

    /*初始化进程*/
    if (tpTaskInit(atoi(argv[1]), "tpTcpALS", &m_tpTask) != 0) {
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
    if ((ret = mq_bind(m_tpTask.bindQ, &mqh)) != 0) {
        LOG_ERR("mq_bind() error: retcode=%d", ret);
        LOG_ERR("绑定队列[%d]出错", m_tpTask.bindQ);
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
            if ((msgLen = tcp_recv_len(m_csock, tpMsg.body, 0, m_lenlen, m_timeout)) <= 0) {
                LOG_ERR("tcp_recv() error: %s", strerror(errno));
                LOG_ERR("接收客户端报文出错");
                close(m_csock);
                break;
            }
            LOG_MSG("收到客户端[%s:%d]发来的报文",peerAddr, peerPort);
            
            if (hasLogLevel(DEBUG)) LOG_HEX(tpMsg.body, msgLen);
            if (msgLen == 4)  continue; /*心跳报文*/
            
            /*gxz
            sprintf(filename, "%s/iso.txt", getenv("HOME"));
            fp = fopen(filename,"w+");
            fprintf(fp, "%d\n",msgLen);
            fwrite(tpMsg.body,1,msgLen,fp);
            fclose(fp); */

            /* 初始化报文头 */
            if (tpNewHead(m_tpTask.bindQ, &tpMsg.head) != 0) {
                LOG_ERR("初始化报文头出错");
                continue;
            }
            tpMsg.head.bodyLen = msgLen - m_lenlen;
            
            tpSetLogPrefix(tpMsg.head.tpId, logPrefix);
            
            LOG_MSG("初始化报文头...[ok]");
            if (hasLogLevel(DEBUG)) tpHeadDebug(&tpMsg.head);

            /*写主控队列*/
            snprintf(clazz, sizeof(clazz), "%d", tpMsg.head.tpId); 
            msgLen = tpMsg.head.bodyLen + sizeof(TPHEAD);
            memmove(tpMsg.body,tpMsg.body + m_lenlen, tpMsg.head.bodyLen);

            q_des = m_des_q;

            if ((ret = mq_post(mqh, NULL, q_des, 0, clazz, (char *)&tpMsg, msgLen, 0)) != 0) {
                LOG_ERR("mq_post() error: retcode=%d", ret);
                LOG_ERR("写主控队列[%d]出错", Q_SVR);
                continue;
            }
            LOG_MSG("写主控队列[%d]...[ok]", q_des);
        }  
    }  
}

/***************************************************************
 * 初始化配置参数
 **************************************************************/
int tpParamInit()
{
    if (tpGetParamString(m_tpTask.params, "bindAddr", m_bindAddr) != 0) {
        LOG_ERR("读取参数[bindAddr]出错");
        return -1;
    }
    LOG_MSG("参数[bindAddr]=[%s]", m_bindAddr);

    if (tpGetParamInt(m_tpTask.params, "bindPort", &m_bindPort) != 0) {
        LOG_ERR("读取参数[bindPort]出错");
        return -1;
    }
    LOG_MSG("参数[bindPort]=[%d]", m_bindPort);
  
    if (tpGetParamInt(m_tpTask.params, "lenlen", &m_lenlen) != 0) m_lenlen = 4;
    LOG_MSG("参数[lenlen]=[%d]", m_lenlen);

    if (tpGetParamInt(m_tpTask.params, "timeout", &m_timeout) != 0) m_timeout = 60;
    LOG_MSG("参数[timeout]=[%d]", m_timeout);
 
    if (tpGetParamInt(m_tpTask.params, "des_q", &m_des_q) != 0) m_des_q = Q_SVR;
    LOG_MSG("参数[des_q]=[%d]", m_des_q);
 
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


/****************************************************************
 * 接收消息报文(可指定预读长度,返回报文带长度值)
 ****************************************************************/
int tcp_recv_len(int sockfd, void *buf, int len, int lenlen, int timeout)
{
    char lenbuf[11];
    int    nread,flags;
    int    nsecs = timeout;

    if (lenlen > 0) {
        if (lenlen >= sizeof(lenbuf)) lenlen = sizeof(lenbuf) - 1;
        if (tcp_read(sockfd, lenbuf, lenlen, timeout) != 0) return -1;
        lenbuf[lenlen] = 0;
        len = atoi(lenbuf);
        strcpy(buf, lenbuf);
        if (tcp_read(sockfd, buf+lenlen, len, timeout) != 0) return -1;
        return (len+lenlen);
    } else {
        if (len > 0) {
            if (tcp_read(sockfd, buf, len, timeout) != 0) return -1;
            return len;
        }

        if (timeout <= 0) {
            while (1) {
                if ((nread = read(sockfd, buf, SORCVBUFSZ)) == -1) {
                    if (errno == EINTR) continue;
                    return -1;
                } else if (nread == 0) {
                    errno = ECONNRESET;
                    return -1;
                } else {
                    return nread;
                }
            }
        } else {
            flags = fcntl(sockfd, F_GETFL, 0);
            fcntl(sockfd, F_SETFL, flags | O_NONBLOCK); 
            /* 设置非阻塞模式 */
            while (nsecs > 0) {
                if (readable(sockfd, 1)) {
                    if ((nread = read(sockfd, buf, SORCVBUFSZ)) == -1) {
                        /*if (errno == EINTR) continue;*/
                        return -1;
                    } else if (nread == 0) {
                        errno = ECONNRESET;
                        return -1;
                    } else {
                        fcntl(sockfd, F_SETFL, flags);
                        return nread;
                    }
                }
                nsecs--;
            }
            fcntl(sockfd, F_SETFL, flags);
            errno = ETIMEDOUT;
            return -1;
        }
    }
}
