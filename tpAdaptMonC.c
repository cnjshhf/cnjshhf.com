/**************************************************************
 * ���׼�ط��ͳ���
 * create by dwxiong 2014-12-10 20:33:13
 *************************************************************/
#include "tpbus.h"

#define QUEUE_APP_SYS  31
/*connect timeout*/
#define ESBERR         "999999"

typedef struct {
    char msglen[4];
    char ctpdesc[15];
    char trancode[4];
    char rspno[6];
    char bankid[4];
    char source[2];
    char brnno[6];
    char oprno[6];
    char transeq[6];
    char cardno[19];
}TranMonMsg;

typedef struct {
    char acqinst[8+1];
    char bankid[4+1];
}BankMap;


TPTASK  m_tpTask;  /*��������*/
int     banknum=2;
BankMap bankmap[]={{"04615511","3004"},{"64403600","3004"},{"        ","    "}};

char m_bindAddr[IPADDRLEN+1];  /*���ض�IP��ַ*/
char m_peerAddr[IPADDRLEN+1];  /*�����IP��ַ*/
int  m_peerPort;               /*����������˿�*/
int  m_lenlen;                 /*ͨѶͷ����*/
int  m_timeout;                /*��ʱʱ��*/
int  m_nMaxThds;               /*��󲢳���*/
int  m_sndflag; 


int     tpParamInit();
void    *tpProcessThread(void *arg);
int     valGet(char *src, char *targetL, char *targetR, char *result);
int     setMonMsg(TPMSG *tpMsg, int src_Q, TranMonMsg *monMsg);
void    Trim(char *s);

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
    if (tpTaskInit(atoi(argv[1]), "tpAdaptMonC", &m_tpTask) != 0) {
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

    if (tpGetParamInt(m_tpTask.params, "nMaxThds", &m_nMaxThds) != 0) m_nMaxThds = 1;/*mod by dwxiong */
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
    int     ret,sockfd = -1;
    int     q,msgLen;
    char    logPrefix[21];
    MQHDL   *mqh = NULL;
    TranMonMsg monMsg;
    TPMSG   tpMsg;

    /*�󶨶���*/
    if ((ret = mq_bind(m_tpTask.bindQ, &mqh)) != 0) {
        LOG_ERR("mq_bind() error: retcode=%d", ret);
        LOG_ERR("�󶨶���[%d]����", m_tpTask.bindQ);
        goto THD_END;
    }

    while (1) { 
        msgLen = sizeof(TPMSG);
        if ((ret = mq_pend(mqh, NULL, &q, NULL, NULL, (char *)&tpMsg, &msgLen, 0)) != 0) {
            LOG_ERR("mq_pend() error: retcode=%d", ret);
            LOG_ERR("���󶨶���[%d]����", mqh->qid);
            goto THD_END;
        }
 
        LOG_MSG("�յ�[%d]�����ı���[%d][%d]",q,msgLen,tpMsg.head.bodyLen);
        tpMsg.head.orgQ = q;
        
        /*
        if (hasLogLevel(DEBUG)) 
            LOG_HEX(tpMsg.body, tpMsg.head.bodyLen);
        */
        /*  del by dwxiong 2014-9-3 17:20:22*/
		if( !m_sndflag ) continue;

        memset(&monMsg,      0x20, sizeof(TranMonMsg));
        
        if(q == QUEUE_APP_SYS)
        {
            if ( ret=setMonMsg(&tpMsg,q, &monMsg) != 0)
                continue;
        }
        else
        {
            LOG_MSG("����յ�[%d]���б���...[ignore]",tpMsg.head.orgQ);
            continue;
        }
        

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
        if ((ret = tcp_send(sockfd, (char *)&monMsg, sizeof(TranMonMsg), 0, 0)) == -1) {
            LOG_ERR("�����˷��ͱ��ĳ���");
            continue;
        }
        LOG_MSG("���ͱ��ĵ������...[ok]");
        if (hasLogLevel(DEBUG)) LOG_HEX((char *)&monMsg, sizeof(TranMonMsg));

        if (sockfd != -1) close(sockfd);
    }

THD_END:
    if (mqh) mq_detach(mqh);
    if (sockfd != -1) close(sockfd);
    tpThreadExit();
    return NULL;
}

