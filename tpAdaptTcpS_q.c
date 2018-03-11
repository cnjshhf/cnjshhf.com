/**************************************************************
 * TCP通讯同步短连接服务端程序
 * 增加辅助动能 1：消息队列
               2：动态调整接收消息线程数
 *************************************************************/
#include "queue.h"
#include "tpbus.h"
#define MSG_PEEK_NOT_FOUND -100

static TPTASK m_tpTask;

static char m_bindAddr[IPADDRLEN+1];  /*服务端IP地址*/
static int  m_bindPort;               /*服务端侦听端口*/
static int  m_lenlen;                 /*通讯头长度*/
static int  m_timeout;                /*超时时间*/
static int  m_recv_timeout;           /*recv接收超时时间*/
static int  m_sleepts;                /*超时连接清理间隔时间*/ 
static int  m_nReqMaxThds;            /*线程池预开线程数*/
static int  m_nMaxThds;               /*线程池最大线程数*/
static int  m_monQ;                   /*交易监控转发队列*/      /*add by xmdwen @2014-12-10 15:42:26*/
static int  m_desQ;                   /*转发队列        */      /*add by xmdwen @2016-10-31 16:33:00*/
static int  m_logLen;                 /*打印日志长度 避免太多日志 */
static int  m_mngFlag;                /*是否动态调整线程*/
static int  m_msgKey;                 /*消息队列关键字*/

static char m_ign_ip1[IPADDRLEN+1];   /* 日志忽略ip */
static char m_ign_ip2[IPADDRLEN+1];

struct sock_info {
    int    sockfd;
    char   peerAddr[IPADDRLEN+1];
    int    peerPort;
    int    tpId;
    time_t time;
    char   logkey[60];               /*打印日志关键字*/
    TAILQ_ENTRY(sock_info) entry;
};
TAILQ_HEAD(, sock_info) m_tq;  /*请求套接字队列*/
TAILQ_HEAD(, sock_info) m_wq;  /*等待套接字队列*/

int    thread_cnt;
int    gmsqid;

#define  MIN_REQ_THREAD     4

static void  tpQuit();
static int   tpParamInit();
static void *tpAcceptThread(void *arg);
static void *tpRequestThread(void *arg);
static void *tpResponseThread(void *arg);
static void *tpChgThread(void *arg);
static void *tpThreadMng(void *arg);
static int tcp_recv_h(int sockfd, void *buf, int len, int lenlen, int timeout);

/**************************************************************
 * 主函数 
 *************************************************************/
