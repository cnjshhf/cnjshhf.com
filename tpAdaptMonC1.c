/**************************************************************
 * TCPͨѶͬ�������ӿͻ��˳���
 *************************************************************/
#include "tpbus.h"

TPTASK m_tpTask;  /*��������*/

char m_bindAddr[IPADDRLEN+1];  /*���ض�IP��ַ*/
char m_peerAddr[IPADDRLEN+1];  /*�����IP��ַ*/
int  m_peerPort;               /*����������˿�*/
int  m_lenlen;                 /*ͨѶͷ����*/
int  m_timeout;                /*��ʱʱ��*/
int  m_nMaxThds;               /*��󲢳���*/
int  m_sndflag; 

typedef struct {
    char msglen[4];
    char ctpdesc[15];
    char trancode[4];
    char rspno[6];
    char bankid[4];
    char transrc[2];
    char unitno[6];
    char teller[6];
    char transeq[6];
    char idno[19];
    char tranname[30];
    char tranamt[12];
}TranMonMsg;

int   tpParamInit();
void *tpProcessThread(void *arg);
void setMonMsg(char *, int, TranMonMsg *);

/**************************************************************
 * ������ 
 *************************************************************/
int main(int argc, char *argv[])
{
    int i,ret;

    /*��������в���*/
    if (argc != 2) {
        printf("Usage: %s <����ID>\n", argv[0]);
        return -1;
    }

    /*��ʼ������*/
    if (tpTaskInit(atoi(argv[1]), "tpTcpC", &m_tpTask) != 0) {
        printf("��ʼ�����̳���");
        return -1;
    }

    /*��ʼ�����ò���*/
    if (tpParamInit() != 0) {
        LOG_ERR("��ʼ�����ò�������");
        return -1;
    }

    /*��ʼ������*/
    if ((ret = mq_init()) != 0) {
        LOG_ERR("mq_init() error: retcode=%d", ret);
        LOG_ERR("��ʼ�����г���");
        return -1;
    }

    /*���������̳߳�*/
    for (i=0; i<m_nMaxThds; i++) {
        if (tpThreadCreate(tpProcessThread, NULL) != 0) {
            LOG_ERR("���������߳�ʧ��: %s", strerror(errno));
            return -1;
        }
    }

    while (1) sleep(1);
}

/***************************************************************
 * ��ʼ�����ò���
 **************************************************************/
int tpParamInit()
{
    if (tpGetParamString(m_tpTask.params, "bindAddr", m_bindAddr) != 0) strcpy(m_bindAddr, "localhost");
    LOG_MSG("����[bindAddr]=[%s]", m_bindAddr);

    if (tpGetParamString(m_tpTask.params, "peerAddr", m_peerAddr) != 0) {
        LOG_ERR("��ȡ����[peerAddr]����");
        return -1;
    }
    LOG_MSG("����[peerAddr]=[%s]", m_peerAddr);

    if (tpGetParamInt(m_tpTask.params, "peerPort", &m_peerPort) != 0) {
        LOG_ERR("��ȡ����[peerPort]����");
        return -1;
    }
    LOG_MSG("����[peerPort]=[%d]", m_peerPort);
  
    if (tpGetParamInt(m_tpTask.params, "lenlen", &m_lenlen) != 0) m_lenlen = 4;
    LOG_MSG("����[lenlen]=[%d]", m_lenlen);

    if (tpGetParamInt(m_tpTask.params, "timeout", &m_timeout) != 0) m_timeout = 60;
    LOG_MSG("����[timeout]=[%d]", m_timeout);

    if (tpGetParamInt(m_tpTask.params, "nMaxThds", &m_nMaxThds) != 0) m_nMaxThds = 5;
    LOG_MSG("����[nMaxThds]=[%d]", m_nMaxThds);

    if (tpGetParamInt(m_tpTask.params, "sndflag", &m_sndflag) != 0) m_sndflag = 1;
    LOG_MSG("����[sndflag]=[%d]", m_sndflag);

    return 0;
}

/***************************************************************
 * �����߳�
 **************************************************************/