int setMonMsg(TPMSG *tpMsg, int src_Q, TranMonMsg *monMsg)
{ 
    int   ret;
    char  trxcode       [4+1];
    char  esb_retcode   [8+1];
    char  netacq_respcode[6+1];
    char  card_FLD39[2+1];
    char  banknbr   [4+1];
    char  source    [2+1];
    char  brnno     [6+1];
    char  openo     [6+1];
    char  seqno     [6+1];
    char  pan       [19+1];
    char  date      [8+1];
    char  time      [8+1];
    
    ret = 0;
    memset(trxcode,     0x00, sizeof(trxcode));
    memset(esb_retcode, 0x00, sizeof(esb_retcode));
    memset(netacq_respcode,      0x20, sizeof(netacq_respcode)-1);
    memset(card_FLD39,  0x00, sizeof(card_FLD39));
    memset(banknbr,     0x00, sizeof(banknbr));
    memset(source,      0x00, sizeof(source));
    memset(brnno,       0x00, sizeof(brnno));
    memset(openo,       0x00, sizeof(openo));
    memset(seqno,       0x00, sizeof(seqno));
    memset(pan,         0x00, sizeof(pan));
    memset(date,        0x00, sizeof(date));
    memset(time,        0x00, sizeof(time));
    
    valGet(tpMsg->body, "<TRXCODE>",     "</TRXCODE>",       trxcode);
    valGet(tpMsg->body, "<TRXDATE>",     "</TRXDATE>",       date);
    valGet(tpMsg->body, "<TRXTIME>",     "</TRXTIME>",       time);
    
    LOG_MSG("setMonMsg TRXCODE[%s][%d]",trxcode,atoi(trxcode));
    
    Trim(date);
    Trim(time);
    
    /* if(src_Q == QUEUE_APP_SYS) */    /*Ĭ��ΪAPP����*/
    memcpy(monMsg->source,"OS",2);
    memcpy(monMsg->msglen,"0053",4);
    memcpy(monMsg->ctpdesc,"DBESB0",6);
    memcpy(monMsg->oprno,date+2,6);
    memcpy(monMsg->brnno,time,6);
    
    chgTranCode(trxcode,monMsg);
    
    switch(atoi(trxcode))
    {
        case 3000:
            ret = setMonMsg_netacq(tpMsg, monMsg);
            if(ret != 0)    return ret;
            break;
        case 2501:
        case 7601:
            ret = setMonMsg_card8583(tpMsg, monMsg,atoi(trxcode));
            if(ret != 0)    return ret;
            break;
        case 5628:
        case 5630:
            ret = setMonMsg_cardnvl(tpMsg, monMsg);
            if(ret != 0)    return ret;
            break;
        case 6001:
        case 5514:
        case 5608:
        case 5620:
        case 5626:
        case 5631:
        default:
            return -1;
    }
    
    LOG_MSG("setMonMsg ok monMsg.ctpdesc[%15.15s]",monMsg->ctpdesc);
    LOG_MSG("setMonMsg ok monMsg.bankid [%4.4s]"  ,monMsg->bankid);
    LOG_MSG("setMonMsg ok monMsg.transeq[%6.6s]"  ,monMsg->transeq);
    LOG_MSG("setMonMsg ok monMsg.rspno  [%6.6s]"  ,monMsg->rspno);
    LOG_MSG("setMonMsg ok monMsg.YYMMDD [%6.6s]"  ,monMsg->oprno);
    LOG_MSG("setMonMsg ok monMsg.hhmmss [%6.6s]"  ,monMsg->brnno);
    
    
    return 0;
}


int Acqinst2Bank(char *acqinst,char *bankidout)
{

    int i;
    for(i=0; i<banknum; i++) 
        if(memcmp(acqinst, bankmap[i].acqinst,8))
        {
            memcpy(bankidout, bankmap[i].bankid,4);
            return 0;
        }
    return i;
}

/***************************************************************
 * �ַ�������ת��Ϊ�����ʶ���4λ������
 * add by dwxiong @2014-12-9 16:56:07
 **************************************************************/
