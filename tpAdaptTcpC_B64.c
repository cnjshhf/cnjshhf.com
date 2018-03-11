/**************************************************************
 * TCP通讯同步短连接客户端程序      明文 <-> Base64密文
 *************************************************************/
#include "queue.h"
#include "tpbus.h"

static TPTASK m_tpTask;

char m_bindAddr[IPADDRLEN+1];  /*本地端IP地址*/
char m_peerAddr[IPADDRLEN+1];  /*服务端IP地址*/
int  m_peerPort;               /*服务端侦听端口*/
int  m_lenlen;                 /*通讯头长度*/
int  m_timeout;                /*超时时间*/
int  m_selectus;               /*可读侦测间隔时间*/
int  m_nMaxThds;               /*最大并程数*/

struct sock_info {
    int  sockfd;
    time_t time;
    TPHEAD head;
    TAILQ_ENTRY(sock_info) entry;
};
TAILQ_HEAD(, sock_info) m_tq;   /*请求套接字队列*/
TAILQ_HEAD(, sock_info) m_sq;   /*可读套接字队列*/

static struct sock_info *m_socks[FD_SETSIZE];

static int   tpParamInit();
static void *tpRequestThread(void *arg);
static void *tpResponseThread(void *arg);

/**************************************************************
 * 主函数 
 *************************************************************/