int main(int argc, char *argv[])
{
    int i,ret;
    time_t t;
    struct sock_info *pSock,*next;

    /*检查命令行参数*/
    if (argc != 2) {
        printf("Usage: %s <任务ID>\n", argv[0]);
        return -1;
    }

    /*初始化进程*/
    if (tpTaskInit(atoi(argv[1]), "tpTcpS", &m_tpTask) != 0) {
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
    
    /*设置退出信号*/
    signal(SIGTERM, tpQuit);

    /* 创建消息队列 */
    if ( m_msgKey == -1 )
        gmsqid = msgget((key_t)IPC_PRIVATE, IPC_CREAT|0600);
    else
        gmsqid = msgget((key_t)m_msgKey, IPC_CREAT|0600);
    if ( gmsqid < 0) {
         LOG_ERR("创建消息队列出错: %s", strerror(errno));
         return -1;
    }
    LOG_MSG("创建消息队列[%d]...[OK]", gmsqid);

    TAILQ_INIT(&m_tq);
    TAILQ_INIT(&m_wq);

    /*创建请求处理线程池*/
    for (i=0; i<m_nReqMaxThds; i++) {
        if (tpThreadCreate(tpRequestThread, NULL) != 0) {
            LOG_ERR("创建线程失败: %s", strerror(errno));
            if (gmsqid != -1) msgctl(gmsqid, IPC_RMID, 0);
            return -1;
        }
    }
    LOG_MSG("创建请求处理线程池...[ok]");
    
    for (i=0; i<(m_nMaxThds); i++) {
        if (tpThreadCreate(tpChgThread, NULL) != 0) {
            LOG_ERR("创建线程失败: %s", strerror(errno));
            if (gmsqid != -1) msgctl(gmsqid, IPC_RMID, 0);
            return -1;
        }
    }
    LOG_MSG("创建转发处理线程池...[ok]");

    /*创建响应处理线程池*/
    for (i=0; i<m_nMaxThds; i++) {
        if (tpThreadCreate(tpResponseThread, NULL) != 0) {
            LOG_ERR("创建线程失败: %s", strerror(errno));
            if (gmsqid != -1) msgctl(gmsqid, IPC_RMID, 0);
            return -1;
        }
    }
    LOG_MSG("创建响应处理线程池...[ok]");

    /*创建连接侦听线程*/
    if (tpThreadCreate(tpAcceptThread, NULL) != 0) {
        LOG_ERR("创建线程失败: %s", strerror(errno));
        if (gmsqid != -1) msgctl(gmsqid, IPC_RMID, 0);
        return -1;
    }
    LOG_MSG("创建连接侦听线程...[ok]");
    
    /*创建管理线程*/
    if (m_mngFlag ==1) {
        if (tpThreadCreate(tpThreadMng, NULL) != 0) {
            LOG_ERR("创建线程失败: %s", strerror(errno));
            if (gmsqid != -1) msgctl(gmsqid, IPC_RMID, 0);
            return -1;
        }
        LOG_MSG("创建管理线程...[ok]");
    }

    /*清理超时连接*/
    while (1) {
        time(&t);
        TAILQ_LOCK(&m_wq);
    
        pSock = TAILQ_FIRST(&m_wq);
        while (pSock) {
            next = TAILQ_NEXT(pSock, entry);
            if ((t - pSock->time) > m_timeout) {
                LOG_WRN("[%s]清理超时连接:[%d]->[%d]:[%d]",pSock->logkey, pSock->time, t, pSock->sockfd);
                TAILQ_REMOVE(&m_wq, pSock, entry);
                close(pSock->sockfd);
                free(pSock);
            }
            pSock = next;
        }
        TAILQ_UNLOCK(&m_wq);
        sleep(m_sleepts);
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

    if (tpGetParamInt(m_tpTask.params, "msgKey", &m_msgKey) != 0) {
        LOG_ERR("读取参数[msgKey]出错");
        m_msgKey = -1; 
        return -1;
    }
    LOG_MSG("参数[msgKey]=[%d]", m_msgKey);
    
  
    if (tpGetParamInt(m_tpTask.params, "lenlen", &m_lenlen) != 0) m_lenlen = 4;
    LOG_MSG("参数[lenlen]=[%d]", m_lenlen);

    if (tpGetParamInt(m_tpTask.params, "recv_timeout", &m_recv_timeout) != 0) m_recv_timeout = 20;
    LOG_MSG("参数[recv_timeout]=[%d]", m_recv_timeout);

    if (tpGetParamInt(m_tpTask.params, "timeout", &m_timeout) != 0) m_timeout = 60;
    LOG_MSG("参数[timeout]=[%d]", m_timeout);

    if (tpGetParamInt(m_tpTask.params, "sleepts", &m_sleepts) != 0) m_sleepts = 10;
    LOG_MSG("参数[sleepts]=[%d]", m_sleepts);

     if (tpGetParamInt(m_tpTask.params, "nMaxThds", &m_nMaxThds) != 0) m_nMaxThds = 5;
    LOG_MSG("参数[nMaxThds]=[%d]", m_nMaxThds);
    
    if (tpGetParamInt(m_tpTask.params, "nReqMaxThds", &m_nReqMaxThds) != 0) m_nReqMaxThds = 100;
    LOG_MSG("参数[nReqMaxThds]=[%d]", m_nReqMaxThds);
    
    if (tpGetParamInt(m_tpTask.params, "mngFlag", &m_mngFlag) != 0) m_mngFlag = 0;
    LOG_MSG("参数[mngFlag]=[%d]", m_mngFlag);

    if (tpGetParamInt(m_tpTask.params, "monQ", &m_monQ) != 0) m_monQ = -1;
    LOG_MSG("参数[monQ]=[%d]", m_monQ);
   
    if (tpGetParamInt(m_tpTask.params, "desQ", &m_desQ) != 0) m_desQ = Q_SVR;
    LOG_MSG("参数[desQ]=[%d]", m_desQ);
    
    if (tpGetParamInt(m_tpTask.params, "logLen", &m_logLen) != 0) m_logLen = 300;
    LOG_MSG("参数[logLen]=[%d]", m_logLen);
    
    if (tpGetParamString(m_tpTask.params, "ign_ip1", m_ign_ip1) != 0) strcpy(m_ign_ip1,"127.0.0.1");
    LOG_MSG("参数[ign_ip1]=[%s]", m_ign_ip1);

    if (tpGetParamString(m_tpTask.params, "ign_ip2", m_ign_ip2) != 0) strcpy(m_ign_ip2,"127.0.0.1");
    LOG_MSG("参数[ign_ip2]=[%s]", m_ign_ip2);

    return 0;
}

/***************************************************************
 * 连接侦听线程
 **************************************************************/
void *tpAcceptThread(void *arg)
{
    int    sockfd;
    struct sock_info *pSock;
    char logPrefix[21];
    struct timeval timestamp;
    time_t t;

    tpSetLogPrefix(0, logPrefix);
    /*初始化侦听端口*/
    if ((sockfd = tcp_listen(m_bindAddr, m_bindPort)) == -1) {
        LOG_ERR("初始化侦听端口[%s:%d]出错", m_bindAddr, m_bindPort);
        tpThreadExit();
        return NULL;
    }

    while (1) {
        /*申请套接字资源*/
        if (!(pSock = (struct sock_info*)malloc(sizeof(struct sock_info)))) {
            LOG_ERR("malloc() error: %s", strerror(errno));
            LOG_ERR("申请套接字资源出错");
            sleep(1);
            continue;
        }
        memset(pSock, 0, sizeof(struct sock_info));

        /*接受连接请求*/
        pSock->sockfd = tcp_accept(sockfd, pSock->peerAddr, &pSock->peerPort);
        if (pSock->sockfd == -1) {
            LOG_ERR("接受客户端连接请求出错");
            tpThreadExit();
            return NULL;
        }
        gettimeofday( &timestamp, NULL );
        sprintf(pSock->logkey, "%d|%d.%06d",pSock->sockfd, timestamp.tv_sec, timestamp.tv_usec);
        /* 设置日志过滤条件 */
        if (strcmp(pSock->peerAddr, m_ign_ip1) != 0 && strcmp(pSock->peerAddr, m_ign_ip2) !=0) {
            LOG_MSG("[%s]接受客户端[%s:%d]的连接请求[sockfd=%d]", pSock->logkey, pSock->peerAddr, pSock->peerPort, pSock->sockfd); 
        } 
        set_sock_linger(pSock->sockfd, 1, 1);

        /*写入请求套接字队列*/
        time(&t);
        pSock->time = t;
        TAILQ_LOCK(&m_tq);
        TAILQ_INSERT_TAIL(&m_tq, pSock, entry);
        TAILQ_UNLOCK(&m_tq);
        TAILQ_SIGNAL(&m_tq);
    }
}

/***************************************************************
 * 请求处理线程
 **************************************************************/
void *tpRequestThread(void *arg)
{
    int  ret,msgLen;
    char logPrefix[21];
    MQHDL *mqh;
    TPMSG tpMsg;
    time_t t, t1, t2, t3;
    struct sock_info *pSock;
    char log_buf[MAXMSGLEN];
    int log_len;
    struct timeval time_recv;
    struct timeval time_1;
    struct timeval time_2;
    struct timeval time_3;
    struct timeval time_4;
    char time_buf[20];

    int sk_f = 0;         /* 日志过滤标识 */

    while (1) {
        /*提取请求套接字*/
        TAILQ_LOCK(&m_tq);
        while (TAILQ_EMPTY(&m_tq)) TAILQ_WAIT(&m_tq);
        if (TAILQ_EMPTY(&m_tq)) {
            TAILQ_UNLOCK(&m_tq);
            continue;
        }
        pSock = TAILQ_FIRST(&m_tq);
        TAILQ_REMOVE(&m_tq, pSock, entry);
        TAILQ_UNLOCK(&m_tq);
        
        if (pSock->sockfd == -100) {
            LOG_MSG("%d 收到线程退出指令，线程退出运行",pthread_self());
            free(pSock);
            thread_cnt--;
            tpThreadExit();
            return NULL; 
        }

         time(&t1);

        tpSetLogPrefix(0, logPrefix);
        gettimeofday( &time_1, NULL );

        /*接收请求报文  udpated by fox 2016/12/29 */
        if((msgLen = tcp_recv_h(pSock->sockfd, tpMsg.body, 0, m_lenlen, 0)) <= 0)
        {
          time(&t3);
          if ( (t3-pSock->time) >= m_recv_timeout) {
              LOG_MSG("[%s]tcp_recv overtime %ds close sockfd[%d]",pSock->logkey, m_recv_timeout, pSock->sockfd);
              close(pSock->sockfd);
              free(pSock);
              continue;
          }
          if (msgLen == MSG_PEEK_NOT_FOUND){
             /*写入请求套接字队列*/
             TAILQ_LOCK(&m_tq);
             TAILQ_INSERT_TAIL(&m_tq, pSock, entry);
             TAILQ_UNLOCK(&m_tq);
             TAILQ_SIGNAL(&m_tq);
             continue;
          }
          if(strcmp(pSock->peerAddr, m_ign_ip1) && strcmp(pSock->peerAddr, m_ign_ip2))
          {
            LOG_DBG("[%s]tcp_recv return [%d]", pSock->logkey, msgLen);
            if (msgLen != -2)
              LOG_ERR("[%s]接收客户端请求报文出错", pSock->logkey);
            else
              LOG_MSG("[%s]客户端断开连接", pSock->logkey);
          }
          close(pSock->sockfd);
          free(pSock);
          continue;
        }
        time(&t2);
        if( t2 - t1 > 1){
            LOG_MSG("[%s]tcp_recv_h :recv over 1 s:recv begin[%d]->recv end[%d] overtime:[%d]s [%s:%d] sockfd [%d]", pSock->logkey, t1, t2, t2-t1, pSock->peerAddr, pSock->peerPort ,pSock->sockfd );
        }

        /* 初始化报文头 */
        if (tpNewHead(m_tpTask.bindQ, &tpMsg.head) != 0) {
            LOG_ERR("初始化报文头出错");
            close(pSock->sockfd);
            free(pSock);
            continue;
        }
        tpMsg.head.bodyLen = msgLen;
        /*tpMsg.head.corrId = pSock->sockfd;*/

        if(tpMsg.head.bodyLen>m_logLen)
            log_len = m_logLen;
        else
            log_len = tpMsg.head.bodyLen;
        memcpy(log_buf, tpMsg.body, log_len);
        log_buf[log_len] = 0x00;

        tpSetLogPrefix(tpMsg.head.tpId, logPrefix);
        LOG_MSG("[%s]接收客户端请求报文...[ok][%d][%s]",pSock->logkey, tpMsg.head.bodyLen, log_buf); 
        
        /*写入等待套接字队列*/
        time(&t);
        TAILQ_LOCK(&m_wq);
        pSock->tpId = tpMsg.head.tpId;
        pSock->time = t;
        TAILQ_INSERT_TAIL(&m_wq, pSock, entry);
        TAILQ_UNLOCK(&m_wq);

        /*必须先将套接字队列放在前面 否则如果返回线程先读到返回报文 但是套接字没有进入队列就会出现找不到的情况*/
        if (msgsnd(gmsqid, &tpMsg, msgLen+sizeof(TPHEAD), 0) == -1) {
            LOG_ERR("msgsnd() error: %s", strerror(errno));
            close(pSock->sockfd);
            free(pSock);
            continue;;
        }

    }
}

/***************************************************************
 * 转发处理线程
 **************************************************************/
void *tpChgThread(void *arg)
{
    int  ret,msgLen;
    char logPrefix[21];
    MQHDL *mqh;
    TPMSG tpMsg;
    time_t t;
    struct sock_info *pSock;

    int sk_f = 0;         /* 日志过滤标识 */

    /*绑定队列*/
    if ((ret = mq_bind(m_tpTask.bindQ, &mqh)) != 0) {
        LOG_ERR("mq_bind() error: retcode=%d", ret);
        LOG_ERR("绑定队列[%d]出错", m_tpTask.bindQ);
        tpThreadExit();
        return NULL;        
    }

    while (1) {
        msgLen = msgrcv(gmsqid, &tpMsg, sizeof(TPMSG), 0, 0) ;
        if (msgLen < 0) {
            LOG_ERR("msgsnd() error: %s", strerror(errno));
            continue;;
        } 
        
        tpSetLogPrefix(tpMsg.head.tpId, logPrefix);
        /*发送交易监控队列 add by xmdwen @2014-12-10 15:54:59 bgn*/
        if( m_monQ >= 0)
        {
            if (ret= mq_post(mqh, NULL, m_monQ, 0, NULL, (char *)&tpMsg, msgLen, 0)) {
                LOG_ERR("write queue_mon[%d] error[%d]",m_monQ,ret);
            } 
            else
                LOG_MSG("write queue_mon[%d] OK",m_monQ);
        }
        /*发送交易监控队列 add by xmdwen @2014-12-10 15:54:59 end*/
        
        /*发送TPBUS请求*/
        if (tpSend1(mqh, &tpMsg, m_desQ, 0) != 0) {
            LOG_ERR("发送TPBUS请求出错");
            continue;
        }
    }
}

/***************************************************************
 * 响应处理线程
 **************************************************************/
void *tpResponseThread(void *arg)
{
    int  ret;
    char logPrefix[21];
    MQHDL *mqh;
    TPMSG tpMsg;
    struct sock_info *pSock;
    char log_buf[MAXMSGLEN];
    int log_len;
    char time_buf[20];
    struct timeval time_back;
    struct timeval time_1;
    char    msgbuf[MAXMSGLEN];        /*add by dwxiong 2014-11-7 19:43:05 @直销银行.互联网收单*/


    /*绑定队列*/
    if ((ret = mq_bind(m_tpTask.bindQ, &mqh)) != 0) {
        LOG_ERR("mq_bind() error: retcode=%d", ret);
        LOG_ERR("绑定队列[%d]出错", m_tpTask.bindQ);
        tpThreadExit();
        return NULL;        
    }

    while (1) {
        tpSetLogPrefix(0, logPrefix);
        /*接收TPBUS返回*/
        if (tpRecv1(mqh, &tpMsg, m_desQ, 0, 0) != 0) {
            LOG_ERR("接收TPBUS返回出错");
            mq_detach(mqh);
            tpThreadExit();
            return NULL;
        }
        tpSetLogPrefix(tpMsg.head.tpId, logPrefix);

        TAILQ_LOCK(&m_wq);
        pSock = TAILQ_FIRST(&m_wq);
        while (pSock) {
            /*if (pSock->sockfd == tpMsg.head.corrId && pSock->tpId == tpMsg.head.tpId) break;*/
            if (pSock->tpId == tpMsg.head.tpId) break;
            pSock = TAILQ_NEXT(pSock, entry);
        }
        if (pSock) TAILQ_REMOVE(&m_wq, pSock, entry);
        TAILQ_UNLOCK(&m_wq);

        if (!pSock) {
            LOG_ERR("[%s]迟到响应(tpId=[%d]): 连接因超时已关闭", pSock->logkey, tpMsg.head.tpId);
            continue;
        } 

        if(tpMsg.head.bodyLen>m_logLen)
            log_len = m_logLen;
        else
            log_len = tpMsg.head.bodyLen;
        memcpy(log_buf, tpMsg.body, log_len);
        log_buf[log_len] = 0x00;

        memset(msgbuf,0x00,sizeof(msgbuf));
        snprintf(msgbuf, sizeof(msgbuf), "%*.*d", m_lenlen, m_lenlen, tpMsg.head.bodyLen);
        memcpy(msgbuf+m_lenlen,tpMsg.body,tpMsg.head.bodyLen);
        msgbuf[MAXMSGLEN-1] = '\0';
        if ((ret = tcp_send(pSock->sockfd, msgbuf, tpMsg.head.bodyLen+m_lenlen, -1, 0)) == -1) {
            LOG_ERR("tcp_send() error: %s", strerror(errno));
            LOG_ERR("[%s]发送客户端响应报文出错", pSock->logkey);
        } else {
            gettimeofday( &time_back, NULL );
            LOG_MSG("[%s]发送客户端响应报文...[ok][%d][%s]", pSock->logkey, tpMsg.head.bodyLen, log_buf);
            
            /*发送交易监控队列 add by xmdwen @2014-12-10 15:54:59 bgn*/
            if( m_monQ >= 0)
            {
                if (ret= mq_post(mqh, NULL, m_monQ, 0, NULL, (char *)&tpMsg, sizeof(TPMSG), 0)) {
                    LOG_ERR("[%s]write queue_mon[%d] error[%d]", pSock->logkey, m_monQ,ret);
                }
                else
                    LOG_MSG("write queue_mon[%d] OK",m_monQ);
            }
            /*发送交易监控队列 add by xmdwen @2014-12-10 15:54:59 end*/
        }
        close(pSock->sockfd);
        free(pSock);
    }
}

/***************************************************************
 * 管理线程
 **************************************************************/
void *tpThreadMng(void *arg)
{
    int    i,j;
    int    tq_cnd     = 0;
    struct sock_info *pSock;
    
    thread_cnt = m_nMaxThds*MIN_REQ_THREAD;
    while(1) {
        sleep(1);

        tq_cnd = 0;
        TAILQ_LOCK(&m_tq);
        TAILQ_FOREACH(pSock, &m_tq, entry) tq_cnd++;
        TAILQ_UNLOCK(&m_tq);
        if( tq_cnd > m_nMaxThds ) tq_cnd = m_nMaxThds;

        LOG_MSG("tpThreadMng:[tq_cnd=%d] [thread_cnt=%d]", tq_cnd, thread_cnt);
        if (tq_cnd > thread_cnt ) {
            /*创建请求处理线程池*/
            j = 0;
            for (i=0; i<(tq_cnd - thread_cnt); i++) {
                if (tpThreadCreate(tpRequestThread, NULL) != 0) {
                    LOG_ERR("创建线程失败: %s", strerror(errno));
                }
                else
                    j++;
            }
            thread_cnt += j;
            LOG_MSG("xjtest: 创建请求处理线程池，线程数=[%d],当前线程总数=[%d]",j,thread_cnt);
        }
        else if( (thread_cnt > tq_cnd) && (thread_cnt > m_nMaxThds*MIN_REQ_THREAD) ){
            for (i=0; i<(thread_cnt - tq_cnd - m_nMaxThds*MIN_REQ_THREAD); i++) {
                if (!(pSock = (struct sock_info*)malloc(sizeof(struct sock_info)))) {
                    LOG_ERR("malloc() error: %s", strerror(errno));
                    LOG_ERR("申请套接字资源出错");
                    continue;
                }
                memset(pSock, 0, sizeof(struct sock_info));
                pSock->sockfd = -100;
            
                TAILQ_LOCK(&m_tq);
                TAILQ_INSERT_TAIL(&m_tq, pSock, entry);
                TAILQ_UNLOCK(&m_tq);
                TAILQ_SIGNAL(&m_tq); 
            }
            LOG_MSG("xjtest: 退出冗余的[%d]个请求处理线程",thread_cnt - tq_cnd - m_nMaxThds*MIN_REQ_THREAD);
        }
    }
}


/****************************************************************
 * 接收消息报文(可指定预读长度)
 ****************************************************************/
int tcp_recv_h(int sockfd, void *buf, int len, int lenlen, int timeout)
{
    char lenbuf[11];
    int    nread,flags;
    int    nsecs = timeout;

	char   tbuf[10+1];
	int    nget;

    if (lenlen > 0) {
        if (lenlen >= sizeof(lenbuf)) lenlen = sizeof(lenbuf) - 1;
        /*MSG_PEEK 去查看是否有lenlen长度的报文,探测不到报文返回*/
        flags = fcntl(sockfd, F_GETFL, 0);
        fcntl(sockfd, F_SETFL, flags | O_NONBLOCK);
        nget = recv(sockfd, tbuf, lenlen, MSG_PEEK);
        if (nget == -1 || errno == EINTR) {
            return MSG_PEEK_NOT_FOUND;
        }
        if(timeout<=0) {
            fcntl(sockfd, F_SETFL, 0);
        }
        /* 2 times try to get 1 byte for testing recieve */
        if (tcp_read(sockfd, lenbuf, lenlen, timeout) != 0)
        {
            if (0 == nget)  /* 0 byte recv,means no byte been sent */
                return -2;
            else      /* 1 byte recv succ,means sent error */
                return -1;
        }
        lenbuf[lenlen] = 0;
        len = atoi(lenbuf);
        if (tcp_read(sockfd, buf, len, timeout) != 0) return -1;
        return len;
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


/***************************************************************
 * 发送服务请求(用于请求端通讯程序)
 **************************************************************/
int tpSend1(MQHDL *mqh, TPMSG *ptpMsg, int m_desq, int flag)
{
    int   i,ret,msgLen;
    char *rqm,clazz[MSGCLZLEN+1];

    i = (ptpMsg->head.tpId/100000000)%10;
    rqm = (i==0)? NULL : gShm.ptpConf->hosts[i-1].qm;
    clazz[0] = 0;
    if (flag) snprintf(clazz, sizeof(clazz), "%d", ptpMsg->head.tpId);
    msgLen = ptpMsg->head.bodyLen + sizeof(TPHEAD);
    if ((ret = mq_post(mqh, rqm, m_desq, 0, clazz, (char *)ptpMsg, msgLen, 0)) != 0) {
        LOG_ERR("mq_post() error: retcode=%d", ret);
        LOG_ERR("写主控队列[%d]出错", m_desq);
        return -1;
    }
    LOG_MSG("写主控队列[%d]...[ok]", m_desq);
    return 0;
}

/***************************************************************
 * 接收服务响应(用于请求端通讯程序)
 **************************************************************/
int tpRecv1(MQHDL *mqh, TPMSG *ptpMsg, int m_desq, int timeout, int flag)
{
    int  q,ret,msgLen;
    char clazz[MSGCLZLEN+1];

    clazz[0] = 0;
    if (flag) snprintf(clazz, sizeof(clazz), "%d", ptpMsg->head.tpId);
    msgLen = sizeof(TPMSG);
    if ((ret = mq_pend(mqh, NULL, &q, NULL, clazz, (char *)ptpMsg, &msgLen, timeout)) != 0) {
        LOG_ERR("mq_pend() error: retcode=%d", ret);
        LOG_ERR("读绑定队列[%d]出错", mqh->qid);
        return -1;
    }
    if (q == m_desq) {
        /**
        LOG_MSG("收到来自主控程序的报文");
        **/
        return 0;
    }
    else if(q == Q_REV)
    {
        /**
        LOG_MSG("收到来自事务程序的报文");
        **/
        return 0;
    }
    else {
        LOG_ERR("收到来自队列[%d]的非法报文", q);
        return -1;
    }
    return 0;
}

/******************************************************************************
 * 函数: tpQuit
 * 功能: 进程退出函数
 ******************************************************************************/
void tpQuit(int sig)
{
    signal(SIGTERM, SIG_IGN);
    if (gmsqid != -1) msgctl(gmsqid, IPC_RMID, 0);
    exit(sig);
}