int chgTranCode(char *srcTranCode, TranMonMsg *monMsg)
{
    switch(atoi(srcTranCode))
    {        
        case 3000:/*ʵ����֤*/
            memcpy(monMsg->trancode,"6600",4);
            break;
        case 2501:/*�ʽ�ת��*/
            memcpy(monMsg->trancode,"6602",4);
            break;
        case 7601:/*�ʽ�ת��*/
            memcpy(monMsg->trancode,"6601",4);
            break;
        case 5628:/*��������*/
            memcpy(monMsg->trancode,"6603",4);
            break;
        case 5630:/*��ƿ�����*/
            memcpy(monMsg->trancode,"6604",4);
            break;
        case 6001:
        case 5514:
        case 5608:
        case 5620:
        case 5626:
        case 5631:
        default:
            break;
    }
    return 0;
}

/***************************************************************
 * �������յ�.ʵ����֤����ת�����
 * add by dwxiong @2014-12-9 16:56:07
 **************************************************************/
int setMonMsg_netacq(TPMSG *tpMsg, TranMonMsg *monMsg)
{
    int   ret;
    char  esb_retcode   [8+1];
    char  netacq_respcode[6+1];
    char  acqinst   [8+1];
    char  banknbr   [4+1];
    char  orderid   [14+1];
    char  seqno     [6+1];
    char  pan       [19+1];
    
    memset(esb_retcode, 0x00, sizeof(esb_retcode));
    memset(netacq_respcode,      0x20, sizeof(netacq_respcode)-1);
    memset(acqinst,     0x00, sizeof(acqinst));
    memset(banknbr,     0x00, sizeof(banknbr));
    memset(orderid,     0x00, sizeof(orderid));
    memset(seqno,       0x00, sizeof(seqno));
    memset(pan,         0x00, sizeof(pan));
    
    valGet(tpMsg->body, "<RETCODE>",     "</RETCODE>",       esb_retcode);
    valGet(tpMsg->body, "<RESPCODE>",    "</RESPCODE>",      netacq_respcode);
    valGet(tpMsg->body, "<ACQINSCODE>",  "</ACQINSCODE>",    acqinst);
    valGet(tpMsg->body, "<ORDERID>",     "</ORDERID>",       orderid);
    valGet(tpMsg->body, "<ACCNO>",       "</ACCNO>",         pan);
    
    Trim(esb_retcode);
    Trim(pan);
    ret = Acqinst2Bank(acqinst,monMsg->bankid);
    if(ret < 0 )
        LOG_WRN("cannot find acqinst[%s], change to bankid erro[%d]",acqinst,ret);

    memcpy(monMsg->transeq,orderid+(strlen(orderid)-6),6);
    memcpy(monMsg->cardno ,pan,19);
    memcpy(monMsg->ctpdesc+6,monMsg->bankid,4);
    
    
    if(isSpace(esb_retcode,sizeof(esb_retcode)-1)==1)
    {       /*req*/
        memcpy(monMsg->ctpdesc+14 ,"S",1);
    }
    else    /*rsp*/
    {   
        memcpy(monMsg->ctpdesc+14 ,"R",1);
        /*Ӧ����������������ʶ2 byte*/
        chgRspCode_netacq(netacq_respcode,monMsg);
    }
	/* RETCODEӦ��������2 byte������ʶ */
    if(memcmp(esb_retcode+2, ESBERR , 6) == 0)
        memcpy(monMsg->rspno,"300001",6);
        
    return 0;
}

