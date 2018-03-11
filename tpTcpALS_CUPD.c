/**************************************************************
 * TCPͨѶͬ�������ӷ���˳���
 *************************************************************/
#include "tpbus.h"

TPTASK *m_ptpTask;

char m_bindAddr[IPADDRLEN+1];  /*�����IP��ַ*/
int  m_bindPort;               /*����������˿�*/
int  m_lenlen;                 /*ͨѶͷ����*/
int  m_timeout;                /*��ʱʱ��*/
int  m_nMaxThds;               /*��󲢷���*/
int  m_isoQ;                   /*ISO�������Ͷ�Ӧ����*/
int  m_nvlQ;                   /*NVL�������Ͷ�Ӧ����*/

MQHDL *mqh=NULL;
int  sockfd, m_csock;

int   tpParamInit();
void  tpQuit(int);

/**************************************************************
 * ������ 
 *************************************************************/
int main(int argc, char *argv[])
{
    int  ret;
    int  peerPort;
    char peerAddr[IPADDRLEN+1];
    int  msgLen;
    char logPrefix[21];
    char clazz[MSGCLZLEN+1];
    char logfname[200];
    time_t t;
    TPMSG tpMsg;
    char info_type[3+1];
    int  qid;
    
    /*��������в���*/
    if (argc != 2) {
        printf("Usage: %s <����ID>\n", argv[0]);
        return -1;
    }

    /*��ʼ������*/
    sprintf(logfname, "tpTcpALS_CUPD_%s",argv[1]);
    if (tpTaskInit(argv[1], logfname, &m_ptpTask) != 0) {
        printf("��ʼ�����̳���\n");
        return -1;
    }

    /*��ʼ�����ò���*/
    if (tpParamInit() != 0) {
        LOG_ERR("��ʼ�����ò�������");
        return -1;
    }
   
    /*�����˳��ź�*/
    signal(SIGTERM, tpQuit);
  
    /*��ʼ������*/
    if ((ret = mq_init()) != 0) {
        LOG_ERR("mq_init() error: retcode=%d", ret);
        LOG_ERR("��ʼ�����г���");
        return -1;
    }
    
    /*�󶨶���*/
    if ((ret = mq_bind(m_ptpTask->bindQ, &mqh)) != 0) {
        LOG_ERR("mq_bind() error: retcode=%d", ret);
        LOG_ERR("�󶨶���[%d]����", m_ptpTask->bindQ);
        return -1;
    }

    /*��ʼ�������˿�*/
    if ((sockfd = tcp_listen(m_bindAddr, m_bindPort)) == -1) {
        LOG_ERR("��ʼ�������˿�[%s:%d]����", m_bindAddr, m_bindPort);
        return -1;
    }

    for (;;){
        if ((m_csock = tcp_accept(sockfd, peerAddr, &peerPort)) == -1) {
            LOG_ERR("���տͻ��������������");
            close(sockfd);
            if (mqh) mq_detach(mqh);
            return -1;
        }
        
        LOG_MSG("���ܿͻ���[%s:%d]����������", peerAddr, peerPort);
        
        /*ͨѶ����*/
        if (set_sock_linger(sockfd, 1, 1) == -1) {
            LOG_ERR("�������ӹرն���ʱ�����");
            close(m_csock);
            close(sockfd);
            if (mqh) mq_detach(mqh);
            return -1;
        }
        
        for (;;){
            /*���ձ���*/
            memset(&tpMsg, 0x00, sizeof(tpMsg));
            if ((msgLen = tcp_recv_len(m_csock, tpMsg.body, 0, m_lenlen, 0)) <= 0) {
                LOG_ERR("tcp_recv() error: %s", strerror(errno));
                LOG_ERR("���տͻ��˱��ĳ���");
                close(m_csock);
                break;
            }
            LOG_MSG("�յ��ͻ��˷����ı���");
            
            if (hasLogLevel(DEBUG)) LOG_HEX(tpMsg.body, msgLen);
            if (msgLen == 4)  continue; /*��������*/
            
            /*�жϱ�������*/
            memcpy(info_type, tpMsg.body+69, 3);
            info_type[3] = 0x0;
            
            if (memcmp(info_type, "ISO", 3) == 0)
                qid = m_isoQ;
            else
                qid = m_nvlQ;
            
            /*д����*/
            snprintf(clazz, sizeof(clazz), "%d", tpMsg.head.tpId); 
            msgLen += sizeof(TPHEAD);
            if ((ret = mq_post(mqh, NULL, qid, 0, clazz, (char *)&tpMsg, msgLen, 0)) != 0) {
                LOG_ERR("mq_post() error: retcode=%d", ret);
                LOG_ERR("дǰ�ö���[%d]����", qid);
                continue;
            }
            LOG_MSG("дǰ�ö���[%d]...[ok]", qid);
        }  
    }  
}

/***************************************************************
 * ��ʼ�����ò���
 **************************************************************/
int tpParamInit()
{
    if (tpGetParamString(m_ptpTask->params, "bindAddr", m_bindAddr) != 0) {
        LOG_ERR("��ȡ����[bindAddr]����");
        return -1;
    }
    LOG_MSG("����[bindAddr]=[%s]", m_bindAddr);

    if (tpGetParamInt(m_ptpTask->params, "bindPort", &m_bindPort) != 0) {
        LOG_ERR("��ȡ����[bindPort]����");
        return -1;
    }
    LOG_MSG("����[bindPort]=[%d]", m_bindPort);
  
    if (tpGetParamInt(m_ptpTask->params, "lenlen", &m_lenlen) != 0) m_lenlen = 4;
    LOG_MSG("����[lenlen]=[%d]", m_lenlen);

    if (tpGetParamInt(m_ptpTask->params, "timeout", &m_timeout) != 0) m_timeout = 60;
    LOG_MSG("����[timeout]=[%d]", m_timeout);
 
    if (tpGetParamInt(m_ptpTask->params, "isoQ", &m_isoQ) != 0) m_isoQ = 22;
    LOG_MSG("����[isoQ]=[%d]", m_isoQ);
    
    if (tpGetParamInt(m_ptpTask->params, "nvlQ", &m_nvlQ) != 0) m_nvlQ = 21;
    LOG_MSG("����[nvlQ]=[%d]", m_nvlQ);
        
    return 0;
}

/******************************************************************************
 * ����: tpQuit
 * ����: �����˳�����
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
 * ������Ϣ����(��ָ��Ԥ������,���ر��Ĵ�����ֵ)
 ****************************************************************/
int tcp_recv_len(int sockfd, void *buf, int len, int lenlen, int timeout)
{
    char lenbuf[21];
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
            /* ���÷�����ģʽ */
            while (nsecs > 0) {
                if (readable(sockfd, 1)) {
                    if ((nread = read(sockfd, buf, SORCVBUFSZ)) == -1) {
                        //if (errno == EINTR) continue;
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