void *tpProcessThread(void *arg)
{
    int ret,sockfd = -1;
    int q,msgLen;
    char logPrefix[21];
    MQHDL *mqh = NULL;
    TranMonMsg monMsg;
    char  msgbuf[1024*32];

    /*�󶨶���*/
    if ((ret = mq_bind(m_tpTask.bindQ, &mqh)) != 0) {
        LOG_ERR("mq_bind() error: retcode=%d", ret);
        LOG_ERR("�󶨶���[%d]����", m_tpTask.bindQ);
        goto THD_END;
    }

    while (1) { 
        msgLen = sizeof(TPMSG);
        if ((ret = mq_pend(mqh, NULL, &q, NULL, NULL, (char *)msgbuf, &msgLen, 0)) != 0) {
            LOG_ERR("mq_pend() error: retcode=%d", ret);
            LOG_ERR("���󶨶���[%d]����", mqh->qid);
            goto THD_END;
        }
 
        LOG_MSG("�յ�[%d]�����ı���",q);
        if (hasLogLevel(DEBUG)) LOG_HEX(msgbuf, msgLen);

		if( !m_sndflag ) continue;

        setMonMsg(msgbuf+sizeof(TPHEAD), msgLen-sizeof(TPHEAD), &monMsg);

        /*ͨѶ����*/
        if ((sockfd = tcp_connect(m_bindAddr, m_peerAddr, m_peerPort, 0)) == -1) {
            LOG_ERR("tcp_connect(%s,%d) error", m_peerAddr, m_peerPort);
            LOG_ERR("��ʼ��ͨѶ���ӳ���");
            continue;
        }
        LOG_MSG("����������[%s:%d]��ͨѶ����...[ok]", m_peerAddr, m_peerPort);
 
        /*ͨѶ����*/
        if (set_sock_linger(sockfd, 1, 1) == -1) {
            LOG_ERR("�������ӹرն���ʱ�����");
            continue;
        }

        /*���ͱ���*/
        if ((ret = tcp_send(sockfd, (char *)&monMsg, sizeof(TranMonMsg)-12, 0, 0)) == -1) {
            LOG_ERR("�����˷��ͱ��ĳ���");
            continue;
        }
        LOG_MSG("���ͱ��ĵ������...[ok]");
        if (hasLogLevel(DEBUG)) LOG_HEX((char *)&monMsg, sizeof(TranMonMsg)-12);

        if (sockfd != -1) close(sockfd);
    }

THD_END:
    if (mqh) mq_detach(mqh);
    if (sockfd != -1) close(sockfd);
    tpThreadExit();
    return NULL;
}

void setMonMsg(char *msgbuf, int msglen, TranMonMsg *monMsg)
{
    char  tran_no[37];
    char  rsp_no[5];
    char  id_no[20];
    char  hiscard[30];

    if (msglen < 297) {
        LOG_ERR("���ĳ���[%d]�Ƿ���С�ڱ���ͷ����.",msglen);
        return;      
    }

    memset(monMsg, 0x20, sizeof(TranMonMsg));

    memcpy(monMsg->msglen, "0083", 4); 
    memcpy(monMsg->ctpdesc,"CUPSK08",7);
    memcpy(monMsg->ctpdesc+7,msgbuf+260,3);
    memcpy(monMsg->trancode,msgbuf,4);
    if (monMsg->trancode[3] == '1') monMsg->trancode[3] = '0';
    if (monMsg->trancode[3] == '6') monMsg->trancode[3] = '5';

    memcpy(rsp_no,msgbuf+193,4);
    rsp_no[4] = 0;
    if (rsp_no[0] == 0x20) {
        monMsg->ctpdesc[14] = 'R';
    }
    else {
        monMsg->ctpdesc[14] = 'S';
        memcpy(monMsg->rspno,"000000",6);
    }

    memcpy(monMsg->bankid,msgbuf+259,4);
/* del by xujun 20140211
    if (memcmp(monMsg->bankid,"0103",4) == 0) {
        monMsg->bankid[0] = '8';
    }
*/
    memcpy(monMsg->bankid,"8005",4);   
    memcpy(monMsg->unitno,msgbuf+48,6);
    memcpy(monMsg->teller,msgbuf+42,6);
    memcpy(tran_no,msgbuf+56,36);
    tran_no[36] = 0;
    strTrim(tran_no);
    memcpy(monMsg->transeq,tran_no+(strlen(tran_no)-6),6);
    memcpy(monMsg->tranname,msgbuf+4,30);

    if (rsp_no[0] == 0x20) {
        memset(id_no, 0x00, sizeof(id_no));
        memset(hiscard, 0x00, sizeof(hiscard));
        memcpy(id_no,msgbuf+299,20);
        memcpy(hiscard,msgbuf+319,30);
        strTrim(id_no);
        strTrim(hiscard);
  
        if (strlen(hiscard)) {
            memcpy(monMsg->idno,id_no,19);
        }
        else {
            memcpy(monMsg->idno,hiscard,19);
        }
        if ( (memcmp(monMsg->trancode,"2000",4) == 0) ||
             (memcmp(monMsg->trancode,"3000",4) == 0) ||
             (memcmp(monMsg->trancode,"6000",4) == 0) ) {
           memcpy(monMsg->tranamt,msgbuf+349,12); 
        }
    }
    else {
        if ( (memcmp(monMsg->trancode,"2000",4) == 0) ||
             (memcmp(monMsg->trancode,"3000",4) == 0) ||
             (memcmp(monMsg->trancode,"3005",4) == 0) ||
             (memcmp(monMsg->trancode,"3010",4) == 0) ||
             (memcmp(monMsg->trancode,"4200",4) == 0) ||
             (memcmp(monMsg->trancode,"6000",4) == 0) ) {
           memcpy(monMsg->tranamt,msgbuf+297,12); 
        }
    }

    return;
}