int setMonMsg_card8583(TPMSG *tpMsg, TranMonMsg *monMsg,int trxcode)
{
    char  esb_retcode   [8+1];
    char  netacq_respcode[6+1];
    char  card_FLD39[2+1];
    char  banknbr   [11+1];
    char  seqno     [6+1];
    char  pan       [19+1];

    memset(esb_retcode, 0x00, sizeof(esb_retcode));
    memset(netacq_respcode,      0x20, sizeof(netacq_respcode)-1);
    memset(card_FLD39,  0x00, sizeof(card_FLD39));
    memset(banknbr,     0x00, sizeof(banknbr));
    memset(seqno,       0x00, sizeof(seqno));
    
    valGet(tpMsg->body, "<RETCODE>",     "</RETCODE>",       esb_retcode);
    valGet(tpMsg->body, "<FLD39>",       "</FLD39>",         card_FLD39);
    valGet(tpMsg->body, "<RESPCODE>",    "</RESPCODE>",      netacq_respcode);
    valGet(tpMsg->body, "<FLD100>",      "</FLD100>",        banknbr);
    valGet(tpMsg->body, "<FLD11>",       "</FLD11>",         seqno);
    valGet(tpMsg->body, "<FLD2>",        "</FLD2>",          pan);
    
    Trim(esb_retcode);
    Trim(card_FLD39);
    Trim(netacq_respcode);
    Trim(banknbr);
    Trim(pan);

    memcpy(monMsg->bankid,banknbr,4);
    memcpy(monMsg->transeq,seqno ,6);
    memcpy(monMsg->cardno ,pan   ,19);
    memcpy(monMsg->ctpdesc+6,monMsg->bankid,4);
    
    if(isSpace(esb_retcode,sizeof(esb_retcode)-1)==1)
    {       /*req*/
        memcpy(monMsg->ctpdesc+14 ,"S",1);
    }
    else    /*rsp*/
    {
    
        memcpy(monMsg->ctpdesc+14 ,"R",1);

        LOG_MSG("trxcode[%d]netacq_rsp[%s]fld39[%s]esb_ret[%s]",trxcode,netacq_respcode,card_FLD39,esb_retcode);
        
        memcpy(monMsg->rspno,"000000",6);
        if(trxcode == 7601)
        {
			/* ת�뽻���ȶ���ƿ�ϵͳ���ٶϻ������յ�/���� */
            if(strcmp(card_FLD39,"00") != 0)
                chgRspCode_card8583(card_FLD39,monMsg);
            if(strcmp(netacq_respcode,"00") != 0 &&
			   strcmp(netacq_respcode,"000000") !=0 )
                chgRspCode_netacq(netacq_respcode,monMsg);
        }
        else if(trxcode == 2501)
        {
			/* ת�������ȶϻ������յ�/���ڣ��ٶ���ƿ�ϵͳ */
            if(strcmp(netacq_respcode,"00") != 0 &&
			   strcmp(netacq_respcode,"000000") != 0 )
                chgRspCode_netacq(netacq_respcode,monMsg);
            if(strcmp(card_FLD39,"00") != 0)
                chgRspCode_card8583(card_FLD39,monMsg);
        }
		/* RETCODEӦ��������2 byte������ʶ */
        if(memcmp(esb_retcode+2, ESBERR , 6) == 0)
                memcpy(monMsg->rspno,"300001",6);
    }
    return 0;
}


int setMonMsg_cardnvl(TPMSG *tpMsg, TranMonMsg *monMsg)
{
    char  retcode   [6+1];
    char  banknbr   [11+1];
    char  seqno     [6+1];
    char  pan       [19+1];

    memset(retcode ,    0x00, sizeof(retcode));
    memset(banknbr,     0x00, sizeof(banknbr));
    memset(seqno,       0x00, sizeof(seqno));
    memset(pan,         0x00, sizeof(pan));
    
    valGet(tpMsg->body, "<RETCODE>",     "</RETCODE>",       retcode);
    valGet(tpMsg->body, "<BNKNBR>",      "</BNKNBR>",        banknbr);
    valGet(tpMsg->body, "<SEQNO>",       "</SEQNO>",         seqno);
    valGet(tpMsg->body, "<CARDNBR>",     "</CARDNBR>",       pan);
    
    Trim(retcode);
    Trim(banknbr);
    Trim(seqno);
    Trim(pan);
    /*
    if(memcmp(retcode, ESBERR , 6) == 0)
    {   
        LOG_WRN(" request time out , not send to monSys");
        return -1;
    }
    */  
    memcpy(monMsg->bankid,banknbr,4);
    memcpy(monMsg->transeq,seqno ,6);
    memcpy(monMsg->cardno ,pan   ,19);
    memcpy(monMsg->ctpdesc+6,monMsg->bankid,4);
    
    if(isSpace(retcode,sizeof(retcode)-1)==1)
    {       /*req*/
        memcpy(monMsg->ctpdesc+14 ,"S",1);
    }
    else    /*rsp*/
    {
        memcpy(monMsg->ctpdesc+14 ,"R",1);
        /* chgRspCode_cardnvl(retcode,monMsg); */   /* Ӧ��������������־*/
        chgRspCode_cardnvl(retcode+2,monMsg);
   
    }
    
    return 0;
}

