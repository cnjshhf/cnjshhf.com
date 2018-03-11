/**************************************************************
 * TCP通讯同步短连接客户端程序
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
int  m_srcQ;                   /*目标队列*/

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
    if (tpTaskInit(atoi(argv[1]), "tpTcpC", &m_tpTask) != 0) {
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

    if (tpGetParamInt(m_tpTask.params, "srcQ", &m_srcQ) != 0) m_srcQ = Q_SVR;
    LOG_MSG("参数[srcQ]=[%d]", m_srcQ);

    return 0;
}
/***************************************************************
 * 接收服务响应(用于请求端通讯程序)
 **************************************************************/
int tpRecv_N(MQHDL *mqh, TPMSG *ptpMsg)
{
    int  q,ret,msgLen;
    char clazz[MSGCLZLEN+1];

    msgLen = sizeof(TPMSG);
    if ((ret = mq_pend(mqh, NULL, &q, NULL, clazz, (char *)ptpMsg, &msgLen, 0)) != 0) {
        LOG_ERR("mq_pend() error: retcode=%d", ret);
        LOG_ERR("读绑定队列[%d]出错", mqh->qid);
        return -1;
    }
    if (q == m_srcQ) {
        LOG_MSG("收到来自主控程序的报文");
    }
    else if(q == Q_REV)
    {
        LOG_MSG("收到来自事务程序的报文");
    }
    else {
        LOG_ERR("收到来自队列[%d]的非法报文", q);
        return -1;
    }

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
    char    msgbuf[MAXMSGLEN];        /*add by dwxiong 2014-11-7 19:43:05 @直销银行.互联网收单*/

    /*绑定队列*/
    if ((ret = mq_bind(m_tpTask.bindQ, &mqh)) != 0) {
        LOG_ERR("mq_bind() error: retcode=%d", ret);
        LOG_ERR("绑定队列[%d]出错", m_tpTask.bindQ);
        tpThreadExit();
        return NULL;
    }

    while (1) {
        /*接收TPBUS请求*/
        if (tpRecv_N(mqh, &tpMsg) != 0) {
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
        /*if (tcp_send(sockfd, tpMsg.body, tpMsg.head.bodyLen, m_lenlen, 0) == -1) {
            LOG_ERR("发送服务端请求报文出错");
            close(sockfd);
            continue;
        }*/
        /*add by dwxiong 2014-11-7 19:43:05 @直销银行.互联网收单  bgn*/
        memset(msgbuf,0x00,sizeof(msgbuf));
        snprintf(msgbuf, sizeof(msgbuf), "%*.*d", m_lenlen, m_lenlen, tpMsg.head.bodyLen);
        memcpy(msgbuf+m_lenlen,tpMsg.body,tpMsg.head.bodyLen);
        msgbuf[MAXMSGLEN-1] = '\0';
        if ((ret = tcp_send(sockfd, msgbuf, tpMsg.head.bodyLen+m_lenlen, -1, 0)) == -1) {
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
        if ((tpMsg.head.bodyLen = tcp_recv(pSock->sockfd, tpMsg.body, 0, m_lenlen, 0)) == -1) {
            LOG_ERR("tcp_recv() error: %s", strerror(errno));
            LOG_ERR("接收服务端返回报文出错");
        } else {
            LOG_MSG("接收服务端返回报文...[ok]");
            if (hasLogLevel(DEBUG)) LOG_HEX(tpMsg.body, tpMsg.head.bodyLen);

            /*返回TPBUS响应*/
            if (tpReturn1(mqh, &tpMsg, m_srcQ) != 0) {
               LOG_ERR("返回TPBUS响应出错");
            } else {
               LOG_MSG("返回TPBUS响应...[ok]");
            }
        }
        close(pSock->sockfd);
        free(pSock);
    }
}
/***************************************************************
 * 返回服务响应(用于服务端通讯程序)
 **************************************************************/
int tpReturn1(MQHDL *mqh, TPMSG *ptpMsg, int desQ)
{
    char *rqm;
    int i,j,ret,msgLen;

    msgLen = ptpMsg->head.bodyLen + sizeof(TPHEAD);

    if (ptpMsg->head.tpId) {
        i = (ptpMsg->head.tpId/100000000)%10;
        rqm = (i==0)? NULL : gShm.ptpConf->hosts[i-1].qm;
        if ((ret = mq_post(mqh, rqm, desQ, 0, NULL, (char *)ptpMsg, msgLen, 0)) != 0) {
            LOG_ERR("mq_post() error: retcode=%d", ret);
            LOG_ERR("写主控队列[%d]出错", desQ);
            return -1;
        }
    } else {
        if (!gShm.ptpConf->dispatch[0]) { /*非集群模式*/
            if ((ret = mq_post(mqh, NULL, desQ, 0, NULL, (char *)ptpMsg, msgLen, 0)) != 0) {
                LOG_ERR("mq_post() error: retcode=%d", ret);
                LOG_ERR("写主控队列[%d]出错", desQ);
                return -1;
            }
        } else { /*集群模式*/
            for (i=0,j=0; i<NMAXHOSTS; i++) {
                if (!gShm.ptpConf->hosts[i].id) continue;
                if (!gShm.ptpConf->hosts[i].status) continue;
                rqm = gShm.ptpConf->hosts[i].qm;
                if ((ret = mq_post(mqh, rqm, desQ, 0, NULL, (char *)ptpMsg, msgLen, 0)) != 0) {
                    LOG_ERR("mq_post() error: retcode=%d", ret);
                    LOG_ERR("写主控队列[%d]出错", desQ);
                    return -1;
                }
                j++;
            }
            if (j==0) {
                LOG_ERR("没有找到可用的服务器");
                return -1;
            }
        }
    }
    LOG_MSG("写主控队列[%d]...[ok]", desQ);
    return 0;
}