int main(int argc, char *argv[])
{
    int i,j,ret,maxfd;
    fd_set rset;
    time_t t;
    struct timeval tval;
    struct sock_info *pSock,*next;

    /*检查命令行参数*/
    if (argc != 2) {
        printf("Usage: %s <任务ID>\n", argv[0]);
        return -1;
    }

    /*初始化进程*/
    if (tpTaskInit(atoi(argv[1]), "tpTcpC_B64", &m_tpTask) != 0) {
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

    TAILQ_INIT(&m_tq);
    TAILQ_INIT(&m_sq);

    /*创建请求处理线程池*/
    for (i=0; i<m_nMaxThds; i++) {
        if (tpThreadCreate(tpRequestThread, NULL) != 0) {
            LOG_ERR("创建线程失败: %s", strerror(errno));
            return -1;
        }
    }
    LOG_MSG("创建响应处理线程池...[ok]");

    /*创建响应处理线程池*/
    for (i=0; i<m_nMaxThds; i++) {
        if (tpThreadCreate(tpResponseThread, NULL) != 0) {
            LOG_ERR("创建线程失败: %s", strerror(errno));
            return -1;
        }
    }
    LOG_MSG("创建响应处理线程池...[ok]");

    tval.tv_sec  = 0;
    tval.tv_usec = m_selectus;

    while (1) {
        time(&t);
        j = 0;
        maxfd = -1;
        FD_ZERO(&rset);

        TAILQ_LOCK(&m_tq);
        while (TAILQ_EMPTY(&m_tq)) TAILQ_WAIT(&m_tq);
        if (TAILQ_EMPTY(&m_tq)) {
            TAILQ_UNLOCK(&m_tq);
            continue;
        }

        pSock = TAILQ_FIRST(&m_tq);
        while (pSock) {
            next = TAILQ_NEXT(pSock, entry);
            if ((t - pSock->time) > m_timeout) {
                TAILQ_REMOVE(&m_tq, pSock, entry);
                close(pSock->sockfd);
                free(pSock);
                pSock = next;
                continue;
            }
            FD_SET(pSock->sockfd, &rset);
            if (pSock->sockfd > maxfd) maxfd = pSock->sockfd;
            m_socks[j++] = pSock;
            pSock = next;
        }
        TAILQ_UNLOCK(&m_tq);

        if (maxfd < 0) {usleep(m_selectus); continue;}
        if ((ret = select(maxfd+1, &rset, NULL, NULL, &tval)) > 0) {
            for (i=0; i<j; i++) {
                pSock = m_socks[i];
                if (FD_ISSET(pSock->sockfd, &rset)) { 
                    TAILQ_LOCK(&m_tq);
                    TAILQ_REMOVE(&m_tq, pSock, entry);
                    TAILQ_UNLOCK(&m_tq);

                    /*写入可读套接字队列*/
                    TAILQ_LOCK(&m_sq);
                    TAILQ_INSERT_TAIL(&m_sq, pSock, entry);
                    TAILQ_UNLOCK(&m_sq);
                    TAILQ_SIGNAL(&m_sq);
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
    if (tpGetParamString(m_tpTask.params, "bindAddr", m_bindAddr) != 0) strcpy(m_bindAddr, "localhost");
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

    if (tpGetParamInt(m_tpTask.params, "selectus", &m_selectus) != 0) m_selectus = 100;
    LOG_MSG("参数[selectus]=[%d]", m_selectus);

    if (tpGetParamInt(m_tpTask.params, "nMaxThds", &m_nMaxThds) != 0) m_nMaxThds = 5;
    LOG_MSG("参数[nMaxThds]=[%d]", m_nMaxThds);

    return 0;
}

/***************************************************************
 * 请求处理线程
 **************************************************************/
void *tpRequestThread(void *arg)
{
    int     ret,sockfd;
    char    logPrefix[21];
    MQHDL   *mqh;
    TPMSG   tpMsg;
    time_t  t;
    struct  sock_info *pSock;
    /* 2014/12/24 dc 修改长度 for Base64编码报文 */
    char    msgbuf[MAXMSGLEN/3*4+12];        /*add by dwxiong 2014-11-7 19:43:05 @直销银行.互联网收单*/

    char    b64msg[MAXMSGLEN/3*4];
    int     b64len;

    /*绑定队列*/
    if ((ret = mq_bind(m_tpTask.bindQ, &mqh)) != 0) {
        LOG_ERR("mq_bind() error: retcode=%d", ret);
        LOG_ERR("绑定队列[%d]出错", m_tpTask.bindQ);
        tpThreadExit();
        return NULL;
    }

    while (1) {
        /*接收TPBUS请求*/
        if (tpAccept(mqh, &tpMsg) != 0) {
            LOG_ERR("接收TPBUS请求出错");
            mq_detach(mqh);
            tpThreadExit();
            return NULL;
        }
        tpSetLogPrefix(tpMsg.head.tpId, logPrefix);
        LOG_MSG("收到TPBUS请求");
        if (hasLogLevel(DEBUG)) tpHeadDebug(&tpMsg.head);
        if (hasLogLevel(DEBUG)) LOG_HEX(tpMsg.body, tpMsg.head.bodyLen);

        /*初始化通讯连接*/
        if ((sockfd = tcp_connect(m_bindAddr, m_peerAddr, m_peerPort, 0)) == -1) {
            LOG_ERR("tcp_connect(%s,%d) error", m_peerAddr, m_peerPort);
            LOG_ERR("连接服务端[%s:%d]出错", m_peerAddr, m_peerPort);
            continue;
        }
        LOG_MSG("连接服务端[%s:%d]...[ok]", m_peerAddr, m_peerPort);
        set_sock_linger(sockfd, 1, 1);

        /*发送服务端请求报文*/
        /*if (tcp_send(sockfd, b64msg, b64len, m_lenlen, 0) == -1) {
            LOG_ERR("发送服务端请求报文出错");
            close(sockfd);
            continue;
        }*/

        /* 2014/12/24 add by dc */
        /* 明文转Base64密文 */
        LOG_MSG("明文->Base64密文");
        b64len = Base64Encode(b64msg, tpMsg.body, tpMsg.head.bodyLen);
        if (b64len == -1)
        {
            LOG_ERR("Base64编码出错");
            close(sockfd);
            continue;
        }
        /*add by dwxiong 2014-11-7 19:43:05 @直销银行.互联网收单  bgn*/
        /* Base64编码修改 */
        snprintf(msgbuf, sizeof(msgbuf), "%*.*d", m_lenlen, m_lenlen, b64len);
        memcpy(msgbuf+m_lenlen,b64msg,b64len+1);
        /* msgbuf[MAXMSGLEN-1] = '\0'; */
        if ((ret = tcp_send(sockfd, msgbuf, b64len+m_lenlen, -1, 0)) == -1) {
            LOG_ERR("向服务端发送报文出错");
            close(sockfd);
            continue;
        }

        /*add by dwxiong 2014-11-7 19:43:05 @直销银行.互联网收单  end*/
        
        if (hasLogLevel(DEBUG)) LOG_HEX(msgbuf, tpMsg.head.bodyLen+m_lenlen);
        
        LOG_MSG("发送服务端请求报文...[ok]");

        /*申请套接字资源*/
        if (!(pSock = (struct sock_info*)malloc(sizeof(struct sock_info)))) {
            LOG_ERR("malloc() error: %s", strerror(errno));
            LOG_ERR("申请套接字资源出错");
            close(sockfd);
            sleep(1);
            continue;
        }
        memset(pSock, 0, sizeof(struct sock_info));

        /*写入请求套接字队列*/
        time(&t);
        TAILQ_LOCK(&m_tq);
        pSock->sockfd = sockfd;
        pSock->time = t;
        memcpy(&pSock->head, &tpMsg.head, sizeof(TPHEAD));
        TAILQ_INSERT_TAIL(&m_tq, pSock, entry);
        TAILQ_SIGNAL(&m_tq);
        TAILQ_UNLOCK(&m_tq);
    }
}

/***************************************************************
 * 响应处理线程
 **************************************************************/
void *tpResponseThread(void *arg)
{
    int ret;
    char logPrefix[21];
    MQHDL *mqh;
    TPMSG tpMsg;
    struct sock_info *pSock;

    char    b64msg[MAXMSGLEN/3*4];
    int     b64len;

    /*绑定队列*/
    if ((ret = mq_bind(m_tpTask.bindQ, &mqh)) != 0) {
        LOG_ERR("mq_bind() error: retcode=%d", ret);
        LOG_ERR("绑定队列[%d]出错", m_tpTask.bindQ);
        tpThreadExit();
        return NULL;        
    }

    while (1) {
        /*提取可读套接字*/
        TAILQ_LOCK(&m_sq);
        while (TAILQ_EMPTY(&m_sq)) TAILQ_WAIT(&m_sq);
        if (TAILQ_EMPTY(&m_sq)) {
            TAILQ_UNLOCK(&m_sq);
            continue;
        }
        pSock = TAILQ_FIRST(&m_sq);
        TAILQ_REMOVE(&m_sq, pSock, entry);
        TAILQ_UNLOCK(&m_sq);

        memcpy(&tpMsg.head, &pSock->head, sizeof(TPHEAD));
        tpSetLogPrefix(tpMsg.head.tpId, logPrefix);
        
        /*接收服务端返回报文*/
        /* 2014/12/24 add by dc */
        /* Base64密文转明文 */
        if ((b64len = tcp_recv(pSock->sockfd, b64msg, 0, m_lenlen, 0)) == -1) {
            LOG_ERR("tcp_recv() error: %s", strerror(errno));
            LOG_ERR("接收服务端返回报文出错");
        } else {
            LOG_MSG("接收服务端返回报文...[ok]");
            if (hasLogLevel(DEBUG)) LOG_HEX(b64msg, b64len);

            LOG_MSG("Base64密文->明文");
            tpMsg.head.bodyLen = Base64Decode(tpMsg.body, b64msg, b64len);
            if (tpMsg.head.bodyLen == -1)
            {
                LOG_ERR("Base64解码出错");
                continue;
            }
            if (hasLogLevel(DEBUG)) LOG_HEX(tpMsg.body, tpMsg.head.bodyLen);

            /*返回TPBUS响应*/
            if (tpReturn(mqh, &tpMsg) != 0) {
               LOG_ERR("返回TPBUS响应出错");
            } else {
               LOG_MSG("返回TPBUS响应...[ok]");
            }
        }
        close(pSock->sockfd);
        free(pSock);
    }
}