/***************************************************************
 * �������յ�Ӧ����ת��Ϊ�����ʶ���6λӦ����
 * add by dwxiong @2014-12-9 18:44:53
 **************************************************************/
int chgRspCode_netacq (char *srcRspCode,  TranMonMsg *monMsg)
{
    memcpy(monMsg->rspno,"300001",6);
    /* ���������ɹ�Ӧ��Ϊ000000 */
    if (strcmp(srcRspCode,"00") == 0 ||
		strcmp(srcRspCode,"000000") == 0)
        memcpy(monMsg->rspno,"000000",6);
    if(strcmp(srcRspCode,"10") == 0)  {   memcpy(monMsg->rspno,"300030",6); return 0; }
    if(strcmp(srcRspCode,"13") == 0)  {   memcpy(monMsg->rspno,"300076",6); return 0; }
    if(strcmp(srcRspCode,"61") == 0)  {   memcpy(monMsg->rspno,"300077",6); return 0; }
    if(strcmp(srcRspCode,"10000") == 0)  {memcpy(monMsg->rspno,"300078",6); return 0; }
}

/***************************************************************
 * 8583Ӧ����ת��Ϊ�����ʶ���6λӦ����
 * add by dwxiong @2014-12-9 18:44:53
 **************************************************************/
int chgRspCode_card8583 (char *srcRspCode,  TranMonMsg *monMsg)
{
    memcpy(monMsg->rspno,"300001",6);
    if (srcRspCode[0] == 0x30 && srcRspCode[1] == 0x30) 
        memcpy(monMsg->rspno,"000000",6);
    if(memcmp(srcRspCode,"12",2) == 0) { memcpy(monMsg->rspno,"300012",6); return 0; }
    if(memcmp(srcRspCode,"14",2) == 0) { memcpy(monMsg->rspno,"300014",6); return 0; }
    if(memcmp(srcRspCode,"25",2) == 0) { memcpy(monMsg->rspno,"300025",6); return 0; }
    if(memcmp(srcRspCode,"30",2) == 0) { memcpy(monMsg->rspno,"300030",6); return 0; }
    if(memcmp(srcRspCode,"99",2) == 0) { memcpy(monMsg->rspno,"300099",6); return 0; }
}

/***************************************************************
 * ��ϵͳ�̶���ʽӦ����ת��Ϊ�����ʶ���6λӦ����
 * add by dwxiong @2014-12-9 18:44:53
 **************************************************************/
int chgRspCode_cardnvl (char *srcRspCode,  TranMonMsg *monMsg)
{
    memcpy(monMsg->rspno,"300001",6);
    if (memcmp(srcRspCode,"000000",6) == 0) 
        memcpy(monMsg->rspno,"000000",6);
    if(memcmp(srcRspCode,"000014",6) == 0)  {   memcpy(monMsg->rspno,"300079",6); return 0; }
    if(memcmp(srcRspCode,"000063",6) == 0)  {   memcpy(monMsg->rspno,"300080",6); return 0; }
    if(memcmp(srcRspCode,"000642",6) == 0)  {   memcpy(monMsg->rspno,"300081",6); return 0; }
    if(memcmp(srcRspCode,"100146",6) == 0)  {   memcpy(monMsg->rspno,"300082",6); return 0; }
    if(memcmp(srcRspCode,"100147",6) == 0)  {   memcpy(monMsg->rspno,"300083",6); return 0; }
    if(memcmp(srcRspCode,"100150",6) == 0)  {   memcpy(monMsg->rspno,"300084",6); return 0; }
    if(memcmp(srcRspCode,"100952",6) == 0)  {   memcpy(monMsg->rspno,"300085",6); return 0; }
    
}

/***************************************************************
 * KMP����,����ģʽ������ֵ
 * add by dwxiong @2014-8-14 10:04:58
 **************************************************************/
int* computeNext(const char* pattern)
{
    int len , i, nextTemp;
    int *next ;
    len = strlen(pattern);
    if (len == 0) len =1; 
    next =  (int*)malloc( sizeof(int) * len );
    next[0] = -1;
    for (i = 1; i < len; i++)
    {
        nextTemp = next[i-1];
        while (pattern[i-1] != pattern[nextTemp] && nextTemp >=0 )
        {
            nextTemp = next[nextTemp];
        }
        next[i] = nextTemp +1;
    }
    return next;
}


/***************************************************************
 * KMP����,ģʽ��ƥ���ַ���
  * add by dwxiong @2014-8-14 10:04:58
 **************************************************************/
int stringMatchKmp(const char* src, const char* target)
{
    int srcLen, targetLen;
    int i = 0, j = 0, index;
    int *next;
    
    if(src==NULL || target==NULL)
        return -1;
    
    srcLen = strlen(src);
    targetLen = strlen(target);
    next = computeNext(target);

    while (i < srcLen && j < targetLen)
    {
       if ( j== -1 || src[i] == target[j] )
       {
          i++;
          j++;
       }
       else
       {
          j = next[j];
       }
    }
    if (j == targetLen)
    {
        index = i -targetLen;
    }
    else
    {
        index = -1;
    }
    free(next);
    return index;
}

/***************************************************************
 * ��ȡ���������ǩ/�ұ�ǩ�е�ֵ  
 * VALGET(src,targetL,targetR)
 * add by dwxiong @2014-8-14 10:04:58
 **************************************************************/
int valGet(char *src, char *targetL, char *targetR, char *result)
{
    int retkmp1,retkmp2;
	int lentgL,lentgR,i;
    char *valout;
    int   mode;

	retkmp1 =0;
	retkmp2 =0;
	lentgL  =0;
	lentgR  =0;
    
    valout  =result;
        
    lentgL=strlen(targetL);
    lentgR=strlen(targetR);
    
    retkmp1 = stringMatchKmp(src, targetL);
    if(retkmp1 < 0)
    {
        LOG_WRN("VALGET �޸ñ�ǩ [%s][%d] erro",targetL,retkmp1);
        return 0;
    }
    
    retkmp2 = stringMatchKmp(src, targetR);
    if(retkmp2 < 0)
    {

        LOG_WRN("VALGET �޸ñ�ǩ [%s][%d] erro",targetL,retkmp1);
        return 0;
    }
        
    memcpy(valout,&src[retkmp1+lentgL],retkmp2-retkmp1-lentgL);
    valout[retkmp2-retkmp1-lentgL] = '\0';
    LOG_MSG("VALGET key = [%s] ok",targetL);
    /* LOG_MSG("VALGET key_rght= [%s]",targetR); */
    /* LOG_MSG("VALGET key_valu= [%s]",valout); */
	return 0;
}

/********************************************/
/*   Name:   IsNull()                       */
/*   Desc:   judge if the domain is empty   */
/*   Return: 1--empty                       */
/*           0--not empty                   */
/********************************************/
int isSpace(const char *str, int len)
{
    int i;

    for (i = 0; i < len; i++)
    {
        if ((str[i] != ' ') && (str[i] != '\0'))
            return 0;
    }

    return 1;
}


/*delete space andr enter and tablespace before string*/
void LTrim(char *s)
{
  int i;
  for(i = 0; i < strlen(s); i++){
    if((s[i] != ' ') && (s[i] != '\t') && (s[i] != '\n') && (s[i] != '\r')) break;
  }
  strcpy(s, s + i);
}

/*delete space or enter or tablespace after string*/
void RTrim(char *s)
{
  int i;
  for(i = strlen(s) - 1; i > 0; --i){
    if((s[i] != ' ') && (s[i] != '\t') && (s[i] != '\n') && (s[i] != '\r')) break;
  }
  s[i + 1] = 0x00;
}

/*delete all*/
void Trim(char *s)
{
  LTrim(s);
  RTrim(s);
}
