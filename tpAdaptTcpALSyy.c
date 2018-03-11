/**************************************************************
 * TCP通讯同步短连接服务端程序
 *************************************************************/
#include "tpbus.h"
#include <time.h>
TPTASK m_tpTask;

char m_bindAddr[IPADDRLEN+1];  /*服务端IP地址*/
int  m_bindPort;               /*服务端侦听端口*/
int  m_lenlen;                 /*通讯头长度*/
int  m_timeout;                /*超时时间*/
int  m_nMaxThds;               /*最大并发数*/
int  m_des_q;
int  m_des_log,m_des_send;

MQHDL *mqh=NULL;
int  sockfd, m_csock;

int   tpParamInit();
void *tpProcessThread(void *arg);
void  tpQuit(int);
int getacct(char acct[][19+1]);
int mkseconds(char *sendtime);

/**************************************************************
 * 主函数 
 *************************************************************/
 int tpNewHead1(int orgQ, TPHEAD *ptpHead)
{
    TPPORT *ptpPort;

    memset(ptpHead, 0, sizeof(TPHEAD));
 
    if (tpPortShmGet(orgQ, &ptpPort) <= 0) {
        LOG_ERR("读取端点[%d]配置出错\n", orgQ);
        return -1;
    }
 
/*
    if ((ptpHead->tpId = tpNewId()) <= 0) {
        LOG_ERR("分配流水号出错\n");
        return -1;
    }
*/    
 
    ptpHead->orgQ = orgQ;
 
     ptpHead->msgType = MSGAPP;
     ptpHead->fmtType = ptpPort->fmtDefs.fmtType;
     time(&ptpHead->beginTime);
    LOG_MSG("行数[%d]\n",__LINE__);
    return 0;
}

/**获取系统时间,YYYYMMDDhhmmssXXXX*/
int getSystime(char *buftime,char *lsecond)
{
    time_t  ttime;
    struct tm       *timenow;
    struct timeval  tval;

    time(&ttime);
    timenow = localtime(&ttime);
    gettimeofday(&tval, NULL);
    sprintf(buftime,"%04d%02d%02d%02d%02d%02d%06d",timenow->tm_year+1900,timenow->tm_mon+1,timenow->tm_mday,timenow->tm_hour
,timenow->tm_min,timenow->tm_sec,tval.tv_usec);
    sprintf(lsecond,"%d",ttime);
    return 0;
}




int main(int argc, char *argv[])
{
    int  ret;
    int  peerPort;
    char peerAddr[IPADDRLEN+1];
    int  msgLen,msgLen_log;
    int  q_des;
    char alTranCode[5];
    char logPrefix[21];
    char clazz[MSGCLZLEN+1];
    char logfname[200];
    time_t t;
    int i=1,j=0,k=0,l=0;
    TPMSG tpMsg;
        TPMSG tpMsg_log;
int llinterval;
    FILE  *fp=NULL,*fp1=NULL,*fp2=NULL;
    char  filename[200];
    char msgbuf[2048];
    char senddate[8+1];
    char sendtrantime[6+1];
        int len;
    char buf[32*1024];
 char sendtime[32];
    char msgbuf_log[2048];
   char source[2+1];
 char trancode[4+1];
  char sectimemic[6+1];
  char acctno[2001][19+1];
  char alsecond[16+1];

 char sendtimeend[32];
  char alsecondend[16+1];
 char sendtimebefore[32];
  char alsecondbefore[16+1];

 
    float flinterval;

    /*检查命令行参数*/
 /*   if (argc != 2) {
        printf("Usage: %s <任务ID>\n", argv[0]);
        return -1;
    }
*/

  
    /*初始化进程*/
    if (tpTaskInit(atoi(argv[1]), "tpTcpALS", &m_tpTask) != 0) {
        printf("初始化进程出错\n");
        return -1;
    }

    /*初始化配置参数*/
    if (tpParamInit() != 0) {
        printf("初始化配置参数出错\n");
        return -1;
    }
   
    /*设置退出信号*/
    signal(SIGTERM, tpQuit);
  
    /*初始化队列*/
    if ((ret = mq_init()) != 0) {
        printf("mq_init() error: retcode=%d\n", ret);
        printf("初始化队列出错\n");
        return -1;
    }
    
    /*绑定队列*/
    if ((ret = mq_bind(m_tpTask.bindQ, &mqh)) != 0) {
        printf("mq_bind() error: retcode=%d\n", ret);
        printf("绑定队列[%d]出错\n", m_tpTask.bindQ);
        return -1;
    }
    printf("绑定队列[%d]成功\n", m_tpTask.bindQ);
     setLogFile("test.log");
 LOG_MSG("BEGIN"); 


  char bufseq[16+1];
    char bufseqlast[16+1];

 memset(bufseq, 0x00, sizeof(bufseq));
 memset(bufseqlast, 0x00, sizeof(bufseqlast));

    fp1 = fopen("seq.txt","rb+");
    if (fp1 == NULL) {
        close(sockfd);
        LOG_ERR("can not open seqfile[%s]\n","seq.txt");
        printf("can not open seqfile[%s]\n","seq.txt");
        return -1;
    }
    
fread(bufseq, 1, sizeof(bufseq), fp1);
 

i = atoi(bufseq);
printf("seq begin[%s][%d]\n",bufseq,i);
 
 rewind(fp1);



  /*读取报文*/
 memset(&tpMsg, 0x00, sizeof(tpMsg));

    fp = fopen(argv[2],"r");
    if (fp == NULL) {
        close(sockfd);
        LOG_ERR("can not open packfile[%s]\n",argv[2]);
        printf("can not open packfile[%s]\n",argv[2]);
        return -1;
    }
    msgLen = fread(tpMsg.body, 1, sizeof(tpMsg.body), fp);
    
memset(&tpMsg,   0x00, sizeof(tpMsg));
memset(&tpMsg_log,   0x00, sizeof(tpMsg_log));

memset(msgbuf,   0x00, sizeof(msgbuf));
memset(sendtime, 0x00, sizeof(sendtime));

memset(msgbuf_log,   0x00, sizeof(msgbuf_log));

memset(senddate, 0x00, sizeof(senddate));
memset(sendtrantime, 0x00, sizeof(sendtrantime));
memset(sectimemic, 0x00, sizeof(sectimemic));
memset(source, 0x00, sizeof(source));
memset(trancode, 0x00, sizeof(trancode));
 

memset(alsecond, 0x00, sizeof(alsecond));


memset(acctno,   0x00, sizeof(acctno));
memset(sendtimeend,   0x00, sizeof(sendtimeend));
memset(alsecondend,   0x00, sizeof(alsecondend));

memset(sendtimebefore,   0x00, sizeof(sendtimebefore));
memset(alsecondbefore,   0x00, sizeof(alsecondbefore));
 

getSystime(sendtime,alsecond);
LOG_MSG("alsecond=[%s]\n",alsecond);


strcpy(sendtimebefore,sendtime);  
strcpy(alsecondbefore,alsecond);  



  
strncpy(source,"L1",2);    
strncpy(trancode,"7606",4);    
getacct(acctno);
 printf("报文发送开始时间[%s]\n", sendtime);
 LOG_MSG("报文发送开始时间[%s]\n", sendtime);
  printf("预计发送总时间  [%s]\n", argv[4]);
  printf("预计每秒发送次数[%s]\n", argv[3]);
  printf("----------------------------------------------------------\n" );

 LOG_MSG("报文发送开始时间[%s]\n", sendtime);

while(1)
{
memset(senddate,     0x00, sizeof(senddate)     );
memset(sendtrantime, 0x00, sizeof(sendtrantime) );
memset(sectimemic,   0x00, sizeof(sectimemic)   );
strncpy(senddate,     sendtimebefore,     8);    
strncpy(sendtrantime, sendtimebefore+8,   6);  
strncpy(sectimemic,   sendtimebefore+8+6, 6);    

    for(j=1;j<atoi(argv[3])+1;j++)
    {
        
            snprintf(msgbuf, sizeof(msgbuf), "%s%s%s%s%s%s%s%s%s%06d%s%2s%s%s%s%s%s%s%s", 
            "<?xml version=\"1.0\" encoding=\"GB18030\"?><Msg><TRXCODE>"
            ,trancode
            ,"</TRXCODE><BANKCODE>30040000</BANKCODE><TRXDATE>"
            ,senddate
            ,"</TRXDATE><TRXTIME>"
            ,sendtrantime
            ,"</TRXTIME><COINSTCODE>997777</COINSTCODE><COINSTCHANNEL>997777</COINSTCHANNEL><SEQNO>"
            ,senddate
            ,sendtrantime
            ,i
            ,"</SEQNO><SOURCE>"
            ,source 
            ,"</SOURCE><RETCODE></RETCODE><RETMSG></RETMSG><HEADRESERVED></HEADRESERVED><ACCT_NO>"
            ,acctno[k]
            ,"</ACCT_NO><CURRENCY>156</CURRENCY><AMOUNT>000000000001</AMOUNT><KEYTYPE>01</KEYTYPE><IDNO>422823198408120220</IDNO><SURNAME>宋艳清</SURNAME><MOBILE>13521196956</MOBILE>"
            ,"<RESERVED>"
            ,sectimemic
            ,alsecondbefore
            ,"</RESERVED></Msg>"
            ); 
            
            snprintf(tpMsg.body, sizeof(tpMsg.body), "%s", msgbuf ); 
            
            
            msgLen=strlen(msgbuf);
            LOG_MSG("报文发送时间[%s][%s][%s]\n", sendtimebefore,senddate,sendtrantime);
    
            
            /*LOG_MSG("报文[%s]\n",tpMsg.body);
          */

            if (msgLen == 4)  return -1; /*心跳报文*/
            
 
/*printf("行数[%d]\n",__LINE__);
*/
            /* 初始化报文头 */
 /*           if (tpNewHead1(m_tpTask.bindQ, &tpMsg.head) != 0) {
                LOG_MSG("初始化报文头出错 m_tpTask.bindQ[%d]\n",m_tpTask.bindQ);
             }
*/
 
             
            tpMsg.head.bodyLen = msgLen ;
            LOG_MSG("行数[%d]m_lenlen=[%d]tpMsg.head.bodyLen[%d]\n",__LINE__,m_lenlen,tpMsg.head.bodyLen);
            tpSetLogPrefix(tpMsg.head.tpId, logPrefix);
            
            LOG_MSG("初始化报文头...[ok]\n");
 
            /*写主控队列*/
            snprintf(clazz, sizeof(clazz), "%d", tpMsg.head.tpId); 
             msgLen = tpMsg.head.bodyLen + sizeof(TPHEAD);
 
/*            memmove(tpMsg.body,tpMsg.body + m_lenlen, tpMsg.head.bodyLen);
*/
             q_des = m_des_send;
            LOG_MSG("通讯发送[%s]bodylen[%d][%d]...\n", tpMsg.body, tpMsg.head.bodyLen, msgLen);
            if ((ret = mq_post(mqh, NULL, q_des, 0, clazz, (char *)&tpMsg, msgLen, 0)) != 0) {
                LOG_ERR("mq_post() error: retcode=%d\n", ret);
                LOG_ERR("通讯发送[%d]出错\n", Q_SVR);
            }
            else
            {    
                LOG_MSG("通讯发送[%d]...[ok]\n", q_des);
                l++;
            }
 /*==================================================================================================================================================================*/           
            
            
            
                        snprintf(msgbuf_log, sizeof(msgbuf_log), "%s%s%s%8s%6s%06d%8s%-20s", "0001",source,trancode,senddate,sendtrantime,i,"",sendtimebefore);
                        snprintf(tpMsg_log.body, sizeof(tpMsg_log.body), "%s", msgbuf_log ); 
                         tpMsg_log.head.bodyLen = strlen(msgbuf_log) ;
                            /*写主控队列*/
            snprintf(clazz, sizeof(clazz), "%d", tpMsg.head.tpId); 
            msgLen_log = tpMsg_log.head.bodyLen + sizeof(TPHEAD);
            q_des = m_des_log;
            LOG_MSG("日志队列[%s]bodylen[%d][%d]...\n", tpMsg_log.body, tpMsg_log.head.bodyLen, msgLen_log);
            if ((ret = mq_post(mqh, NULL, q_des, 0, clazz, (char *)&tpMsg_log, msgLen_log, 0)) != 0) {
                LOG_ERR("mq_post() error: retcode=%d\n", ret);
                LOG_ERR("日志队列[%d]出错\n", Q_SVR);
            }
            else
            {    
                LOG_MSG("日志队列[%d]...[ok]\n", q_des);
            }
            if(k == 2000) 
            {
                k = 1;
            }
            else
            {
                k++;
            }

            if(i == 999999) 
            {
                i = 1;
            }
            else
            {
                i++;
            }
                
                       
    }            
  
    getSystime(sendtimeend,alsecondend);

   
    llinterval=colinterval(sendtimebefore,sendtimeend);
    flinterval =  llinterval/(float)1000000;
    if(llinterval<=0)
    {
        printf("ERROR========================================耗时[%d]\n",llinterval);
        LOG_MSG("ERROR========================================耗时[%d]\n",llinterval);
    }
    else{
        printf("本次发送耗时[%f]秒 发送结束时[%s]\n",flinterval,sendtimeend);  
        LOG_MSG("本次发送耗时[%f]秒 发送结束时[%s]\n",flinterval,sendtimeend);  
    }
        
      
    if(  (atoi(alsecondend) - atoi(alsecond)) >= atoi(argv[4])  ) 
    {
        printf("发送时间到\n");
        LOG_MSG("发送时间到\n");                
        break;       
    }     
    if(llinterval>=1000000)
    {
        printf("停止发送[0]微秒\n"); 
        LOG_MSG("停止发送[0]微秒\n"); 
        strcpy(sendtimebefore,sendtimeend);  
        strcpy(alsecondbefore,alsecondend);   
         
        memset(sendtimeend,   0x00, sizeof(sendtimeend));
        memset(alsecondend,   0x00, sizeof(alsecondend));
    }
    else
    {       
        usleep(1000000-llinterval);
        printf("停止发送[%d]微秒\n", 1000000-llinterval);
        LOG_MSG("停止发送[%d]微秒\n", 1000000-llinterval);
        memset(sendtimeend,   0x00, sizeof(sendtimeend));
        memset(alsecondend,   0x00, sizeof(alsecondend));        
        getSystime(sendtimeend,alsecondend);      
        strcpy(sendtimebefore,sendtimeend); 
        strcpy(alsecondbefore,alsecondend);          
        memset(sendtimeend,   0x00, sizeof(sendtimeend));
        memset(alsecondend,   0x00, sizeof(alsecondend));
} 
    

            
}


    memset(sendtimeend,   0x00, sizeof(sendtimeend));
    memset(alsecondend,   0x00, sizeof(alsecondend));
    getSystime(sendtimeend,alsecondend);
    llinterval=colinterval(sendtime,sendtimeend);

     flinterval =  llinterval/(float)1000000;
   printf("总耗时[%f]\n", flinterval);
    printf("总条数[%d]\n", l);

    fp2 = fopen("seq.txt","w");
    if (fp2 == NULL) {
        close(sockfd);
        LOG_ERR("can not open seqfile[%s]\n","seq.txt");
        printf("can not open seqfile[%s]\n","seq.txt");
        return -1;
    }


 
 
 sprintf(bufseqlast,"%d",i);
 printf("下次发送起始发送流水号[%s]\n",bufseqlast);
  fwrite(bufseqlast, 1, strlen(bufseqlast) ,fp2);

fclose(fp2);
  
  fclose(fp);
  fclose(fp1);
  
  printf("报文发送截止时间[%s]\n", sendtimeend);
  LOG_MSG("报文发送截止时间[%s]\n", sendtimeend);

}

int colinterval(char *sendtime,char *sendtimeend)
{
    int i,j;
    int linterval;
    char nowmic[6+1];
    char beforemic[6+1];
    
    memset(nowmic,    0x00, sizeof(nowmic));
    memset(beforemic, 0x00, sizeof(beforemic));
    i = mkseconds(sendtime);
    j = mkseconds(sendtimeend);
 
 strncpy(nowmic,    sendtimeend+14, 6);
 strncpy(beforemic, sendtime+14, 6);

 
 
 linterval =(j-i)*1000000+atoi(nowmic)-atoi(beforemic);
 
 return linterval;
}

int mkseconds(char *tsendtime)
{
  time_t timept;
struct tm pt;
char ltm_year[4+1];
char ltm_month[2+1];
char ltm_mday[2+1];
char ltm_hour[2+1];
char ltm_min[2+1];
char ltm_sec[2+1];
memset(ltm_year, 0x00, sizeof(ltm_year));
memset(ltm_month,0x00, sizeof(ltm_month));
memset(ltm_mday, 0x00, sizeof(ltm_mday));
memset(ltm_hour, 0x00, sizeof(ltm_hour));
memset(ltm_min,  0x00, sizeof(ltm_min));
memset(ltm_sec,  0x00, sizeof(ltm_sec));
memset(&pt, 0x00, sizeof(pt));

strncpy(ltm_year, tsendtime, 4);
strncpy(ltm_month,tsendtime+4, 2);
strncpy(ltm_mday, tsendtime+4+2, 2);
strncpy(ltm_hour, tsendtime+4+2+2, 2);
strncpy(ltm_min,  tsendtime+4+2+2+2, 2);
strncpy(ltm_sec,  tsendtime+4+2+2+2+2, 2);
pt.tm_year = atoi(ltm_year) - 1900;
pt.tm_mon  = atoi(ltm_month)- 1;
pt.tm_mday = atoi(ltm_mday);
pt.tm_hour = atoi(ltm_hour);
pt.tm_min  = atoi(ltm_min);
pt.tm_sec  = atoi(ltm_sec);
/*
printf("pt = %s %s %s %s %s %s\n",ltm_year,ltm_month,ltm_mday,ltm_hour,ltm_min,ltm_sec);
printf("pt = %d %d %d %d %d %d\n", pt.tm_year+1900,pt.tm_mon+1,pt.tm_mday,pt.tm_hour,pt.tm_min,pt.tm_sec);
*/
timept = mktime(&pt);
/*printf("time()->localtime()->mktime():%ld\n", timept);
*/
return timept;

}
/***************************************************************
 * 初始化配置参数
 **************************************************************/
int tpParamInit()
{
    if (tpGetParamString(m_tpTask.params, "bindAddr", m_bindAddr) != 0) {
        LOG_ERR("读取参数[bindAddr]出错\n");
        return -1;
    }
    LOG_MSG("参数[bindAddr]=[%s]\n", m_bindAddr);

    if (tpGetParamInt(m_tpTask.params, "bindPort", &m_bindPort) != 0) {
        LOG_ERR("读取参数[bindPort]出错\n");
        return -1;
    }
    LOG_MSG("参数[bindPort]=[%d]\n", m_bindPort);
  
    if (tpGetParamInt(m_tpTask.params, "lenlen", &m_lenlen) != 0) m_lenlen = 4;
    LOG_MSG("参数[lenlen]  =[%d]\n", m_lenlen);

    if (tpGetParamInt(m_tpTask.params, "timeout", &m_timeout) != 0) m_timeout = 60;
    LOG_MSG("参数[timeout] =[%d]\n", m_timeout);
 
    if (tpGetParamInt(m_tpTask.params, "des_q", &m_des_q) != 0) m_des_q = Q_SVR;
    LOG_MSG("参数[des_q]   =[%d]\n", m_des_q);

    if (tpGetParamInt(m_tpTask.params, "des_log", &m_des_log) != 0) m_des_log = Q_SVR;
    LOG_MSG("参数[m_des_log]   =[%d]\n", m_des_log); 

    if (tpGetParamInt(m_tpTask.params, "des_send", &m_des_send) != 0) m_des_send = Q_SVR;
    LOG_MSG("参数[m_des_send]   =[%d]\n", m_des_send);     
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


int getacct(char acct[][19+1])
{
strncpy(acct[0   ], "6236640000000000018", 19);
strncpy(acct[1   ], "6236640000000000026", 19);
strncpy(acct[2   ], "6236640000000000034", 19);
strncpy(acct[3   ], "6236640000000000042", 19);
strncpy(acct[4   ], "6236640000000000059", 19);
strncpy(acct[5   ], "6236640000000000067", 19);
strncpy(acct[6   ], "6236640000000000075", 19);
strncpy(acct[7   ], "6236640000000000083", 19);
strncpy(acct[8   ], "6236640000000000091", 19);
strncpy(acct[9   ], "6236640000000000109", 19);
strncpy(acct[10  ], "6236640000000000117", 19);
strncpy(acct[11  ], "6236640000000000125", 19);
strncpy(acct[12  ], "6236640000000000133", 19);
strncpy(acct[13  ], "6236640000000000158", 19);
strncpy(acct[14  ], "6236640000000000166", 19);
strncpy(acct[15  ], "6236640000000000174", 19);
strncpy(acct[16  ], "6236640000000000182", 19);
strncpy(acct[17  ], "6236640000000000190", 19);
strncpy(acct[18  ], "6236640000000000208", 19);
strncpy(acct[19  ], "6236640000000000216", 19);
strncpy(acct[20  ], "6236640000000000224", 19);
strncpy(acct[21  ], "6236640000000000232", 19);
strncpy(acct[22  ], "6236640000000000240", 19);
strncpy(acct[23  ], "6236640000000000257", 19);
strncpy(acct[24  ], "6236640000000000265", 19);
strncpy(acct[25  ], "6236640000000000273", 19);
strncpy(acct[26  ], "6236640000000000281", 19);
strncpy(acct[27  ], "6236640000000000299", 19);
strncpy(acct[28  ], "6236640000000000307", 19);
strncpy(acct[29  ], "6236640000000000315", 19);
strncpy(acct[30  ], "6236640000000000331", 19);
strncpy(acct[31  ], "6236640000000000349", 19);
strncpy(acct[32  ], "6236640000000000356", 19);
strncpy(acct[33  ], "6236640000000000364", 19);
strncpy(acct[34  ], "6236640000000000372", 19);
strncpy(acct[35  ], "6236640000000000380", 19);
strncpy(acct[36  ], "6236640000000000398", 19);
strncpy(acct[37  ], "6236640000000000406", 19);
strncpy(acct[38  ], "6236640000000000414", 19);
strncpy(acct[39  ], "6236640000000000422", 19);
strncpy(acct[40  ], "6236640000000000430", 19);
strncpy(acct[41  ], "6236640000000000448", 19);
strncpy(acct[42  ], "6236640000000000455", 19);
strncpy(acct[43  ], "6236640000000000463", 19);
strncpy(acct[44  ], "6236640000000000471", 19);
strncpy(acct[45  ], "6236640000000000489", 19);
strncpy(acct[46  ], "6236640000000000497", 19);
strncpy(acct[47  ], "6236640000000000505", 19);
strncpy(acct[48  ], "6236640000000000513", 19);
strncpy(acct[49  ], "6236640000000000521", 19);
strncpy(acct[50  ], "6236640000000000539", 19);
strncpy(acct[51  ], "6236640000000000547", 19);
strncpy(acct[52  ], "6236640000000000554", 19);
strncpy(acct[53  ], "6236640000000000562", 19);
strncpy(acct[54  ], "6236640000000000570", 19);
strncpy(acct[55  ], "6236640000000000588", 19);
strncpy(acct[56  ], "6236640000000000596", 19);
strncpy(acct[57  ], "6236640000000000604", 19);
strncpy(acct[58  ], "6236640000000000612", 19);
strncpy(acct[59  ], "6236640000000000620", 19);
strncpy(acct[60  ], "6236640000000000638", 19);
strncpy(acct[61  ], "6236640000000000646", 19);
strncpy(acct[62  ], "6236640000000000653", 19);
strncpy(acct[63  ], "6236640000000000661", 19);
strncpy(acct[64  ], "6236640000000000679", 19);
strncpy(acct[65  ], "6236640000000000687", 19);
strncpy(acct[66  ], "6236640000000000695", 19);
strncpy(acct[67  ], "6236640000000000703", 19);
strncpy(acct[68  ], "6236640000000000711", 19);
strncpy(acct[69  ], "6236640000000000729", 19);
strncpy(acct[70  ], "6236640000000000737", 19);
strncpy(acct[71  ], "6236640000000000745", 19);
strncpy(acct[72  ], "6236640000000000752", 19);
strncpy(acct[73  ], "6236640000000000760", 19);
strncpy(acct[74  ], "6236640000000000778", 19);
strncpy(acct[75  ], "6236640000000000786", 19);
strncpy(acct[76  ], "6236640000000000794", 19);
strncpy(acct[77  ], "6236640000000000802", 19);
strncpy(acct[78  ], "6236640000000000810", 19);
strncpy(acct[79  ], "6236640000000000828", 19);
strncpy(acct[80  ], "6236640000000000836", 19);
strncpy(acct[81  ], "6236640000000000844", 19);
strncpy(acct[82  ], "6236640000000000851", 19);
strncpy(acct[83  ], "6236640000000000869", 19);
strncpy(acct[84  ], "6236640000000000877", 19);
strncpy(acct[85  ], "6236640000000000885", 19);
strncpy(acct[86  ], "6236640000000000893", 19);
strncpy(acct[87  ], "6236640000000000901", 19);
strncpy(acct[88  ], "6236640000000000919", 19);
strncpy(acct[89  ], "6236640000000000927", 19);
strncpy(acct[90  ], "6236640000000000935", 19);
strncpy(acct[91  ], "6236640000000000943", 19);
strncpy(acct[92  ], "6236640000000000950", 19);
strncpy(acct[93  ], "6236640000000000968", 19);
strncpy(acct[94  ], "6236640000000000976", 19);
strncpy(acct[95  ], "6236640000000000984", 19);
strncpy(acct[96  ], "6236640000000000992", 19);
strncpy(acct[97  ], "6236640000000001008", 19);
strncpy(acct[98  ], "6236640000000001016", 19);
strncpy(acct[99  ], "6236640000000001024", 19);
strncpy(acct[100 ], "6236640000000001032", 19);
strncpy(acct[101 ], "6236640000000001040", 19);
strncpy(acct[102 ], "6236640000000001057", 19);
strncpy(acct[103 ], "6236640000000001065", 19);
strncpy(acct[104 ], "6236640000000001073", 19);
strncpy(acct[105 ], "6236640000000001081", 19);
strncpy(acct[106 ], "6236640000000001099", 19);
strncpy(acct[107 ], "6236640000000001107", 19);
strncpy(acct[108 ], "6236640000000001115", 19);
strncpy(acct[109 ], "6236640000000001123", 19);
strncpy(acct[110 ], "6236640000000001131", 19);
strncpy(acct[111 ], "6236640000000001149", 19);
strncpy(acct[112 ], "6236640000000001156", 19);
strncpy(acct[113 ], "6236640000000001164", 19);
strncpy(acct[114 ], "6236640000000001172", 19);
strncpy(acct[115 ], "6236640000000001180", 19);
strncpy(acct[116 ], "6236640000000001198", 19);
strncpy(acct[117 ], "6236640000000001206", 19);
strncpy(acct[118 ], "6236640000000001214", 19);
strncpy(acct[119 ], "6236640000000001222", 19);
strncpy(acct[120 ], "6236640000000001230", 19);
strncpy(acct[121 ], "6236640000000001248", 19);
strncpy(acct[122 ], "6236640000000001255", 19);
strncpy(acct[123 ], "6236640000000001263", 19);
strncpy(acct[124 ], "6236640000000001271", 19);
strncpy(acct[125 ], "6236640000000001289", 19);
strncpy(acct[126 ], "6236640000000001297", 19);
strncpy(acct[127 ], "6236640000000001305", 19);
strncpy(acct[128 ], "6236640000000001313", 19);
strncpy(acct[129 ], "6236640000000001321", 19);
strncpy(acct[130 ], "6236640000000001339", 19);
strncpy(acct[131 ], "6236640000000001347", 19);
strncpy(acct[132 ], "6236640000000001354", 19);
strncpy(acct[133 ], "6236640000000001362", 19);
strncpy(acct[134 ], "6236640000000001370", 19);
strncpy(acct[135 ], "6236640000000001388", 19);
strncpy(acct[136 ], "6236640000000001396", 19);
strncpy(acct[137 ], "6236640000000001404", 19);
strncpy(acct[138 ], "6236640000000001412", 19);
strncpy(acct[139 ], "6236640000000001420", 19);
strncpy(acct[140 ], "6236640000000001438", 19);
strncpy(acct[141 ], "6236640000000001446", 19);
strncpy(acct[142 ], "6236640000000001453", 19);
strncpy(acct[143 ], "6236640000000001461", 19);
strncpy(acct[144 ], "6236640000000001479", 19);
strncpy(acct[145 ], "6236640000000001487", 19);
strncpy(acct[146 ], "6236640000000001495", 19);
strncpy(acct[147 ], "6236640000000001503", 19);
strncpy(acct[148 ], "6236640000000001511", 19);
strncpy(acct[149 ], "6236640000000001529", 19);
strncpy(acct[150 ], "6236640000000001537", 19);
strncpy(acct[151 ], "6236640000000001545", 19);
strncpy(acct[152 ], "6236640000000001552", 19);
strncpy(acct[153 ], "6236640000000001560", 19);
strncpy(acct[154 ], "6236640000000001578", 19);
strncpy(acct[155 ], "6236640000000001586", 19);
strncpy(acct[156 ], "6236640000000001594", 19);
strncpy(acct[157 ], "6236640000000001602", 19);
strncpy(acct[158 ], "6236640000000001610", 19);
strncpy(acct[159 ], "6236640000000001628", 19);
strncpy(acct[160 ], "6236640000000001636", 19);
strncpy(acct[161 ], "6236640000000001644", 19);
strncpy(acct[162 ], "6236640000000001651", 19);
strncpy(acct[163 ], "6236640000000001669", 19);
strncpy(acct[164 ], "6236640000000001677", 19);
strncpy(acct[165 ], "6236640000000001685", 19);
strncpy(acct[166 ], "6236640000000001693", 19);
strncpy(acct[167 ], "6236640000000001701", 19);
strncpy(acct[168 ], "6236640000000001719", 19);
strncpy(acct[169 ], "6236640000000001727", 19);
strncpy(acct[170 ], "6236640000000001735", 19);
strncpy(acct[171 ], "6236640000000001743", 19);
strncpy(acct[172 ], "6236640000000001750", 19);
strncpy(acct[173 ], "6236640000000001768", 19);
strncpy(acct[174 ], "6236640000000001776", 19);
strncpy(acct[175 ], "6236640000000001784", 19);
strncpy(acct[176 ], "6236640000000001792", 19);
strncpy(acct[177 ], "6236640000000001800", 19);
strncpy(acct[178 ], "6236640000000001826", 19);
strncpy(acct[179 ], "6236640000000001834", 19);
strncpy(acct[180 ], "6236640000000001842", 19);
strncpy(acct[181 ], "6236640000000001859", 19);
strncpy(acct[182 ], "6236640000000001867", 19);
strncpy(acct[183 ], "6236640000000001875", 19);
strncpy(acct[184 ], "6236640000000001883", 19);
strncpy(acct[185 ], "6236640000000001909", 19);
strncpy(acct[186 ], "6236640000000001917", 19);
strncpy(acct[187 ], "6236640000000001925", 19);
strncpy(acct[188 ], "6236640000000001933", 19);
strncpy(acct[189 ], "6236640000000001941", 19);
strncpy(acct[190 ], "6236640000000001958", 19);
strncpy(acct[191 ], "6236640000000001966", 19);
strncpy(acct[192 ], "6236640000000001974", 19);
strncpy(acct[193 ], "6236640000000001982", 19);
strncpy(acct[194 ], "6236640000000001990", 19);
strncpy(acct[195 ], "6236640000000002006", 19);
strncpy(acct[196 ], "6236640000000002014", 19);
strncpy(acct[197 ], "6236640000000002022", 19);
strncpy(acct[198 ], "6236640000000002030", 19);
strncpy(acct[199 ], "6236640000000002048", 19);
strncpy(acct[200 ], "6236640000000002055", 19);
strncpy(acct[201 ], "6236640000000002063", 19);
strncpy(acct[202 ], "6236640000000002071", 19);
strncpy(acct[203 ], "6236640000000002089", 19);
strncpy(acct[204 ], "6236640000000002097", 19);
strncpy(acct[205 ], "6236640000000002105", 19);
strncpy(acct[206 ], "6236640000000002113", 19);
strncpy(acct[207 ], "6236640000000002121", 19);
strncpy(acct[208 ], "6236640000000002139", 19);
strncpy(acct[209 ], "6236640000000002147", 19);
strncpy(acct[210 ], "6236640000000002154", 19);
strncpy(acct[211 ], "6236640000000002162", 19);
strncpy(acct[212 ], "6236640000000002170", 19);
strncpy(acct[213 ], "6236640000000002188", 19);
strncpy(acct[214 ], "6236640000000002196", 19);
strncpy(acct[215 ], "6236640000000002204", 19);
strncpy(acct[216 ], "6236640000000002212", 19);
strncpy(acct[217 ], "6236640000000002220", 19);
strncpy(acct[218 ], "6236640000000002238", 19);
strncpy(acct[219 ], "6236640000000002246", 19);
strncpy(acct[220 ], "6236640000000002253", 19);
strncpy(acct[221 ], "6236640000000002261", 19);
strncpy(acct[222 ], "6236640000000002279", 19);
strncpy(acct[223 ], "6236640000000002287", 19);
strncpy(acct[224 ], "6236640000000002295", 19);
strncpy(acct[225 ], "6236640000000002303", 19);
strncpy(acct[226 ], "6236640000000002311", 19);
strncpy(acct[227 ], "6236640000000002329", 19);
strncpy(acct[228 ], "6236640000000002337", 19);
strncpy(acct[229 ], "6236640000000002345", 19);
strncpy(acct[230 ], "6236640000000002360", 19);
strncpy(acct[231 ], "6236640000000002378", 19);
strncpy(acct[232 ], "6236640000000002386", 19);
strncpy(acct[233 ], "6236640000000002394", 19);
strncpy(acct[234 ], "6236640000000002402", 19);
strncpy(acct[235 ], "6236640000000002410", 19);
strncpy(acct[236 ], "6236640000000002428", 19);
strncpy(acct[237 ], "6236640000000002436", 19);
strncpy(acct[238 ], "6236640000000002444", 19);
strncpy(acct[239 ], "6236640000000002451", 19);
strncpy(acct[240 ], "6236640000000002469", 19);
strncpy(acct[241 ], "6236640000000002477", 19);
strncpy(acct[242 ], "6236640000000002485", 19);
strncpy(acct[243 ], "6236640000000002493", 19);
strncpy(acct[244 ], "6236640000000002501", 19);
strncpy(acct[245 ], "6236640000000002519", 19);
strncpy(acct[246 ], "6236640000000002527", 19);
strncpy(acct[247 ], "6236640000000002535", 19);
strncpy(acct[248 ], "6236640000000002543", 19);
strncpy(acct[249 ], "6236640000000002550", 19);
strncpy(acct[250 ], "6236640000000002568", 19);
strncpy(acct[251 ], "6236640000000002576", 19);
strncpy(acct[252 ], "6236640000000002584", 19);
strncpy(acct[253 ], "6236640000000002592", 19);
strncpy(acct[254 ], "6236640000000002600", 19);
strncpy(acct[255 ], "6236640000000002618", 19);
strncpy(acct[256 ], "6236640000000002626", 19);
strncpy(acct[257 ], "6236640000000002642", 19);
strncpy(acct[258 ], "6236640000000002659", 19);
strncpy(acct[259 ], "6236640000000002667", 19);
strncpy(acct[260 ], "6236640000000002675", 19);
strncpy(acct[261 ], "6236640000000002683", 19);
strncpy(acct[262 ], "6236640000000002691", 19);
strncpy(acct[263 ], "6236640000000002709", 19);
strncpy(acct[264 ], "6236640000000002717", 19);
strncpy(acct[265 ], "6236640000000002725", 19);
strncpy(acct[266 ], "6236640000000002733", 19);
strncpy(acct[267 ], "6236640000000002741", 19);
strncpy(acct[268 ], "6236640000000002758", 19);
strncpy(acct[269 ], "6236640000000002766", 19);
strncpy(acct[270 ], "6236640000000002774", 19);
strncpy(acct[271 ], "6236640000000002782", 19);
strncpy(acct[272 ], "6236640000000002790", 19);
strncpy(acct[273 ], "6236640000000002808", 19);
strncpy(acct[274 ], "6236640000000002816", 19);
strncpy(acct[275 ], "6236640000000002824", 19);
strncpy(acct[276 ], "6236640000000002832", 19);
strncpy(acct[277 ], "6236640000000002840", 19);
strncpy(acct[278 ], "6236640000000002857", 19);
strncpy(acct[279 ], "6236640000000002865", 19);
strncpy(acct[280 ], "6236640000000002873", 19);
strncpy(acct[281 ], "6236640000000002881", 19);
strncpy(acct[282 ], "6236640000000002899", 19);
strncpy(acct[283 ], "6236640000000002907", 19);
strncpy(acct[284 ], "6236640000000002915", 19);
strncpy(acct[285 ], "6236640000000002923", 19);
strncpy(acct[286 ], "6236640000000002931", 19);
strncpy(acct[287 ], "6236640000000002949", 19);
strncpy(acct[288 ], "6236640000000002956", 19);
strncpy(acct[289 ], "6236640000000002964", 19);
strncpy(acct[290 ], "6236640000000002972", 19);
strncpy(acct[291 ], "6236640000000002980", 19);
strncpy(acct[292 ], "6236640000000002998", 19);
strncpy(acct[293 ], "6236640000000003004", 19);
strncpy(acct[294 ], "6236640000000003012", 19);
strncpy(acct[295 ], "6236640000000003020", 19);
strncpy(acct[296 ], "6236640000000003038", 19);
strncpy(acct[297 ], "6236640000000003046", 19);
strncpy(acct[298 ], "6236640000000003053", 19);
strncpy(acct[299 ], "6236640000000003061", 19);
strncpy(acct[300 ], "6236640000000003079", 19);
strncpy(acct[301 ], "6236640000000003087", 19);
strncpy(acct[302 ], "6236640000000003095", 19);
strncpy(acct[303 ], "6236640000000003103", 19);
strncpy(acct[304 ], "6236640000000003111", 19);
strncpy(acct[305 ], "6236640000000003129", 19);
strncpy(acct[306 ], "6236640000000003137", 19);
strncpy(acct[307 ], "6236640000000003145", 19);
strncpy(acct[308 ], "6236640000000003152", 19);
strncpy(acct[309 ], "6236640000000003160", 19);
strncpy(acct[310 ], "6236640000000003178", 19);
strncpy(acct[311 ], "6236640000000003186", 19);
strncpy(acct[312 ], "6236640000000003194", 19);
strncpy(acct[313 ], "6236640000000003202", 19);
strncpy(acct[314 ], "6236640000000003210", 19);
strncpy(acct[315 ], "6236640000000003236", 19);
strncpy(acct[316 ], "6236640000000003244", 19);
strncpy(acct[317 ], "6236640000000003251", 19);
strncpy(acct[318 ], "6236640000000003269", 19);
strncpy(acct[319 ], "6236640000000003277", 19);
strncpy(acct[320 ], "6236640000000003285", 19);
strncpy(acct[321 ], "6236640000000003293", 19);
strncpy(acct[322 ], "6236640000000003301", 19);
strncpy(acct[323 ], "6236640000000003319", 19);
strncpy(acct[324 ], "6236640000000003327", 19);
strncpy(acct[325 ], "6236640000000003335", 19);
strncpy(acct[326 ], "6236640000000003343", 19);
strncpy(acct[327 ], "6236640000000003350", 19);
strncpy(acct[328 ], "6236640000000003368", 19);
strncpy(acct[329 ], "6236640000000003376", 19);
strncpy(acct[330 ], "6236640000000003384", 19);
strncpy(acct[331 ], "6236640000000003392", 19);
strncpy(acct[332 ], "6236640000000003400", 19);
strncpy(acct[333 ], "6236640000000003418", 19);
strncpy(acct[334 ], "6236640000000003426", 19);
strncpy(acct[335 ], "6236640000000003434", 19);
strncpy(acct[336 ], "6236640000000003442", 19);
strncpy(acct[337 ], "6236640000000003459", 19);
strncpy(acct[338 ], "6236640000000003467", 19);
strncpy(acct[339 ], "6236640000000003475", 19);
strncpy(acct[340 ], "6236640000000003483", 19);
strncpy(acct[341 ], "6236640000000003491", 19);
strncpy(acct[342 ], "6236640000000003509", 19);
strncpy(acct[343 ], "6236640000000003517", 19);
strncpy(acct[344 ], "6236640000000003525", 19);
strncpy(acct[345 ], "6236640000000003533", 19);
strncpy(acct[346 ], "6236640000000003541", 19);
strncpy(acct[347 ], "6236640000000003558", 19);
strncpy(acct[348 ], "6236640000000003566", 19);
strncpy(acct[349 ], "6236640000000003574", 19);
strncpy(acct[350 ], "6236640000000003582", 19);
strncpy(acct[351 ], "6236640000000003590", 19);
strncpy(acct[352 ], "6236640000000003608", 19);
strncpy(acct[353 ], "6236640000000003616", 19);
strncpy(acct[354 ], "6236640000000003624", 19);
strncpy(acct[355 ], "6236640000000003632", 19);
strncpy(acct[356 ], "6236640000000003640", 19);
strncpy(acct[357 ], "6236640000000003657", 19);
strncpy(acct[358 ], "6236640000000003665", 19);
strncpy(acct[359 ], "6236640000000003673", 19);
strncpy(acct[360 ], "6236640000000003681", 19);
strncpy(acct[361 ], "6236640000000003699", 19);
strncpy(acct[362 ], "6236640000000003707", 19);
strncpy(acct[363 ], "6236640000000003715", 19);
strncpy(acct[364 ], "6236640000000003723", 19);
strncpy(acct[365 ], "6236640000000003731", 19);
strncpy(acct[366 ], "6236640000000003749", 19);
strncpy(acct[367 ], "6236640000000003756", 19);
strncpy(acct[368 ], "6236640000000003764", 19);
strncpy(acct[369 ], "6236640000000003772", 19);
strncpy(acct[370 ], "6236640000000003780", 19);
strncpy(acct[371 ], "6236640000000003798", 19);
strncpy(acct[372 ], "6236640000000003806", 19);
strncpy(acct[373 ], "6236640000000003814", 19);
strncpy(acct[374 ], "6236640000000003822", 19);
strncpy(acct[375 ], "6236640000000003830", 19);
strncpy(acct[376 ], "6236640000000003848", 19);
strncpy(acct[377 ], "6236640000000003855", 19);
strncpy(acct[378 ], "6236640000000003863", 19);
strncpy(acct[379 ], "6236640000000003871", 19);
strncpy(acct[380 ], "6236640000000003889", 19);
strncpy(acct[381 ], "6236640000000003897", 19);
strncpy(acct[382 ], "6236640000000003905", 19);
strncpy(acct[383 ], "6236640000000003913", 19);
strncpy(acct[384 ], "6236640000000003921", 19);
strncpy(acct[385 ], "6236640000000003939", 19);
strncpy(acct[386 ], "6236640000000003947", 19);
strncpy(acct[387 ], "6236640000000003954", 19);
strncpy(acct[388 ], "6236640000000003962", 19);
strncpy(acct[389 ], "6236640000000003970", 19);
strncpy(acct[390 ], "6236640000000003988", 19);
strncpy(acct[391 ], "6236640000000003996", 19);
strncpy(acct[392 ], "6236640000000004002", 19);
strncpy(acct[393 ], "6236640000000004010", 19);
strncpy(acct[394 ], "6236640000000004028", 19);
strncpy(acct[395 ], "6236640000000004036", 19);
strncpy(acct[396 ], "6236640000000004044", 19);
strncpy(acct[397 ], "6236640000000004051", 19);
strncpy(acct[398 ], "6236640000000004069", 19);
strncpy(acct[399 ], "6236640000000004077", 19);
strncpy(acct[400 ], "6236640000000004085", 19);
strncpy(acct[401 ], "6236640000000004093", 19);
strncpy(acct[402 ], "6236640000000004101", 19);
strncpy(acct[403 ], "6236640000000004119", 19);
strncpy(acct[404 ], "6236640000000004127", 19);
strncpy(acct[405 ], "6236640000000004135", 19);
strncpy(acct[406 ], "6236640000000004143", 19);
strncpy(acct[407 ], "6236640000000004150", 19);
strncpy(acct[408 ], "6236640000000004168", 19);
strncpy(acct[409 ], "6236640000000004176", 19);
strncpy(acct[410 ], "6236640000000004184", 19);
strncpy(acct[411 ], "6236640000000004192", 19);
strncpy(acct[412 ], "6236640000000004200", 19);
strncpy(acct[413 ], "6236640000000004218", 19);
strncpy(acct[414 ], "6236640000000004226", 19);
strncpy(acct[415 ], "6236640000000004234", 19);
strncpy(acct[416 ], "6236640000000004242", 19);
strncpy(acct[417 ], "6236640000000004259", 19);
strncpy(acct[418 ], "6236640000000004267", 19);
strncpy(acct[419 ], "6236640000000004275", 19);
strncpy(acct[420 ], "6236640000000004283", 19);
strncpy(acct[421 ], "6236640000000004291", 19);
strncpy(acct[422 ], "6236640000000004309", 19);
strncpy(acct[423 ], "6236640000000004317", 19);
strncpy(acct[424 ], "6236640000000004325", 19);
strncpy(acct[425 ], "6236640000000004333", 19);
strncpy(acct[426 ], "6236640000000004341", 19);
strncpy(acct[427 ], "6236640000000004358", 19);
strncpy(acct[428 ], "6236640000000004366", 19);
strncpy(acct[429 ], "6236640000000004374", 19);
strncpy(acct[430 ], "6236640000000004382", 19);
strncpy(acct[431 ], "6236640000000004390", 19);
strncpy(acct[432 ], "6236640000000004408", 19);
strncpy(acct[433 ], "6236640000000004416", 19);
strncpy(acct[434 ], "6236640000000004424", 19);
strncpy(acct[435 ], "6236640000000004432", 19);
strncpy(acct[436 ], "6236640000000004440", 19);
strncpy(acct[437 ], "6236640000000004457", 19);
strncpy(acct[438 ], "6236640000000004465", 19);
strncpy(acct[439 ], "6236640000000004473", 19);
strncpy(acct[440 ], "6236640000000004481", 19);
strncpy(acct[441 ], "6236640000000004499", 19);
strncpy(acct[442 ], "6236640000000004507", 19);
strncpy(acct[443 ], "6236640000000004515", 19);
strncpy(acct[444 ], "6236640000000004523", 19);
strncpy(acct[445 ], "6236640000000004531", 19);
strncpy(acct[446 ], "6236640000000004549", 19);
strncpy(acct[447 ], "6236640000000004556", 19);
strncpy(acct[448 ], "6236640000000004564", 19);
strncpy(acct[449 ], "6236640000000004572", 19);
strncpy(acct[450 ], "6236640000000004580", 19);
strncpy(acct[451 ], "6236640000000004598", 19);
strncpy(acct[452 ], "6236640000000004606", 19);
strncpy(acct[453 ], "6236640000000004614", 19);
strncpy(acct[454 ], "6236640000000004622", 19);
strncpy(acct[455 ], "6236640000000004630", 19);
strncpy(acct[456 ], "6236640000000004655", 19);
strncpy(acct[457 ], "6236640000000004663", 19);
strncpy(acct[458 ], "6236640000000004671", 19);
strncpy(acct[459 ], "6236640000000004689", 19);
strncpy(acct[460 ], "6236640000000004697", 19);
strncpy(acct[461 ], "6236640000000004705", 19);
strncpy(acct[462 ], "6236640000000004713", 19);
strncpy(acct[463 ], "6236640000000004721", 19);
strncpy(acct[464 ], "6236640000000004739", 19);
strncpy(acct[465 ], "6236640000000004747", 19);
strncpy(acct[466 ], "6236640000000004754", 19);
strncpy(acct[467 ], "6236640000000004762", 19);
strncpy(acct[468 ], "6236640000000004796", 19);
strncpy(acct[469 ], "6236640000000004804", 19);
strncpy(acct[470 ], "6236640000000004812", 19);
strncpy(acct[471 ], "6236640000000004820", 19);
strncpy(acct[472 ], "6236640000000004838", 19);
strncpy(acct[473 ], "6236640000000004846", 19);
strncpy(acct[474 ], "6236640000000004853", 19);
strncpy(acct[475 ], "6236640000000004861", 19);
strncpy(acct[476 ], "6236640000000004879", 19);
strncpy(acct[477 ], "6236640000000004887", 19);
strncpy(acct[478 ], "6236640000000004895", 19);
strncpy(acct[479 ], "6236640000000004903", 19);
strncpy(acct[480 ], "6236640000000004911", 19);
strncpy(acct[481 ], "6236640000000004929", 19);
strncpy(acct[482 ], "6236640000000004937", 19);
strncpy(acct[483 ], "6236640000000004945", 19);
strncpy(acct[484 ], "6236640000000004952", 19);
strncpy(acct[485 ], "6236640000000004960", 19);
strncpy(acct[486 ], "6236640000000004978", 19);
strncpy(acct[487 ], "6236640000000004986", 19);
strncpy(acct[488 ], "6236640000000004994", 19);
strncpy(acct[489 ], "6236640000000005009", 19);
strncpy(acct[490 ], "6236640000000005017", 19);
strncpy(acct[491 ], "6236640000000005025", 19);
strncpy(acct[492 ], "6236640000000005033", 19);
strncpy(acct[493 ], "6236640000000005041", 19);
strncpy(acct[494 ], "6236640000000005058", 19);
strncpy(acct[495 ], "6236640000000005066", 19);
strncpy(acct[496 ], "6236640000000005074", 19);
strncpy(acct[497 ], "6236640000000005082", 19);
strncpy(acct[498 ], "6236640000000005090", 19);
strncpy(acct[499 ], "6236640000000005108", 19);
strncpy(acct[500 ], "6236640000000005116", 19);
strncpy(acct[501 ], "6236640000000005124", 19);
strncpy(acct[502 ], "6236640000000005132", 19);
strncpy(acct[503 ], "6236640000000005140", 19);
strncpy(acct[504 ], "6236640000000005157", 19);
strncpy(acct[505 ], "6236640000000005165", 19);
strncpy(acct[506 ], "6236640000000005173", 19);
strncpy(acct[507 ], "6236640000000005181", 19);
strncpy(acct[508 ], "6236640000000005199", 19);
strncpy(acct[509 ], "6236640000000005207", 19);
strncpy(acct[510 ], "6236640000000005215", 19);
strncpy(acct[511 ], "6236640000000005223", 19);
strncpy(acct[512 ], "6236640000000005231", 19);
strncpy(acct[513 ], "6236640000000005249", 19);
strncpy(acct[514 ], "6236640000000005256", 19);
strncpy(acct[515 ], "6236640000000005264", 19);
strncpy(acct[516 ], "6236640000000005272", 19);
strncpy(acct[517 ], "6236640000000005280", 19);
strncpy(acct[518 ], "6236640000000005298", 19);
strncpy(acct[519 ], "6236640000000005306", 19);
strncpy(acct[520 ], "6236640000000005314", 19);
strncpy(acct[521 ], "6236640000000005322", 19);
strncpy(acct[522 ], "6236640000000005330", 19);
strncpy(acct[523 ], "6236640000000005348", 19);
strncpy(acct[524 ], "6236640000000005355", 19);
strncpy(acct[525 ], "6236640000000005363", 19);
strncpy(acct[526 ], "6236640000000005389", 19);
strncpy(acct[527 ], "6236640000000005397", 19);
strncpy(acct[528 ], "6236640000000005405", 19);
strncpy(acct[529 ], "6236640000000005413", 19);
strncpy(acct[530 ], "6236640000000005421", 19);
strncpy(acct[531 ], "6236640000000005439", 19);
strncpy(acct[532 ], "6236640000000005447", 19);
strncpy(acct[533 ], "6236640000000005454", 19);
strncpy(acct[534 ], "6236640000000005462", 19);
strncpy(acct[535 ], "6236640000000005470", 19);
strncpy(acct[536 ], "6236640000000005488", 19);
strncpy(acct[537 ], "6236640000000005496", 19);
strncpy(acct[538 ], "6236640000000005504", 19);
strncpy(acct[539 ], "6236640000000005512", 19);
strncpy(acct[540 ], "6236640000000005520", 19);
strncpy(acct[541 ], "6236640000000005538", 19);
strncpy(acct[542 ], "6236640000000005546", 19);
strncpy(acct[543 ], "6236640000000005553", 19);
strncpy(acct[544 ], "6236640000000005561", 19);
strncpy(acct[545 ], "6236640000000005579", 19);
strncpy(acct[546 ], "6236640000000005587", 19);
strncpy(acct[547 ], "6236640000000005595", 19);
strncpy(acct[548 ], "6236640000000005603", 19);
strncpy(acct[549 ], "6236640000000005629", 19);
strncpy(acct[550 ], "6236640000000005637", 19);
strncpy(acct[551 ], "6236640000000005645", 19);
strncpy(acct[552 ], "6236640000000005652", 19);
strncpy(acct[553 ], "6236640000000005660", 19);
strncpy(acct[554 ], "6236640000000005678", 19);
strncpy(acct[555 ], "6236640000000005686", 19);
strncpy(acct[556 ], "6236640000000005694", 19);
strncpy(acct[557 ], "6236640000000005702", 19);
strncpy(acct[558 ], "6236640000000005710", 19);
strncpy(acct[559 ], "6236640000000005736", 19);
strncpy(acct[560 ], "6236640000000005744", 19);
strncpy(acct[561 ], "6236640000000005751", 19);
strncpy(acct[562 ], "6236640000000005769", 19);
strncpy(acct[563 ], "6236640000000005777", 19);
strncpy(acct[564 ], "6236640000000005801", 19);
strncpy(acct[565 ], "6236640000000005819", 19);
strncpy(acct[566 ], "6236640000000005827", 19);
strncpy(acct[567 ], "6236640000000005835", 19);
strncpy(acct[568 ], "6236640000000005843", 19);
strncpy(acct[569 ], "6236640000000005850", 19);
strncpy(acct[570 ], "6236640000000005868", 19);
strncpy(acct[571 ], "6236640000000005876", 19);
strncpy(acct[572 ], "6236640000000005884", 19);
strncpy(acct[573 ], "6236640000000005892", 19);
strncpy(acct[574 ], "6236640000000005900", 19);
strncpy(acct[575 ], "6236640000000005918", 19);
strncpy(acct[576 ], "6236640000000005926", 19);
strncpy(acct[577 ], "6236640000000005934", 19);
strncpy(acct[578 ], "6236640000000005942", 19);
strncpy(acct[579 ], "6236640000000005959", 19);
strncpy(acct[580 ], "6236640000000005967", 19);
strncpy(acct[581 ], "6236640000000005975", 19);
strncpy(acct[582 ], "6236640000000005983", 19);
strncpy(acct[583 ], "6236640000000005991", 19);
strncpy(acct[584 ], "6236640000000006007", 19);
strncpy(acct[585 ], "6236640000000006015", 19);
strncpy(acct[586 ], "6236640000000006023", 19);
strncpy(acct[587 ], "6236640000000006031", 19);
strncpy(acct[588 ], "6236640000000006049", 19);
strncpy(acct[589 ], "6236640000000006056", 19);
strncpy(acct[590 ], "6236640000000006064", 19);
strncpy(acct[591 ], "6236640000000006072", 19);
strncpy(acct[592 ], "6236640000000006080", 19);
strncpy(acct[593 ], "6236640000000006098", 19);
strncpy(acct[594 ], "6236640000000006106", 19);
strncpy(acct[595 ], "6236640000000006114", 19);
strncpy(acct[596 ], "6236640000000006122", 19);
strncpy(acct[597 ], "6236640000000006130", 19);
strncpy(acct[598 ], "6236640000000006148", 19);
strncpy(acct[599 ], "6236640000000006155", 19);
strncpy(acct[600 ], "6236640000000006163", 19);
strncpy(acct[601 ], "6236640000000006171", 19);
strncpy(acct[602 ], "6236640000000006189", 19);
strncpy(acct[603 ], "6236640000000006197", 19);
strncpy(acct[604 ], "6236640000000006205", 19);
strncpy(acct[605 ], "6236640000000006213", 19);
strncpy(acct[606 ], "6236640000000006221", 19);
strncpy(acct[607 ], "6236640000000006239", 19);
strncpy(acct[608 ], "6236640000000006247", 19);
strncpy(acct[609 ], "6236640000000006254", 19);
strncpy(acct[610 ], "6236640000000006262", 19);
strncpy(acct[611 ], "6236640000000006270", 19);
strncpy(acct[612 ], "6236640000000006288", 19);
strncpy(acct[613 ], "6236640000000006296", 19);
strncpy(acct[614 ], "6236640000000006304", 19);
strncpy(acct[615 ], "6236640000000006312", 19);
strncpy(acct[616 ], "6236640000000006320", 19);
strncpy(acct[617 ], "6236640000000006338", 19);
strncpy(acct[618 ], "6236640000000006346", 19);
strncpy(acct[619 ], "6236640000000006353", 19);
strncpy(acct[620 ], "6236640000000006361", 19);
strncpy(acct[621 ], "6236640000000006379", 19);
strncpy(acct[622 ], "6236640000000006387", 19);
strncpy(acct[623 ], "6236640000000006395", 19);
strncpy(acct[624 ], "6236640000000006403", 19);
strncpy(acct[625 ], "6236640000000006411", 19);
strncpy(acct[626 ], "6236640000000006429", 19);
strncpy(acct[627 ], "6236640000000006437", 19);
strncpy(acct[628 ], "6236640000000006445", 19);
strncpy(acct[629 ], "6236640000000006452", 19);
strncpy(acct[630 ], "6236640000000006460", 19);
strncpy(acct[631 ], "6236640000000006478", 19);
strncpy(acct[632 ], "6236640000000006486", 19);
strncpy(acct[633 ], "6236640000000006494", 19);
strncpy(acct[634 ], "6236640000000006502", 19);
strncpy(acct[635 ], "6236640000000006510", 19);
strncpy(acct[636 ], "6236640000000006528", 19);
strncpy(acct[637 ], "6236640000000006536", 19);
strncpy(acct[638 ], "6236640000000006544", 19);
strncpy(acct[639 ], "6236640000000006551", 19);
strncpy(acct[640 ], "6236640000000006569", 19);
strncpy(acct[641 ], "6236640000000006577", 19);
strncpy(acct[642 ], "6236640000000006585", 19);
strncpy(acct[643 ], "6236640000000006593", 19);
strncpy(acct[644 ], "6236640000000006601", 19);
strncpy(acct[645 ], "6236640000000006619", 19);
strncpy(acct[646 ], "6236640000000006627", 19);
strncpy(acct[647 ], "6236640000000006635", 19);
strncpy(acct[648 ], "6236640000000006643", 19);
strncpy(acct[649 ], "6236640000000006650", 19);
strncpy(acct[650 ], "6236640000000006668", 19);
strncpy(acct[651 ], "6236640000000006676", 19);
strncpy(acct[652 ], "6236640000000006684", 19);
strncpy(acct[653 ], "6236640000000006692", 19);
strncpy(acct[654 ], "6236640000000006700", 19);
strncpy(acct[655 ], "6236640000000006718", 19);
strncpy(acct[656 ], "6236640000000006726", 19);
strncpy(acct[657 ], "6236640000000006734", 19);
strncpy(acct[658 ], "6236640000000006742", 19);
strncpy(acct[659 ], "6236640000000006759", 19);
strncpy(acct[660 ], "6236640000000006767", 19);
strncpy(acct[661 ], "6236640000000006775", 19);
strncpy(acct[662 ], "6236640000000006783", 19);
strncpy(acct[663 ], "6236640000000006791", 19);
strncpy(acct[664 ], "6236640000000006809", 19);
strncpy(acct[665 ], "6236640000000006817", 19);
strncpy(acct[666 ], "6236640000000006825", 19);
strncpy(acct[667 ], "6236640000000006833", 19);
strncpy(acct[668 ], "6236640000000006841", 19);
strncpy(acct[669 ], "6236640000000006858", 19);
strncpy(acct[670 ], "6236640000000006866", 19);
strncpy(acct[671 ], "6236640000000006882", 19);
strncpy(acct[672 ], "6236640000000006908", 19);
strncpy(acct[673 ], "6236640000000006916", 19);
strncpy(acct[674 ], "6236640000000006924", 19);
strncpy(acct[675 ], "6236640000000006932", 19);
strncpy(acct[676 ], "6236640000000006940", 19);
strncpy(acct[677 ], "6236640000000006957", 19);
strncpy(acct[678 ], "6236640000000006965", 19);
strncpy(acct[679 ], "6236640000000006973", 19);
strncpy(acct[680 ], "6236640000000006981", 19);
strncpy(acct[681 ], "6236640000000006999", 19);
strncpy(acct[682 ], "6236640000000007005", 19);
strncpy(acct[683 ], "6236640000000007013", 19);
strncpy(acct[684 ], "6236640000000007021", 19);
strncpy(acct[685 ], "6236640000000007039", 19);
strncpy(acct[686 ], "6236640000000007047", 19);
strncpy(acct[687 ], "6236640000000007054", 19);
strncpy(acct[688 ], "6236640000000007062", 19);
strncpy(acct[689 ], "6236640000000007070", 19);
strncpy(acct[690 ], "6236640000000007088", 19);
strncpy(acct[691 ], "6236640000000007096", 19);
strncpy(acct[692 ], "6236640000000007104", 19);
strncpy(acct[693 ], "6236640000000007112", 19);
strncpy(acct[694 ], "6236640000000007120", 19);
strncpy(acct[695 ], "6236640000000007138", 19);
strncpy(acct[696 ], "6236640000000007146", 19);
strncpy(acct[697 ], "6236640000000007153", 19);
strncpy(acct[698 ], "6236640000000007161", 19);
strncpy(acct[699 ], "6236640000000007179", 19);
strncpy(acct[700 ], "6236640000000007187", 19);
strncpy(acct[701 ], "6236640000000007195", 19);
strncpy(acct[702 ], "6236640000000007203", 19);
strncpy(acct[703 ], "6236640000000007211", 19);
strncpy(acct[704 ], "6236640000000007229", 19);
strncpy(acct[705 ], "6236640000000007237", 19);
strncpy(acct[706 ], "6236640000000007245", 19);
strncpy(acct[707 ], "6236640000000007252", 19);
strncpy(acct[708 ], "6236640000000007260", 19);
strncpy(acct[709 ], "6236640000000007278", 19);
strncpy(acct[710 ], "6236640000000007286", 19);
strncpy(acct[711 ], "6236640000000007294", 19);
strncpy(acct[712 ], "6236640000000007302", 19);
strncpy(acct[713 ], "6236640000000007310", 19);
strncpy(acct[714 ], "6236640000000007328", 19);
strncpy(acct[715 ], "6236640000000007336", 19);
strncpy(acct[716 ], "6236640000000007344", 19);
strncpy(acct[717 ], "6236640000000007351", 19);
strncpy(acct[718 ], "6236640000000007369", 19);
strncpy(acct[719 ], "6236640000000007377", 19);
strncpy(acct[720 ], "6236640000000007385", 19);
strncpy(acct[721 ], "6236640000000007393", 19);
strncpy(acct[722 ], "6236640000000007401", 19);
strncpy(acct[723 ], "6236640000000007419", 19);
strncpy(acct[724 ], "6236640000000007427", 19);
strncpy(acct[725 ], "6236640000000007435", 19);
strncpy(acct[726 ], "6236640000000007450", 19);
strncpy(acct[727 ], "6236640000000007468", 19);
strncpy(acct[728 ], "6236640000000007476", 19);
strncpy(acct[729 ], "6236640000000007484", 19);
strncpy(acct[730 ], "6236640000000007492", 19);
strncpy(acct[731 ], "6236640000000007500", 19);
strncpy(acct[732 ], "6236640000000007518", 19);
strncpy(acct[733 ], "6236640000000007526", 19);
strncpy(acct[734 ], "6236640000000007534", 19);
strncpy(acct[735 ], "6236640000000007542", 19);
strncpy(acct[736 ], "6236640000000007559", 19);
strncpy(acct[737 ], "6236640000000007567", 19);
strncpy(acct[738 ], "6236640000000007575", 19);
strncpy(acct[739 ], "6236640000000007583", 19);
strncpy(acct[740 ], "6236640000000007591", 19);
strncpy(acct[741 ], "6236640000000007609", 19);
strncpy(acct[742 ], "6236640000000007617", 19);
strncpy(acct[743 ], "6236640000000007625", 19);
strncpy(acct[744 ], "6236640000000007633", 19);
strncpy(acct[745 ], "6236640000000007641", 19);
strncpy(acct[746 ], "6236640000000007658", 19);
strncpy(acct[747 ], "6236640000000007666", 19);
strncpy(acct[748 ], "6236640000000007674", 19);
strncpy(acct[749 ], "6236640000000007682", 19);
strncpy(acct[750 ], "6236640000000007690", 19);
strncpy(acct[751 ], "6236640000000007708", 19);
strncpy(acct[752 ], "6236640000000007716", 19);
strncpy(acct[753 ], "6236640000000007724", 19);
strncpy(acct[754 ], "6236640000000007732", 19);
strncpy(acct[755 ], "6236640000000007740", 19);
strncpy(acct[756 ], "6236640000000007757", 19);
strncpy(acct[757 ], "6236640000000007765", 19);
strncpy(acct[758 ], "6236640000000007773", 19);
strncpy(acct[759 ], "6236640000000007781", 19);
strncpy(acct[760 ], "6236640000000007799", 19);
strncpy(acct[761 ], "6236640000000007807", 19);
strncpy(acct[762 ], "6236640000000007815", 19);
strncpy(acct[763 ], "6236640000000007823", 19);
strncpy(acct[764 ], "6236640000000007831", 19);
strncpy(acct[765 ], "6236640000000007849", 19);
strncpy(acct[766 ], "6236640000000007856", 19);
strncpy(acct[767 ], "6236640000000007864", 19);
strncpy(acct[768 ], "6236640000000007872", 19);
strncpy(acct[769 ], "6236640000000007880", 19);
strncpy(acct[770 ], "6236640000000007898", 19);
strncpy(acct[771 ], "6236640000000007906", 19);
strncpy(acct[772 ], "6236640000000007914", 19);
strncpy(acct[773 ], "6236640000000007922", 19);
strncpy(acct[774 ], "6236640000000007930", 19);
strncpy(acct[775 ], "6236640000000007948", 19);
strncpy(acct[776 ], "6236640000000007955", 19);
strncpy(acct[777 ], "6236640000000007963", 19);
strncpy(acct[778 ], "6236640000000007971", 19);
strncpy(acct[779 ], "6236640000000007989", 19);
strncpy(acct[780 ], "6236640000000007997", 19);
strncpy(acct[781 ], "6236640000000008003", 19);
strncpy(acct[782 ], "6236640000000008011", 19);
strncpy(acct[783 ], "6236640000000008029", 19);
strncpy(acct[784 ], "6236640000000008037", 19);
strncpy(acct[785 ], "6236640000000008045", 19);
strncpy(acct[786 ], "6236640000000008052", 19);
strncpy(acct[787 ], "6236640000000008060", 19);
strncpy(acct[788 ], "6236640000000008078", 19);
strncpy(acct[789 ], "6236640000000008086", 19);
strncpy(acct[790 ], "6236640000000008094", 19);
strncpy(acct[791 ], "6236640000000008102", 19);
strncpy(acct[792 ], "6236640000000008110", 19);
strncpy(acct[793 ], "6236640000000008128", 19);
strncpy(acct[794 ], "6236640000000008136", 19);
strncpy(acct[795 ], "6236640000000008144", 19);
strncpy(acct[796 ], "6236640000000008151", 19);
strncpy(acct[797 ], "6236640000000008169", 19);
strncpy(acct[798 ], "6236640000000008177", 19);
strncpy(acct[799 ], "6236640000000008185", 19);
strncpy(acct[800 ], "6236640000000008193", 19);
strncpy(acct[801 ], "6236640000000008201", 19);
strncpy(acct[802 ], "6236640000000008219", 19);
strncpy(acct[803 ], "6236640000000008227", 19);
strncpy(acct[804 ], "6236640000000008235", 19);
strncpy(acct[805 ], "6236640000000008243", 19);
strncpy(acct[806 ], "6236640000000008250", 19);
strncpy(acct[807 ], "6236640000000008268", 19);
strncpy(acct[808 ], "6236640000000008276", 19);
strncpy(acct[809 ], "6236640000000008284", 19);
strncpy(acct[810 ], "6236640000000008292", 19);
strncpy(acct[811 ], "6236640000000008300", 19);
strncpy(acct[812 ], "6236640000000008318", 19);
strncpy(acct[813 ], "6236640000000008326", 19);
strncpy(acct[814 ], "6236640000000008334", 19);
strncpy(acct[815 ], "6236640000000008342", 19);
strncpy(acct[816 ], "6236640000000008359", 19);
strncpy(acct[817 ], "6236640000000008367", 19);
strncpy(acct[818 ], "6236640000000008375", 19);
strncpy(acct[819 ], "6236640000000008383", 19);
strncpy(acct[820 ], "6236640000000008391", 19);
strncpy(acct[821 ], "6236640000000008409", 19);
strncpy(acct[822 ], "6236640000000008417", 19);
strncpy(acct[823 ], "6236640000000008425", 19);
strncpy(acct[824 ], "6236640000000008433", 19);
strncpy(acct[825 ], "6236640000000008441", 19);
strncpy(acct[826 ], "6236640000000008458", 19);
strncpy(acct[827 ], "6236640000000008466", 19);
strncpy(acct[828 ], "6236640000000008474", 19);
strncpy(acct[829 ], "6236640000000008482", 19);
strncpy(acct[830 ], "6236640000000008490", 19);
strncpy(acct[831 ], "6236640000000008508", 19);
strncpy(acct[832 ], "6236640000000008516", 19);
strncpy(acct[833 ], "6236640000000008524", 19);
strncpy(acct[834 ], "6236640000000008532", 19);
strncpy(acct[835 ], "6236640000000008540", 19);
strncpy(acct[836 ], "6236640000000008557", 19);
strncpy(acct[837 ], "6236640000000008565", 19);
strncpy(acct[838 ], "6236640000000008573", 19);
strncpy(acct[839 ], "6236640000000008581", 19);
strncpy(acct[840 ], "6236640000000008599", 19);
strncpy(acct[841 ], "6236640000000008607", 19);
strncpy(acct[842 ], "6236640000000008615", 19);
strncpy(acct[843 ], "6236640000000008623", 19);
strncpy(acct[844 ], "6236640000000008631", 19);
strncpy(acct[845 ], "6236640000000008649", 19);
strncpy(acct[846 ], "6236640000000008656", 19);
strncpy(acct[847 ], "6236640000000008664", 19);
strncpy(acct[848 ], "6236640000000008672", 19);
strncpy(acct[849 ], "6236640000000008680", 19);
strncpy(acct[850 ], "6236640000000008698", 19);
strncpy(acct[851 ], "6236640000000008706", 19);
strncpy(acct[852 ], "6236640000000008714", 19);
strncpy(acct[853 ], "6236640000000008722", 19);
strncpy(acct[854 ], "6236640000000008730", 19);
strncpy(acct[855 ], "6236640000000008755", 19);
strncpy(acct[856 ], "6236640000000008763", 19);
strncpy(acct[857 ], "6236640000000008771", 19);
strncpy(acct[858 ], "6236640000000008789", 19);
strncpy(acct[859 ], "6236640000000008797", 19);
strncpy(acct[860 ], "6236640000000008805", 19);
strncpy(acct[861 ], "6236640000000008813", 19);
strncpy(acct[862 ], "6236640000000008821", 19);
strncpy(acct[863 ], "6236640000000008839", 19);
strncpy(acct[864 ], "6236640000000008847", 19);
strncpy(acct[865 ], "6236640000000008854", 19);
strncpy(acct[866 ], "6236640000000008862", 19);
strncpy(acct[867 ], "6236640000000008870", 19);
strncpy(acct[868 ], "6236640000000008888", 19);
strncpy(acct[869 ], "6236640000000008896", 19);
strncpy(acct[870 ], "6236640000000008904", 19);
strncpy(acct[871 ], "6236640000000008912", 19);
strncpy(acct[872 ], "6236640000000008920", 19);
strncpy(acct[873 ], "6236640000000008938", 19);
strncpy(acct[874 ], "6236640000000008946", 19);
strncpy(acct[875 ], "6236640000000008953", 19);
strncpy(acct[876 ], "6236640000000008961", 19);
strncpy(acct[877 ], "6236640000000008979", 19);
strncpy(acct[878 ], "6236640000000008987", 19);
strncpy(acct[879 ], "6236640000000008995", 19);
strncpy(acct[880 ], "6236640000000009001", 19);
strncpy(acct[881 ], "6236640000000009019", 19);
strncpy(acct[882 ], "6236640000000009027", 19);
strncpy(acct[883 ], "6236640000000009035", 19);
strncpy(acct[884 ], "6236640000000009043", 19);
strncpy(acct[885 ], "6236640000000009050", 19);
strncpy(acct[886 ], "6236640000000009068", 19);
strncpy(acct[887 ], "6236640000000009076", 19);
strncpy(acct[888 ], "6236640000000009084", 19);
strncpy(acct[889 ], "6236640000000009092", 19);
strncpy(acct[890 ], "6236640000000009100", 19);
strncpy(acct[891 ], "6236640000000009118", 19);
strncpy(acct[892 ], "6236640000000009126", 19);
strncpy(acct[893 ], "6236640000000009134", 19);
strncpy(acct[894 ], "6236640000000009142", 19);
strncpy(acct[895 ], "6236640000000009159", 19);
strncpy(acct[896 ], "6236640000000009167", 19);
strncpy(acct[897 ], "6236640000000009175", 19);
strncpy(acct[898 ], "6236640000000009183", 19);
strncpy(acct[899 ], "6236640000000009191", 19);
strncpy(acct[900 ], "6236640000000009209", 19);
strncpy(acct[901 ], "6236640000000009217", 19);
strncpy(acct[902 ], "6236640000000009225", 19);
strncpy(acct[903 ], "6236640000000009233", 19);
strncpy(acct[904 ], "6236640000000009241", 19);
strncpy(acct[905 ], "6236640000000009258", 19);
strncpy(acct[906 ], "6236640000000009266", 19);
strncpy(acct[907 ], "6236640000000009274", 19);
strncpy(acct[908 ], "6236640000000009282", 19);
strncpy(acct[909 ], "6236640000000009290", 19);
strncpy(acct[910 ], "6236640000000009308", 19);
strncpy(acct[911 ], "6236640000000009316", 19);
strncpy(acct[912 ], "6236640000000009324", 19);
strncpy(acct[913 ], "6236640000000009332", 19);
strncpy(acct[914 ], "6236640000000009340", 19);
strncpy(acct[915 ], "6236640000000009357", 19);
strncpy(acct[916 ], "6236640000000009365", 19);
strncpy(acct[917 ], "6236640000000009373", 19);
strncpy(acct[918 ], "6236640000000009381", 19);
strncpy(acct[919 ], "6236640000000009399", 19);
strncpy(acct[920 ], "6236640000000009407", 19);
strncpy(acct[921 ], "6236640000000009415", 19);
strncpy(acct[922 ], "6236640000000009423", 19);
strncpy(acct[923 ], "6236640000000009431", 19);
strncpy(acct[924 ], "6236640000000009449", 19);
strncpy(acct[925 ], "6236640000000009456", 19);
strncpy(acct[926 ], "6236640000000009464", 19);
strncpy(acct[927 ], "6236640000000009472", 19);
strncpy(acct[928 ], "6236640000000009480", 19);
strncpy(acct[929 ], "6236640000000009498", 19);
strncpy(acct[930 ], "6236640000000009506", 19);
strncpy(acct[931 ], "6236640000000009514", 19);
strncpy(acct[932 ], "6236640000000009522", 19);
strncpy(acct[933 ], "6236640000000009530", 19);
strncpy(acct[934 ], "6236640000000009548", 19);
strncpy(acct[935 ], "6236640000000009555", 19);
strncpy(acct[936 ], "6236640000000009563", 19);
strncpy(acct[937 ], "6236640000000009571", 19);
strncpy(acct[938 ], "6236640000000009589", 19);
strncpy(acct[939 ], "6236640000000009597", 19);
strncpy(acct[940 ], "6236640000000009605", 19);
strncpy(acct[941 ], "6236640000000009613", 19);
strncpy(acct[942 ], "6236640000000009621", 19);
strncpy(acct[943 ], "6236640000000009639", 19);
strncpy(acct[944 ], "6236640000000009647", 19);
strncpy(acct[945 ], "6236640000000009654", 19);
strncpy(acct[946 ], "6236640000000009662", 19);
strncpy(acct[947 ], "6236640000000009670", 19);
strncpy(acct[948 ], "6236640000000009688", 19);
strncpy(acct[949 ], "6236640000000009696", 19);
strncpy(acct[950 ], "6236640000000009704", 19);
strncpy(acct[951 ], "6236640000000009712", 19);
strncpy(acct[952 ], "6236640000000009720", 19);
strncpy(acct[953 ], "6236640000000009738", 19);
strncpy(acct[954 ], "6236640000000009746", 19);
strncpy(acct[955 ], "6236640000000009753", 19);
strncpy(acct[956 ], "6236640000000009761", 19);
strncpy(acct[957 ], "6236640000000009779", 19);
strncpy(acct[958 ], "6236640000000009787", 19);
strncpy(acct[959 ], "6236640000000009795", 19);
strncpy(acct[960 ], "6236640000000009803", 19);
strncpy(acct[961 ], "6236640000000009811", 19);
strncpy(acct[962 ], "6236640000000009829", 19);
strncpy(acct[963 ], "6236640000000009837", 19);
strncpy(acct[964 ], "6236640000000009845", 19);
strncpy(acct[965 ], "6236640000000009852", 19);
strncpy(acct[966 ], "6236640000000009860", 19);
strncpy(acct[967 ], "6236640000000009878", 19);
strncpy(acct[968 ], "6236640000000009886", 19);
strncpy(acct[969 ], "6236640000000009894", 19);
strncpy(acct[970 ], "6236640000000009902", 19);
strncpy(acct[971 ], "6236640000000009910", 19);
strncpy(acct[972 ], "6236640000000009928", 19);
strncpy(acct[973 ], "6236640000000009936", 19);
strncpy(acct[974 ], "6236640000000009944", 19);
strncpy(acct[975 ], "6236640000000009951", 19);
strncpy(acct[976 ], "6236640000000009969", 19);
strncpy(acct[977 ], "6236640000000009977", 19);
strncpy(acct[978 ], "6236640000000009985", 19);
strncpy(acct[979 ], "6236640000000009993", 19);
strncpy(acct[980 ], "6236640000000010009", 19);
strncpy(acct[981 ], "6236640000000010017", 19);
strncpy(acct[982 ], "6236640000000010025", 19);
strncpy(acct[983 ], "6236640000000010033", 19);
strncpy(acct[984 ], "6236640000000010041", 19);
strncpy(acct[985 ], "6236640000000010058", 19);
strncpy(acct[986 ], "6236640000000010066", 19);
strncpy(acct[987 ], "6236640000000010074", 19);
strncpy(acct[988 ], "6236640000000010082", 19);
strncpy(acct[989 ], "6236640000000010090", 19);
strncpy(acct[990 ], "6236640000000010108", 19);
strncpy(acct[991 ], "6236640000000010116", 19);
strncpy(acct[992 ], "6236640000000010124", 19);
strncpy(acct[993 ], "6236640000000010132", 19);
strncpy(acct[994 ], "6236640000000010140", 19);
strncpy(acct[995 ], "6236640000000010157", 19);
strncpy(acct[996 ], "6236640000000010165", 19);
strncpy(acct[997 ], "6236640000000010173", 19);
strncpy(acct[998 ], "6236640000000010181", 19);
strncpy(acct[999 ], "6236640000000010199", 19);
strncpy(acct[1000], "6236640000000010207", 19);
strncpy(acct[1001], "6236640000000010215", 19);
strncpy(acct[1002], "6236640000000010223", 19);
strncpy(acct[1003], "6236640000000010231", 19);
strncpy(acct[1004], "6236640000000010249", 19);
strncpy(acct[1005], "6236640000000010256", 19);
strncpy(acct[1006], "6236640000000010264", 19);
strncpy(acct[1007], "6236640000000010272", 19);
strncpy(acct[1008], "6236640000000010280", 19);
strncpy(acct[1009], "6236640000000010298", 19);
strncpy(acct[1010], "6236640000000010306", 19);
strncpy(acct[1011], "6236640000000010314", 19);
strncpy(acct[1012], "6236640000000010322", 19);
strncpy(acct[1013], "6236640000000010330", 19);
strncpy(acct[1014], "6236640000000010348", 19);
strncpy(acct[1015], "6236640000000010355", 19);
strncpy(acct[1016], "6236640000000010363", 19);
strncpy(acct[1017], "6236640000000010371", 19);
strncpy(acct[1018], "6236640000000010389", 19);
strncpy(acct[1019], "6236640000000010397", 19);
strncpy(acct[1020], "6236640000000010405", 19);
strncpy(acct[1021], "6236640000000010413", 19);
strncpy(acct[1022], "6236640000000010421", 19);
strncpy(acct[1023], "6236640000000010439", 19);
strncpy(acct[1024], "6236640000000010447", 19);
strncpy(acct[1025], "6236640000000010454", 19);
strncpy(acct[1026], "6236640000000010462", 19);
strncpy(acct[1027], "6236640000000010470", 19);
strncpy(acct[1028], "6236640000000010488", 19);
strncpy(acct[1029], "6236640000000010504", 19);
strncpy(acct[1030], "6236640000000010512", 19);
strncpy(acct[1031], "6236640000000010520", 19);
strncpy(acct[1032], "6236640000000010538", 19);
strncpy(acct[1033], "6236640000000010546", 19);
strncpy(acct[1034], "6236640000000010553", 19);
strncpy(acct[1035], "6236640000000010561", 19);
strncpy(acct[1036], "6236640000000010579", 19);
strncpy(acct[1037], "6236640000000010587", 19);
strncpy(acct[1038], "6236640000000010595", 19);
strncpy(acct[1039], "6236640000000010603", 19);
strncpy(acct[1040], "6236640000000010611", 19);
strncpy(acct[1041], "6236640000000010629", 19);
strncpy(acct[1042], "6236640000000010637", 19);
strncpy(acct[1043], "6236640000000010645", 19);
strncpy(acct[1044], "6236640000000010652", 19);
strncpy(acct[1045], "6236640000000010660", 19);
strncpy(acct[1046], "6236640000000010678", 19);
strncpy(acct[1047], "6236640000000010686", 19);
strncpy(acct[1048], "6236640000000010694", 19);
strncpy(acct[1049], "6236640000000010702", 19);
strncpy(acct[1050], "6236640000000010710", 19);
strncpy(acct[1051], "6236640000000010728", 19);
strncpy(acct[1052], "6236640000000010736", 19);
strncpy(acct[1053], "6236640000000010744", 19);
strncpy(acct[1054], "6236640000000010751", 19);
strncpy(acct[1055], "6236640000000010769", 19);
strncpy(acct[1056], "6236640000000010777", 19);
strncpy(acct[1057], "6236640000000010785", 19);
strncpy(acct[1058], "6236640000000010793", 19);
strncpy(acct[1059], "6236640000000010801", 19);
strncpy(acct[1060], "6236640000000010819", 19);
strncpy(acct[1061], "6236640000000010827", 19);
strncpy(acct[1062], "6236640000000010835", 19);
strncpy(acct[1063], "6236640000000010843", 19);
strncpy(acct[1064], "6236640000000010850", 19);
strncpy(acct[1065], "6236640000000010868", 19);
strncpy(acct[1066], "6236640000000010876", 19);
strncpy(acct[1067], "6236640000000010884", 19);
strncpy(acct[1068], "6236640000000010892", 19);
strncpy(acct[1069], "6236640000000010900", 19);
strncpy(acct[1070], "6236640000000010918", 19);
strncpy(acct[1071], "6236640000000010926", 19);
strncpy(acct[1072], "6236640000000010934", 19);
strncpy(acct[1073], "6236640000000010942", 19);
strncpy(acct[1074], "6236640000000010959", 19);
strncpy(acct[1075], "6236640000000010967", 19);
strncpy(acct[1076], "6236640000000010975", 19);
strncpy(acct[1077], "6236640000000010983", 19);
strncpy(acct[1078], "6236640000000010991", 19);
strncpy(acct[1079], "6236640000000011007", 19);
strncpy(acct[1080], "6236640000000011015", 19);
strncpy(acct[1081], "6236640000000011023", 19);
strncpy(acct[1082], "6236640000000011031", 19);
strncpy(acct[1083], "6236640000000011049", 19);
strncpy(acct[1084], "6236640000000011056", 19);
strncpy(acct[1085], "6236640000000011064", 19);
strncpy(acct[1086], "6236640000000011072", 19);
strncpy(acct[1087], "6236640000000011080", 19);
strncpy(acct[1088], "6236640000000011098", 19);
strncpy(acct[1089], "6236640000000011106", 19);
strncpy(acct[1090], "6236640000000011114", 19);
strncpy(acct[1091], "6236640000000011122", 19);
strncpy(acct[1092], "6236640000000011130", 19);
strncpy(acct[1093], "6236640000000011148", 19);
strncpy(acct[1094], "6236640000000011155", 19);
strncpy(acct[1095], "6236640000000011163", 19);
strncpy(acct[1096], "6236640000000011171", 19);
strncpy(acct[1097], "6236640000000011189", 19);
strncpy(acct[1098], "6236640000000011197", 19);
strncpy(acct[1099], "6236640000000011205", 19);
strncpy(acct[1100], "6236640000000011213", 19);
strncpy(acct[1101], "6236640000000011221", 19);
strncpy(acct[1102], "6236640000000011239", 19);
strncpy(acct[1103], "6236640000000011247", 19);
strncpy(acct[1104], "6236640000000011254", 19);
strncpy(acct[1105], "6236640000000011262", 19);
strncpy(acct[1106], "6236640000000011270", 19);
strncpy(acct[1107], "6236640000000011288", 19);
strncpy(acct[1108], "6236640000000011296", 19);
strncpy(acct[1109], "6236640000000011304", 19);
strncpy(acct[1110], "6236640000000011312", 19);
strncpy(acct[1111], "6236640000000011320", 19);
strncpy(acct[1112], "6236640000000011338", 19);
strncpy(acct[1113], "6236640000000011346", 19);
strncpy(acct[1114], "6236640000000011353", 19);
strncpy(acct[1115], "6236640000000011361", 19);
strncpy(acct[1116], "6236640000000011387", 19);
strncpy(acct[1117], "6236640000000011395", 19);
strncpy(acct[1118], "6236640000000011403", 19);
strncpy(acct[1119], "6236640000000011411", 19);
strncpy(acct[1120], "6236640000000011437", 19);
strncpy(acct[1121], "6236640000000011445", 19);
strncpy(acct[1122], "6236640000000011452", 19);
strncpy(acct[1123], "6236640000000011460", 19);
strncpy(acct[1124], "6236640000000011478", 19);
strncpy(acct[1125], "6236640000000011486", 19);
strncpy(acct[1126], "6236640000000011494", 19);
strncpy(acct[1127], "6236640000000011502", 19);
strncpy(acct[1128], "6236640000000011510", 19);
strncpy(acct[1129], "6236640000000011528", 19);
strncpy(acct[1130], "6236640000000011536", 19);
strncpy(acct[1131], "6236640000000011544", 19);
strncpy(acct[1132], "6236640000000011551", 19);
strncpy(acct[1133], "6236640000000011569", 19);
strncpy(acct[1134], "6236640000000011577", 19);
strncpy(acct[1135], "6236640000000011585", 19);
strncpy(acct[1136], "6236640000000011593", 19);
strncpy(acct[1137], "6236640000000011601", 19);
strncpy(acct[1138], "6236640000000011627", 19);
strncpy(acct[1139], "6236640000000011635", 19);
strncpy(acct[1140], "6236640000000011643", 19);
strncpy(acct[1141], "6236640000000011650", 19);
strncpy(acct[1142], "6236640000000011668", 19);
strncpy(acct[1143], "6236640000000011676", 19);
strncpy(acct[1144], "6236640000000011684", 19);
strncpy(acct[1145], "6236640000000011692", 19);
strncpy(acct[1146], "6236640000000011700", 19);
strncpy(acct[1147], "6236640000000011718", 19);
strncpy(acct[1148], "6236640000000011726", 19);
strncpy(acct[1149], "6236640000000011734", 19);
strncpy(acct[1150], "6236640000000011742", 19);
strncpy(acct[1151], "6236640000000011759", 19);
strncpy(acct[1152], "6236640000000011767", 19);
strncpy(acct[1153], "6236640000000011775", 19);
strncpy(acct[1154], "6236640000000011783", 19);
strncpy(acct[1155], "6236640000000011791", 19);
strncpy(acct[1156], "6236640000000011809", 19);
strncpy(acct[1157], "6236640000000011817", 19);
strncpy(acct[1158], "6236640000000011825", 19);
strncpy(acct[1159], "6236640000000011833", 19);
strncpy(acct[1160], "6236640000000011841", 19);
strncpy(acct[1161], "6236640000000011858", 19);
strncpy(acct[1162], "6236640000000011866", 19);
strncpy(acct[1163], "6236640000000011874", 19);
strncpy(acct[1164], "6236640000000011882", 19);
strncpy(acct[1165], "6236640000000011890", 19);
strncpy(acct[1166], "6236640000000011908", 19);
strncpy(acct[1167], "6236640000000011916", 19);
strncpy(acct[1168], "6236640000000011924", 19);
strncpy(acct[1169], "6236640000000011932", 19);
strncpy(acct[1170], "6236640000000011940", 19);
strncpy(acct[1171], "6236640000000011957", 19);
strncpy(acct[1172], "6236640000000011965", 19);
strncpy(acct[1173], "6236640000000011973", 19);
strncpy(acct[1174], "6236640000000011981", 19);
strncpy(acct[1175], "6236640000000011999", 19);
strncpy(acct[1176], "6236640000000012005", 19);
strncpy(acct[1177], "6236640000000012013", 19);
strncpy(acct[1178], "6236640000000012021", 19);
strncpy(acct[1179], "6236640000000012039", 19);
strncpy(acct[1180], "6236640000000012047", 19);
strncpy(acct[1181], "6236640000000012054", 19);
strncpy(acct[1182], "6236640000000012062", 19);
strncpy(acct[1183], "6236640000000012070", 19);
strncpy(acct[1184], "6236640000000012088", 19);
strncpy(acct[1185], "6236640000000012096", 19);
strncpy(acct[1186], "6236640000000012104", 19);
strncpy(acct[1187], "6236640000000012112", 19);
strncpy(acct[1188], "6236640000000012120", 19);
strncpy(acct[1189], "6236640000000012138", 19);
strncpy(acct[1190], "6236640000000012146", 19);
strncpy(acct[1191], "6236640000000012153", 19);
strncpy(acct[1192], "6236640000000012161", 19);
strncpy(acct[1193], "6236640000000012179", 19);
strncpy(acct[1194], "6236640000000012187", 19);
strncpy(acct[1195], "6236640000000012195", 19);
strncpy(acct[1196], "6236640000000012203", 19);
strncpy(acct[1197], "6236640000000012211", 19);
strncpy(acct[1198], "6236640000000012229", 19);
strncpy(acct[1199], "6236640000000012237", 19);
strncpy(acct[1200], "6236640000000012245", 19);
strncpy(acct[1201], "6236640000000012252", 19);
strncpy(acct[1202], "6236640000000012260", 19);
strncpy(acct[1203], "6236640000000012278", 19);
strncpy(acct[1204], "6236640000000012286", 19);
strncpy(acct[1205], "6236640000000012294", 19);
strncpy(acct[1206], "6236640000000012302", 19);
strncpy(acct[1207], "6236640000000012310", 19);
strncpy(acct[1208], "6236640000000012328", 19);
strncpy(acct[1209], "6236640000000012336", 19);
strncpy(acct[1210], "6236640000000012344", 19);
strncpy(acct[1211], "6236640000000012351", 19);
strncpy(acct[1212], "6236640000000012369", 19);
strncpy(acct[1213], "6236640000000012377", 19);
strncpy(acct[1214], "6236640000000012385", 19);
strncpy(acct[1215], "6236640000000012393", 19);
strncpy(acct[1216], "6236640000000012401", 19);
strncpy(acct[1217], "6236640000000012419", 19);
strncpy(acct[1218], "6236640000000012427", 19);
strncpy(acct[1219], "6236640000000012435", 19);
strncpy(acct[1220], "6236640000000012443", 19);
strncpy(acct[1221], "6236640000000012450", 19);
strncpy(acct[1222], "6236640000000012468", 19);
strncpy(acct[1223], "6236640000000012476", 19);
strncpy(acct[1224], "6236640000000012484", 19);
strncpy(acct[1225], "6236640000000012492", 19);
strncpy(acct[1226], "6236640000000012500", 19);
strncpy(acct[1227], "6236640000000012518", 19);
strncpy(acct[1228], "6236640000000012526", 19);
strncpy(acct[1229], "6236640000000012534", 19);
strncpy(acct[1230], "6236640000000012542", 19);
strncpy(acct[1231], "6236640000000012559", 19);
strncpy(acct[1232], "6236640000000012567", 19);
strncpy(acct[1233], "6236640000000012575", 19);
strncpy(acct[1234], "6236640000000012583", 19);
strncpy(acct[1235], "6236640000000012591", 19);
strncpy(acct[1236], "6236640000000012609", 19);
strncpy(acct[1237], "6236640000000012617", 19);
strncpy(acct[1238], "6236640000000012625", 19);
strncpy(acct[1239], "6236640000000012633", 19);
strncpy(acct[1240], "6236640000000012641", 19);
strncpy(acct[1241], "6236640000000012658", 19);
strncpy(acct[1242], "6236640000000012666", 19);
strncpy(acct[1243], "6236640000000012674", 19);
strncpy(acct[1244], "6236640000000012682", 19);
strncpy(acct[1245], "6236640000000012690", 19);
strncpy(acct[1246], "6236640000000012708", 19);
strncpy(acct[1247], "6236640000000012716", 19);
strncpy(acct[1248], "6236640000000012724", 19);
strncpy(acct[1249], "6236640000000012732", 19);
strncpy(acct[1250], "6236640000000012740", 19);
strncpy(acct[1251], "6236640000000012757", 19);
strncpy(acct[1252], "6236640000000012765", 19);
strncpy(acct[1253], "6236640000000012773", 19);
strncpy(acct[1254], "6236640000000012781", 19);
strncpy(acct[1255], "6236640000000012799", 19);
strncpy(acct[1256], "6236640000000012807", 19);
strncpy(acct[1257], "6236640000000012815", 19);
strncpy(acct[1258], "6236640000000012823", 19);
strncpy(acct[1259], "6236640000000012831", 19);
strncpy(acct[1260], "6236640000000012849", 19);
strncpy(acct[1261], "6236640000000012856", 19);
strncpy(acct[1262], "6236640000000012864", 19);
strncpy(acct[1263], "6236640000000012872", 19);
strncpy(acct[1264], "6236640000000012898", 19);
strncpy(acct[1265], "6236640000000012906", 19);
strncpy(acct[1266], "6236640000000012914", 19);
strncpy(acct[1267], "6236640000000012922", 19);
strncpy(acct[1268], "6236640000000012930", 19);
strncpy(acct[1269], "6236640000000012948", 19);
strncpy(acct[1270], "6236640000000012955", 19);
strncpy(acct[1271], "6236640000000012963", 19);
strncpy(acct[1272], "6236640000000012971", 19);
strncpy(acct[1273], "6236640000000012989", 19);
strncpy(acct[1274], "6236640000000012997", 19);
strncpy(acct[1275], "6236640000000013003", 19);
strncpy(acct[1276], "6236640000000013029", 19);
strncpy(acct[1277], "6236640000000013037", 19);
strncpy(acct[1278], "6236640000000013045", 19);
strncpy(acct[1279], "6236640000000013052", 19);
strncpy(acct[1280], "6236640000000013060", 19);
strncpy(acct[1281], "6236640000000013078", 19);
strncpy(acct[1282], "6236640000000013086", 19);
strncpy(acct[1283], "6236640000000013094", 19);
strncpy(acct[1284], "6236640000000013102", 19);
strncpy(acct[1285], "6236640000000013110", 19);
strncpy(acct[1286], "6236640000000013128", 19);
strncpy(acct[1287], "6236640000000013136", 19);
strncpy(acct[1288], "6236640000000013144", 19);
strncpy(acct[1289], "6236640000000013151", 19);
strncpy(acct[1290], "6236640000000013169", 19);
strncpy(acct[1291], "6236640000000013177", 19);
strncpy(acct[1292], "6236640000000013185", 19);
strncpy(acct[1293], "6236640000000013193", 19);
strncpy(acct[1294], "6236640000000013201", 19);
strncpy(acct[1295], "6236640000000013219", 19);
strncpy(acct[1296], "6236640000000013227", 19);
strncpy(acct[1297], "6236640000000013235", 19);
strncpy(acct[1298], "6236640000000013243", 19);
strncpy(acct[1299], "6236640000000013250", 19);
strncpy(acct[1300], "6236640000000013268", 19);
strncpy(acct[1301], "6236640000000013276", 19);
strncpy(acct[1302], "6236640000000013284", 19);
strncpy(acct[1303], "6236640000000013292", 19);
strncpy(acct[1304], "6236640000000013300", 19);
strncpy(acct[1305], "6236640000000013318", 19);
strncpy(acct[1306], "6236640000000013326", 19);
strncpy(acct[1307], "6236640000000013334", 19);
strncpy(acct[1308], "6236640000000013342", 19);
strncpy(acct[1309], "6236640000000013359", 19);
strncpy(acct[1310], "6236640000000013367", 19);
strncpy(acct[1311], "6236640000000013375", 19);
strncpy(acct[1312], "6236640000000013383", 19);
strncpy(acct[1313], "6236640000000013391", 19);
strncpy(acct[1314], "6236640000000013409", 19);
strncpy(acct[1315], "6236640000000013417", 19);
strncpy(acct[1316], "6236640000000013425", 19);
strncpy(acct[1317], "6236640000000013433", 19);
strncpy(acct[1318], "6236640000000013441", 19);
strncpy(acct[1319], "6236640000000013458", 19);
strncpy(acct[1320], "6236640000000013466", 19);
strncpy(acct[1321], "6236640000000013482", 19);
strncpy(acct[1322], "6236640000000013490", 19);
strncpy(acct[1323], "6236640000000013508", 19);
strncpy(acct[1324], "6236640000000013516", 19);
strncpy(acct[1325], "6236640000000013524", 19);
strncpy(acct[1326], "6236640000000013532", 19);
strncpy(acct[1327], "6236640000000013540", 19);
strncpy(acct[1328], "6236640000000013557", 19);
strncpy(acct[1329], "6236640000000013565", 19);
strncpy(acct[1330], "6236640000000013573", 19);
strncpy(acct[1331], "6236640000000013581", 19);
strncpy(acct[1332], "6236640000000013599", 19);
strncpy(acct[1333], "6236640000000013607", 19);
strncpy(acct[1334], "6236640000000013615", 19);
strncpy(acct[1335], "6236640000000013623", 19);
strncpy(acct[1336], "6236640000000013631", 19);
strncpy(acct[1337], "6236640000000013649", 19);
strncpy(acct[1338], "6236640000000013656", 19);
strncpy(acct[1339], "6236640000000013664", 19);
strncpy(acct[1340], "6236640000000013672", 19);
strncpy(acct[1341], "6236640000000013680", 19);
strncpy(acct[1342], "6236640000000013698", 19);
strncpy(acct[1343], "6236640000000013706", 19);
strncpy(acct[1344], "6236640000000013714", 19);
strncpy(acct[1345], "6236640000000013722", 19);
strncpy(acct[1346], "6236640000000013730", 19);
strncpy(acct[1347], "6236640000000013748", 19);
strncpy(acct[1348], "6236640000000013755", 19);
strncpy(acct[1349], "6236640000000013763", 19);
strncpy(acct[1350], "6236640000000013771", 19);
strncpy(acct[1351], "6236640000000013789", 19);
strncpy(acct[1352], "6236640000000013797", 19);
strncpy(acct[1353], "6236640000000013805", 19);
strncpy(acct[1354], "6236640000000013813", 19);
strncpy(acct[1355], "6236640000000013821", 19);
strncpy(acct[1356], "6236640000000013839", 19);
strncpy(acct[1357], "6236640000000013847", 19);
strncpy(acct[1358], "6236640000000013854", 19);
strncpy(acct[1359], "6236640000000013862", 19);
strncpy(acct[1360], "6236640000000013870", 19);
strncpy(acct[1361], "6236640000000013888", 19);
strncpy(acct[1362], "6236640000000013896", 19);
strncpy(acct[1363], "6236640000000013904", 19);
strncpy(acct[1364], "6236640000000013912", 19);
strncpy(acct[1365], "6236640000000013920", 19);
strncpy(acct[1366], "6236640000000013938", 19);
strncpy(acct[1367], "6236640000000013953", 19);
strncpy(acct[1368], "6236640000000013961", 19);
strncpy(acct[1369], "6236640000000013979", 19);
strncpy(acct[1370], "6236640000000013995", 19);
strncpy(acct[1371], "6236640000000014001", 19);
strncpy(acct[1372], "6236640000000014019", 19);
strncpy(acct[1373], "6236640000000014027", 19);
strncpy(acct[1374], "6236640000000014035", 19);
strncpy(acct[1375], "6236640000000014043", 19);
strncpy(acct[1376], "6236640000000014050", 19);
strncpy(acct[1377], "6236640000000014068", 19);
strncpy(acct[1378], "6236640000000014076", 19);
strncpy(acct[1379], "6236640000000014084", 19);
strncpy(acct[1380], "6236640000000014092", 19);
strncpy(acct[1381], "6236640000000014100", 19);
strncpy(acct[1382], "6236640000000014118", 19);
strncpy(acct[1383], "6236640000000014126", 19);
strncpy(acct[1384], "6236640000000014134", 19);
strncpy(acct[1385], "6236640000000014142", 19);
strncpy(acct[1386], "6236640000000014159", 19);
strncpy(acct[1387], "6236640000000014167", 19);
strncpy(acct[1388], "6236640000000014175", 19);
strncpy(acct[1389], "6236640000000014191", 19);
strncpy(acct[1390], "6236640000000014209", 19);
strncpy(acct[1391], "6236640000000014217", 19);
strncpy(acct[1392], "6236640000000014225", 19);
strncpy(acct[1393], "6236640000000014233", 19);
strncpy(acct[1394], "6236640000000014241", 19);
strncpy(acct[1395], "6236640000000014258", 19);
strncpy(acct[1396], "6236640000000014266", 19);
strncpy(acct[1397], "6236640000000014274", 19);
strncpy(acct[1398], "6236640000000014282", 19);
strncpy(acct[1399], "6236640000000014290", 19);
strncpy(acct[1400], "6236640000000014308", 19);
strncpy(acct[1401], "6236640000000014316", 19);
strncpy(acct[1402], "6236640000000014324", 19);
strncpy(acct[1403], "6236640000000014332", 19);
strncpy(acct[1404], "6236640000000014340", 19);
strncpy(acct[1405], "6236640000000014357", 19);
strncpy(acct[1406], "6236640000000014365", 19);
strncpy(acct[1407], "6236640000000014373", 19);
strncpy(acct[1408], "6236640000000014381", 19);
strncpy(acct[1409], "6236640000000014399", 19);
strncpy(acct[1410], "6236640000000014407", 19);
strncpy(acct[1411], "6236640000000014415", 19);
strncpy(acct[1412], "6236640000000014423", 19);
strncpy(acct[1413], "6236640000000014431", 19);
strncpy(acct[1414], "6236640000000014449", 19);
strncpy(acct[1415], "6236640000000014456", 19);
strncpy(acct[1416], "6236640000000014464", 19);
strncpy(acct[1417], "6236640000000014472", 19);
strncpy(acct[1418], "6236640000000014480", 19);
strncpy(acct[1419], "6236640000000014498", 19);
strncpy(acct[1420], "6236640000000014506", 19);
strncpy(acct[1421], "6236640000000014514", 19);
strncpy(acct[1422], "6236640000000014522", 19);
strncpy(acct[1423], "6236640000000014530", 19);
strncpy(acct[1424], "6236640000000014548", 19);
strncpy(acct[1425], "6236640000000014555", 19);
strncpy(acct[1426], "6236640000000014563", 19);
strncpy(acct[1427], "6236640000000014571", 19);
strncpy(acct[1428], "6236640000000014589", 19);
strncpy(acct[1429], "6236640000000014597", 19);
strncpy(acct[1430], "6236640000000014605", 19);
strncpy(acct[1431], "6236640000000014613", 19);
strncpy(acct[1432], "6236640000000014621", 19);
strncpy(acct[1433], "6236640000000014639", 19);
strncpy(acct[1434], "6236640000000014647", 19);
strncpy(acct[1435], "6236640000000014654", 19);
strncpy(acct[1436], "6236640000000014662", 19);
strncpy(acct[1437], "6236640000000014670", 19);
strncpy(acct[1438], "6236640000000014688", 19);
strncpy(acct[1439], "6236640000000014696", 19);
strncpy(acct[1440], "6236640000000014712", 19);
strncpy(acct[1441], "6236640000000014720", 19);
strncpy(acct[1442], "6236640000000014738", 19);
strncpy(acct[1443], "6236640000000014746", 19);
strncpy(acct[1444], "6236640000000014753", 19);
strncpy(acct[1445], "6236640000000014761", 19);
strncpy(acct[1446], "6236640000000014779", 19);
strncpy(acct[1447], "6236640000000014787", 19);
strncpy(acct[1448], "6236640000000014795", 19);
strncpy(acct[1449], "6236640000000014803", 19);
strncpy(acct[1450], "6236640000000014811", 19);
strncpy(acct[1451], "6236640000000014829", 19);
strncpy(acct[1452], "6236640000000014837", 19);
strncpy(acct[1453], "6236640000000014845", 19);
strncpy(acct[1454], "6236640000000014852", 19);
strncpy(acct[1455], "6236640000000014860", 19);
strncpy(acct[1456], "6236640000000014878", 19);
strncpy(acct[1457], "6236640000000014886", 19);
strncpy(acct[1458], "6236640000000014894", 19);
strncpy(acct[1459], "6236640000000014902", 19);
strncpy(acct[1460], "6236640000000014910", 19);
strncpy(acct[1461], "6236640000000014928", 19);
strncpy(acct[1462], "6236640000000014936", 19);
strncpy(acct[1463], "6236640000000014944", 19);
strncpy(acct[1464], "6236640000000014951", 19);
strncpy(acct[1465], "6236640000000014969", 19);
strncpy(acct[1466], "6236640000000014977", 19);
strncpy(acct[1467], "6236640000000014985", 19);
strncpy(acct[1468], "6236640000000014993", 19);
strncpy(acct[1469], "6236640000000015008", 19);
strncpy(acct[1470], "6236640000000015016", 19);
strncpy(acct[1471], "6236640000000015024", 19);
strncpy(acct[1472], "6236640000000015032", 19);
strncpy(acct[1473], "6236640000000015040", 19);
strncpy(acct[1474], "6236640000000015057", 19);
strncpy(acct[1475], "6236640000000015065", 19);
strncpy(acct[1476], "6236640000000015073", 19);
strncpy(acct[1477], "6236640000000015081", 19);
strncpy(acct[1478], "6236640000000015099", 19);
strncpy(acct[1479], "6236640000000015107", 19);
strncpy(acct[1480], "6236640000000015115", 19);
strncpy(acct[1481], "6236640000000015123", 19);
strncpy(acct[1482], "6236640000000015131", 19);
strncpy(acct[1483], "6236640000000015149", 19);
strncpy(acct[1484], "6236640000000015156", 19);
strncpy(acct[1485], "6236640000000015164", 19);
strncpy(acct[1486], "6236640000000015172", 19);
strncpy(acct[1487], "6236640000000015180", 19);
strncpy(acct[1488], "6236640000000015198", 19);
strncpy(acct[1489], "6236640000000015206", 19);
strncpy(acct[1490], "6236640000000015214", 19);
strncpy(acct[1491], "6236640000000015222", 19);
strncpy(acct[1492], "6236640000000015230", 19);
strncpy(acct[1493], "6236640000000015248", 19);
strncpy(acct[1494], "6236640000000015255", 19);
strncpy(acct[1495], "6236640000000015263", 19);
strncpy(acct[1496], "6236640000000015271", 19);
strncpy(acct[1497], "6236640000000015289", 19);
strncpy(acct[1498], "6236640000000015297", 19);
strncpy(acct[1499], "6236640000000015305", 19);
strncpy(acct[1500], "6236640000000015313", 19);
strncpy(acct[1501], "6236640000000015321", 19);
strncpy(acct[1502], "6236640000000015339", 19);
strncpy(acct[1503], "6236640000000015347", 19);
strncpy(acct[1504], "6236640000000015354", 19);
strncpy(acct[1505], "6236640000000015362", 19);
strncpy(acct[1506], "6236640000000015370", 19);
strncpy(acct[1507], "6236640000000015388", 19);
strncpy(acct[1508], "6236640000000015396", 19);
strncpy(acct[1509], "6236640000000015404", 19);
strncpy(acct[1510], "6236640000000015412", 19);
strncpy(acct[1511], "6236640000000015420", 19);
strncpy(acct[1512], "6236640000000015438", 19);
strncpy(acct[1513], "6236640000000015446", 19);
strncpy(acct[1514], "6236640000000015453", 19);
strncpy(acct[1515], "6236640000000015461", 19);
strncpy(acct[1516], "6236640000000015479", 19);
strncpy(acct[1517], "6236640000000015487", 19);
strncpy(acct[1518], "6236640000000015495", 19);
strncpy(acct[1519], "6236640000000015503", 19);
strncpy(acct[1520], "6236640000000015511", 19);
strncpy(acct[1521], "6236640000000015537", 19);
strncpy(acct[1522], "6236640000000015545", 19);
strncpy(acct[1523], "6236640000000015552", 19);
strncpy(acct[1524], "6236640000000015560", 19);
strncpy(acct[1525], "6236640000000015578", 19);
strncpy(acct[1526], "6236640000000015586", 19);
strncpy(acct[1527], "6236640000000015594", 19);
strncpy(acct[1528], "6236640000000015602", 19);
strncpy(acct[1529], "6236640000000015610", 19);
strncpy(acct[1530], "6236640000000015628", 19);
strncpy(acct[1531], "6236640000000015636", 19);
strncpy(acct[1532], "6236640000000015644", 19);
strncpy(acct[1533], "6236640000000015651", 19);
strncpy(acct[1534], "6236640000000015669", 19);
strncpy(acct[1535], "6236640000000015677", 19);
strncpy(acct[1536], "6236640000000015685", 19);
strncpy(acct[1537], "6236640000000015693", 19);
strncpy(acct[1538], "6236640000000015701", 19);
strncpy(acct[1539], "6236640000000015719", 19);
strncpy(acct[1540], "6236640000000015727", 19);
strncpy(acct[1541], "6236640000000015735", 19);
strncpy(acct[1542], "6236640000000015743", 19);
strncpy(acct[1543], "6236640000000015750", 19);
strncpy(acct[1544], "6236640000000015768", 19);
strncpy(acct[1545], "6236640000000015776", 19);
strncpy(acct[1546], "6236640000000015784", 19);
strncpy(acct[1547], "6236640000000015792", 19);
strncpy(acct[1548], "6236640000000015800", 19);
strncpy(acct[1549], "6236640000000015818", 19);
strncpy(acct[1550], "6236640000000015826", 19);
strncpy(acct[1551], "6236640000000015834", 19);
strncpy(acct[1552], "6236640000000015842", 19);
strncpy(acct[1553], "6236640000000015859", 19);
strncpy(acct[1554], "6236640000000015867", 19);
strncpy(acct[1555], "6236640000000015875", 19);
strncpy(acct[1556], "6236640000000015883", 19);
strncpy(acct[1557], "6236640000000015891", 19);
strncpy(acct[1558], "6236640000000015909", 19);
strncpy(acct[1559], "6236640000000015925", 19);
strncpy(acct[1560], "6236640000000015933", 19);
strncpy(acct[1561], "6236640000000015941", 19);
strncpy(acct[1562], "6236640000000015958", 19);
strncpy(acct[1563], "6236640000000015966", 19);
strncpy(acct[1564], "6236640000000015974", 19);
strncpy(acct[1565], "6236640000000015982", 19);
strncpy(acct[1566], "6236640000000015990", 19);
strncpy(acct[1567], "6236640000000016006", 19);
strncpy(acct[1568], "6236640000000016014", 19);
strncpy(acct[1569], "6236640000000016022", 19);
strncpy(acct[1570], "6236640000000016030", 19);
strncpy(acct[1571], "6236640000000016048", 19);
strncpy(acct[1572], "6236640000000016055", 19);
strncpy(acct[1573], "6236640000000016063", 19);
strncpy(acct[1574], "6236640000000016071", 19);
strncpy(acct[1575], "6236640000000016089", 19);
strncpy(acct[1576], "6236640000000016097", 19);
strncpy(acct[1577], "6236640000000016105", 19);
strncpy(acct[1578], "6236640000000016113", 19);
strncpy(acct[1579], "6236640000000016121", 19);
strncpy(acct[1580], "6236640000000016139", 19);
strncpy(acct[1581], "6236640000000016147", 19);
strncpy(acct[1582], "6236640000000016162", 19);
strncpy(acct[1583], "6236640000000016170", 19);
strncpy(acct[1584], "6236640000000016188", 19);
strncpy(acct[1585], "6236640000000016196", 19);
strncpy(acct[1586], "6236640000000016204", 19);
strncpy(acct[1587], "6236640000000016212", 19);
strncpy(acct[1588], "6236640000000016220", 19);
strncpy(acct[1589], "6236640000000016238", 19);
strncpy(acct[1590], "6236640000000016246", 19);
strncpy(acct[1591], "6236640000000016253", 19);
strncpy(acct[1592], "6236640000000016261", 19);
strncpy(acct[1593], "6236640000000016279", 19);
strncpy(acct[1594], "6236640000000016287", 19);
strncpy(acct[1595], "6236640000000016295", 19);
strncpy(acct[1596], "6236640000000016303", 19);
strncpy(acct[1597], "6236640000000016311", 19);
strncpy(acct[1598], "6236640000000016329", 19);
strncpy(acct[1599], "6236640000000016337", 19);
strncpy(acct[1600], "6236640000000016345", 19);
strncpy(acct[1601], "6236640000000016352", 19);
strncpy(acct[1602], "6236640000000016360", 19);
strncpy(acct[1603], "6236640000000016378", 19);
strncpy(acct[1604], "6236640000000016386", 19);
strncpy(acct[1605], "6236640000000016394", 19);
strncpy(acct[1606], "6236640000000016402", 19);
strncpy(acct[1607], "6236640000000016410", 19);
strncpy(acct[1608], "6236640000000016428", 19);
strncpy(acct[1609], "6236640000000016436", 19);
strncpy(acct[1610], "6236640000000016444", 19);
strncpy(acct[1611], "6236640000000016451", 19);
strncpy(acct[1612], "6236640000000016469", 19);
strncpy(acct[1613], "6236640000000016477", 19);
strncpy(acct[1614], "6236640000000016485", 19);
strncpy(acct[1615], "6236640000000016493", 19);
strncpy(acct[1616], "6236640000000016501", 19);
strncpy(acct[1617], "6236640000000016519", 19);
strncpy(acct[1618], "6236640000000016527", 19);
strncpy(acct[1619], "6236640000000016535", 19);
strncpy(acct[1620], "6236640000000016543", 19);
strncpy(acct[1621], "6236640000000016550", 19);
strncpy(acct[1622], "6236640000000016568", 19);
strncpy(acct[1623], "6236640000000016576", 19);
strncpy(acct[1624], "6236640000000016584", 19);
strncpy(acct[1625], "6236640000000016592", 19);
strncpy(acct[1626], "6236640000000016600", 19);
strncpy(acct[1627], "6236640000000016618", 19);
strncpy(acct[1628], "6236640000000016626", 19);
strncpy(acct[1629], "6236640000000016634", 19);
strncpy(acct[1630], "6236640000000016642", 19);
strncpy(acct[1631], "6236640000000016659", 19);
strncpy(acct[1632], "6236640000000016667", 19);
strncpy(acct[1633], "6236640000000016675", 19);
strncpy(acct[1634], "6236640000000016683", 19);
strncpy(acct[1635], "6236640000000016691", 19);
strncpy(acct[1636], "6236640000000016709", 19);
strncpy(acct[1637], "6236640000000016717", 19);
strncpy(acct[1638], "6236640000000016725", 19);
strncpy(acct[1639], "6236640000000016741", 19);
strncpy(acct[1640], "6236640000000016758", 19);
strncpy(acct[1641], "6236640000000016766", 19);
strncpy(acct[1642], "6236640000000016774", 19);
strncpy(acct[1643], "6236640000000016782", 19);
strncpy(acct[1644], "6236640000000016790", 19);
strncpy(acct[1645], "6236640000000016808", 19);
strncpy(acct[1646], "6236640000000016816", 19);
strncpy(acct[1647], "6236640000000016824", 19);
strncpy(acct[1648], "6236640000000016832", 19);
strncpy(acct[1649], "6236640000000016840", 19);
strncpy(acct[1650], "6236640000000016857", 19);
strncpy(acct[1651], "6236640000000016865", 19);
strncpy(acct[1652], "6236640000000016873", 19);
strncpy(acct[1653], "6236640000000016881", 19);
strncpy(acct[1654], "6236640000000016899", 19);
strncpy(acct[1655], "6236640000000016907", 19);
strncpy(acct[1656], "6236640000000016915", 19);
strncpy(acct[1657], "6236640000000016923", 19);
strncpy(acct[1658], "6236640000000016931", 19);
strncpy(acct[1659], "6236640000000016949", 19);
strncpy(acct[1660], "6236640000000016956", 19);
strncpy(acct[1661], "6236640000000016964", 19);
strncpy(acct[1662], "6236640000000016972", 19);
strncpy(acct[1663], "6236640000000016980", 19);
strncpy(acct[1664], "6236640000000016998", 19);
strncpy(acct[1665], "6236640000000017004", 19);
strncpy(acct[1666], "6236640000000017012", 19);
strncpy(acct[1667], "6236640000000017020", 19);
strncpy(acct[1668], "6236640000000017038", 19);
strncpy(acct[1669], "6236640000000017046", 19);
strncpy(acct[1670], "6236640000000017053", 19);
strncpy(acct[1671], "6236640000000017061", 19);
strncpy(acct[1672], "6236640000000017079", 19);
strncpy(acct[1673], "6236640000000017087", 19);
strncpy(acct[1674], "6236640000000017095", 19);
strncpy(acct[1675], "6236640000000017103", 19);
strncpy(acct[1676], "6236640000000017111", 19);
strncpy(acct[1677], "6236640000000017129", 19);
strncpy(acct[1678], "6236640000000017137", 19);
strncpy(acct[1679], "6236640000000017145", 19);
strncpy(acct[1680], "6236640000000017152", 19);
strncpy(acct[1681], "6236640000000017160", 19);
strncpy(acct[1682], "6236640000000017178", 19);
strncpy(acct[1683], "6236640000000017186", 19);
strncpy(acct[1684], "6236640000000017194", 19);
strncpy(acct[1685], "6236640000000017202", 19);
strncpy(acct[1686], "6236640000000017210", 19);
strncpy(acct[1687], "6236640000000017228", 19);
strncpy(acct[1688], "6236640000000017236", 19);
strncpy(acct[1689], "6236640000000017244", 19);
strncpy(acct[1690], "6236640000000017251", 19);
strncpy(acct[1691], "6236640000000017269", 19);
strncpy(acct[1692], "6236640000000017277", 19);
strncpy(acct[1693], "6236640000000017285", 19);
strncpy(acct[1694], "6236640000000017293", 19);
strncpy(acct[1695], "6236640000000017301", 19);
strncpy(acct[1696], "6236640000000017319", 19);
strncpy(acct[1697], "6236640000000017327", 19);
strncpy(acct[1698], "6236640000000017335", 19);
strncpy(acct[1699], "6236640000000017343", 19);
strncpy(acct[1700], "6236640000000017350", 19);
strncpy(acct[1701], "6236640000000017368", 19);
strncpy(acct[1702], "6236640000000017376", 19);
strncpy(acct[1703], "6236640000000017384", 19);
strncpy(acct[1704], "6236640000000017392", 19);
strncpy(acct[1705], "6236640000000017400", 19);
strncpy(acct[1706], "6236640000000017418", 19);
strncpy(acct[1707], "6236640000000017426", 19);
strncpy(acct[1708], "6236640000000017434", 19);
strncpy(acct[1709], "6236640000000017442", 19);
strncpy(acct[1710], "6236640000000017459", 19);
strncpy(acct[1711], "6236640000000017467", 19);
strncpy(acct[1712], "6236640000000017475", 19);
strncpy(acct[1713], "6236640000000017483", 19);
strncpy(acct[1714], "6236640000000017491", 19);
strncpy(acct[1715], "6236640000000017509", 19);
strncpy(acct[1716], "6236640000000017517", 19);
strncpy(acct[1717], "6236640000000017525", 19);
strncpy(acct[1718], "6236640000000017533", 19);
strncpy(acct[1719], "6236640000000017541", 19);
strncpy(acct[1720], "6236640000000017558", 19);
strncpy(acct[1721], "6236640000000017566", 19);
strncpy(acct[1722], "6236640000000017574", 19);
strncpy(acct[1723], "6236640000000017582", 19);
strncpy(acct[1724], "6236640000000017590", 19);
strncpy(acct[1725], "6236640000000017608", 19);
strncpy(acct[1726], "6236640000000017616", 19);
strncpy(acct[1727], "6236640000000017624", 19);
strncpy(acct[1728], "6236640000000017632", 19);
strncpy(acct[1729], "6236640000000017640", 19);
strncpy(acct[1730], "6236640000000017657", 19);
strncpy(acct[1731], "6236640000000017665", 19);
strncpy(acct[1732], "6236640000000017673", 19);
strncpy(acct[1733], "6236640000000017681", 19);
strncpy(acct[1734], "6236640000000017699", 19);
strncpy(acct[1735], "6236640000000017707", 19);
strncpy(acct[1736], "6236640000000017715", 19);
strncpy(acct[1737], "6236640000000017723", 19);
strncpy(acct[1738], "6236640000000017731", 19);
strncpy(acct[1739], "6236640000000017749", 19);
strncpy(acct[1740], "6236640000000017756", 19);
strncpy(acct[1741], "6236640000000017764", 19);
strncpy(acct[1742], "6236640000000017772", 19);
strncpy(acct[1743], "6236640000000017780", 19);
strncpy(acct[1744], "6236640000000017798", 19);
strncpy(acct[1745], "6236640000000017806", 19);
strncpy(acct[1746], "6236640000000017814", 19);
strncpy(acct[1747], "6236640000000017822", 19);
strncpy(acct[1748], "6236640000000017830", 19);
strncpy(acct[1749], "6236640000000017848", 19);
strncpy(acct[1750], "6236640000000017855", 19);
strncpy(acct[1751], "6236640000000017863", 19);
strncpy(acct[1752], "6236640000000017871", 19);
strncpy(acct[1753], "6236640000000017889", 19);
strncpy(acct[1754], "6236640000000017897", 19);
strncpy(acct[1755], "6236640000000017913", 19);
strncpy(acct[1756], "6236640000000017921", 19);
strncpy(acct[1757], "6236640000000017939", 19);
strncpy(acct[1758], "6236640000000017947", 19);
strncpy(acct[1759], "6236640000000017954", 19);
strncpy(acct[1760], "6236640000000017962", 19);
strncpy(acct[1761], "6236640000000017970", 19);
strncpy(acct[1762], "6236640000000017988", 19);
strncpy(acct[1763], "6236640000000017996", 19);
strncpy(acct[1764], "6236640000000018010", 19);
strncpy(acct[1765], "6236640000000018028", 19);
strncpy(acct[1766], "6236640000000018036", 19);
strncpy(acct[1767], "6236640000000018044", 19);
strncpy(acct[1768], "6236640000000018051", 19);
strncpy(acct[1769], "6236640000000018069", 19);
strncpy(acct[1770], "6236640000000018077", 19);
strncpy(acct[1771], "6236640000000018085", 19);
strncpy(acct[1772], "6236640000000018093", 19);
strncpy(acct[1773], "6236640000000018101", 19);
strncpy(acct[1774], "6236640000000018119", 19);
strncpy(acct[1775], "6236640000000018127", 19);
strncpy(acct[1776], "6236640000000018135", 19);
strncpy(acct[1777], "6236640000000018143", 19);
strncpy(acct[1778], "6236640000000018150", 19);
strncpy(acct[1779], "6236640000000018168", 19);
strncpy(acct[1780], "6236640000000018176", 19);
strncpy(acct[1781], "6236640000000018184", 19);
strncpy(acct[1782], "6236640000000018192", 19);
strncpy(acct[1783], "6236640000000018200", 19);
strncpy(acct[1784], "6236640000000018218", 19);
strncpy(acct[1785], "6236640000000018226", 19);
strncpy(acct[1786], "6236640000000018234", 19);
strncpy(acct[1787], "6236640000000018242", 19);
strncpy(acct[1788], "6236640000000018259", 19);
strncpy(acct[1789], "6236640000000018267", 19);
strncpy(acct[1790], "6236640000000018275", 19);
strncpy(acct[1791], "6236640000000018283", 19);
strncpy(acct[1792], "6236640000000018291", 19);
strncpy(acct[1793], "6236640000000018309", 19);
strncpy(acct[1794], "6236640000000018317", 19);
strncpy(acct[1795], "6236640000000018325", 19);
strncpy(acct[1796], "6236640000000018333", 19);
strncpy(acct[1797], "6236640000000018358", 19);
strncpy(acct[1798], "6236640000000018366", 19);
strncpy(acct[1799], "6236640000000018374", 19);
strncpy(acct[1800], "6236640000000018382", 19);
strncpy(acct[1801], "6236640000000018390", 19);
strncpy(acct[1802], "6236640000000018408", 19);
strncpy(acct[1803], "6236640000000018416", 19);
strncpy(acct[1804], "6236640000000018424", 19);
strncpy(acct[1805], "6236640000000018432", 19);
strncpy(acct[1806], "6236640000000018440", 19);
strncpy(acct[1807], "6236640000000018457", 19);
strncpy(acct[1808], "6236640000000018465", 19);
strncpy(acct[1809], "6236640000000018473", 19);
strncpy(acct[1810], "6236640000000018481", 19);
strncpy(acct[1811], "6236640000000018499", 19);
strncpy(acct[1812], "6236640000000018507", 19);
strncpy(acct[1813], "6236640000000018515", 19);
strncpy(acct[1814], "6236640000000018523", 19);
strncpy(acct[1815], "6236640000000018531", 19);
strncpy(acct[1816], "6236640000000018549", 19);
strncpy(acct[1817], "6236640000000018556", 19);
strncpy(acct[1818], "6236640000000018564", 19);
strncpy(acct[1819], "6236640000000018572", 19);
strncpy(acct[1820], "6236640000000018580", 19);
strncpy(acct[1821], "6236640000000018598", 19);
strncpy(acct[1822], "6236640000000018606", 19);
strncpy(acct[1823], "6236640000000018614", 19);
strncpy(acct[1824], "6236640000000018622", 19);
strncpy(acct[1825], "6236640000000018630", 19);
strncpy(acct[1826], "6236640000000018648", 19);
strncpy(acct[1827], "6236640000000018655", 19);
strncpy(acct[1828], "6236640000000018663", 19);
strncpy(acct[1829], "6236640000000018671", 19);
strncpy(acct[1830], "6236640000000018689", 19);
strncpy(acct[1831], "6236640000000018697", 19);
strncpy(acct[1832], "6236640000000018705", 19);
strncpy(acct[1833], "6236640000000018713", 19);
strncpy(acct[1834], "6236640000000018721", 19);
strncpy(acct[1835], "6236640000000018739", 19);
strncpy(acct[1836], "6236640000000018747", 19);
strncpy(acct[1837], "6236640000000018754", 19);
strncpy(acct[1838], "6236640000000018762", 19);
strncpy(acct[1839], "6236640000000018770", 19);
strncpy(acct[1840], "6236640000000018788", 19);
strncpy(acct[1841], "6236640000000018796", 19);
strncpy(acct[1842], "6236640000000018804", 19);
strncpy(acct[1843], "6236640000000018812", 19);
strncpy(acct[1844], "6236640000000018820", 19);
strncpy(acct[1845], "6236640000000018838", 19);
strncpy(acct[1846], "6236640000000018846", 19);
strncpy(acct[1847], "6236640000000018853", 19);
strncpy(acct[1848], "6236640000000018861", 19);
strncpy(acct[1849], "6236640000000018879", 19);
strncpy(acct[1850], "6236640000000018887", 19);
strncpy(acct[1851], "6236640000000018895", 19);
strncpy(acct[1852], "6236640000000018903", 19);
strncpy(acct[1853], "6236640000000018911", 19);
strncpy(acct[1854], "6236640000000018929", 19);
strncpy(acct[1855], "6236640000000018937", 19);
strncpy(acct[1856], "6236640000000018945", 19);
strncpy(acct[1857], "6236640000000018952", 19);
strncpy(acct[1858], "6236640000000018960", 19);
strncpy(acct[1859], "6236640000000018978", 19);
strncpy(acct[1860], "6236640000000018986", 19);
strncpy(acct[1861], "6236640000000018994", 19);
strncpy(acct[1862], "6236640000000019000", 19);
strncpy(acct[1863], "6236640000000019018", 19);
strncpy(acct[1864], "6236640000000019026", 19);
strncpy(acct[1865], "6236640000000019034", 19);
strncpy(acct[1866], "6236640000000019042", 19);
strncpy(acct[1867], "6236640000000019059", 19);
strncpy(acct[1868], "6236640000000019067", 19);
strncpy(acct[1869], "6236640000000019075", 19);
strncpy(acct[1870], "6236640000000019083", 19);
strncpy(acct[1871], "6236640000000019091", 19);
strncpy(acct[1872], "6236640000000019109", 19);
strncpy(acct[1873], "6236640000000019117", 19);
strncpy(acct[1874], "6236640000000019125", 19);
strncpy(acct[1875], "6236640000000019133", 19);
strncpy(acct[1876], "6236640000000019141", 19);
strncpy(acct[1877], "6236640000000019158", 19);
strncpy(acct[1878], "6236640000000019166", 19);
strncpy(acct[1879], "6236640000000019174", 19);
strncpy(acct[1880], "6236640000000019182", 19);
strncpy(acct[1881], "6236640000000019190", 19);
strncpy(acct[1882], "6236640000000019208", 19);
strncpy(acct[1883], "6236640000000019216", 19);
strncpy(acct[1884], "6236640000000019224", 19);
strncpy(acct[1885], "6236640000000019232", 19);
strncpy(acct[1886], "6236640000000019240", 19);
strncpy(acct[1887], "6236640000000019257", 19);
strncpy(acct[1888], "6236640000000019265", 19);
strncpy(acct[1889], "6236640000000019273", 19);
strncpy(acct[1890], "6236640000000019281", 19);
strncpy(acct[1891], "6236640000000019299", 19);
strncpy(acct[1892], "6236640000000019307", 19);
strncpy(acct[1893], "6236640000000019315", 19);
strncpy(acct[1894], "6236640000000019323", 19);
strncpy(acct[1895], "6236640000000019331", 19);
strncpy(acct[1896], "6236640000000019349", 19);
strncpy(acct[1897], "6236640000000019356", 19);
strncpy(acct[1898], "6236640000000019364", 19);
strncpy(acct[1899], "6236640000000019372", 19);
strncpy(acct[1900], "6236640000000019380", 19);
strncpy(acct[1901], "6236640000000019398", 19);
strncpy(acct[1902], "6236640000000019406", 19);
strncpy(acct[1903], "6236640000000019414", 19);
strncpy(acct[1904], "6236640000000019422", 19);
strncpy(acct[1905], "6236640000000019430", 19);
strncpy(acct[1906], "6236640000000019448", 19);
strncpy(acct[1907], "6236640000000019455", 19);
strncpy(acct[1908], "6236640000000019463", 19);
strncpy(acct[1909], "6236640000000019471", 19);
strncpy(acct[1910], "6236640000000019489", 19);
strncpy(acct[1911], "6236640000000019497", 19);
strncpy(acct[1912], "6236640000000019505", 19);
strncpy(acct[1913], "6236640000000019513", 19);
strncpy(acct[1914], "6236640000000019521", 19);
strncpy(acct[1915], "6236640000000019539", 19);
strncpy(acct[1916], "6236640000000019547", 19);
strncpy(acct[1917], "6236640000000019554", 19);
strncpy(acct[1918], "6236640000000019562", 19);
strncpy(acct[1919], "6236640000000019570", 19);
strncpy(acct[1920], "6236640000000019588", 19);
strncpy(acct[1921], "6236640000000019596", 19);
strncpy(acct[1922], "6236640000000019604", 19);
strncpy(acct[1923], "6236640000000019612", 19);
strncpy(acct[1924], "6236640000000019620", 19);
strncpy(acct[1925], "6236640000000019638", 19);
strncpy(acct[1926], "6236640000000019646", 19);
strncpy(acct[1927], "6236640000000019653", 19);
strncpy(acct[1928], "6236640000000019661", 19);
strncpy(acct[1929], "6236640000000019679", 19);
strncpy(acct[1930], "6236640000000019687", 19);
strncpy(acct[1931], "6236640000000019695", 19);
strncpy(acct[1932], "6236640000000019703", 19);
strncpy(acct[1933], "6236640000000019711", 19);
strncpy(acct[1934], "6236640000000019729", 19);
strncpy(acct[1935], "6236640000000019737", 19);
strncpy(acct[1936], "6236640000000019745", 19);
strncpy(acct[1937], "6236640000000019760", 19);
strncpy(acct[1938], "6236640000000019778", 19);
strncpy(acct[1939], "6236640000000019786", 19);
strncpy(acct[1940], "6236640000000019794", 19);
strncpy(acct[1941], "6236640000000019802", 19);
strncpy(acct[1942], "6236640000000019810", 19);
strncpy(acct[1943], "6236640000000019828", 19);
strncpy(acct[1944], "6236640000000019836", 19);
strncpy(acct[1945], "6236640000000019844", 19);
strncpy(acct[1946], "6236640000000019851", 19);
strncpy(acct[1947], "6236640000000019869", 19);
strncpy(acct[1948], "6236640000000019877", 19);
strncpy(acct[1949], "6236640000000019885", 19);
strncpy(acct[1950], "6236640000000019893", 19);
strncpy(acct[1951], "6236640000000019901", 19);
strncpy(acct[1952], "6236640000000019919", 19);
strncpy(acct[1953], "6236640000000019927", 19);
strncpy(acct[1954], "6236640000000019935", 19);
strncpy(acct[1955], "6236640000000019943", 19);
strncpy(acct[1956], "6236640000000019950", 19);
strncpy(acct[1957], "6236640000000019968", 19);
strncpy(acct[1958], "6236640000000019976", 19);
strncpy(acct[1959], "6236640000000019984", 19);
strncpy(acct[1960], "6236640000000019992", 19);
strncpy(acct[1961], "6236640000000020008", 19);
strncpy(acct[1962], "6236640000000020016", 19);
strncpy(acct[1963], "6236640000000020024", 19);
strncpy(acct[1964], "6236640000000020032", 19);
strncpy(acct[1965], "6236640000000020040", 19);
strncpy(acct[1966], "6236640000000020057", 19);
strncpy(acct[1967], "6236640000000020065", 19);
strncpy(acct[1968], "6236640000000020073", 19);
strncpy(acct[1969], "6236640000000020081", 19);
strncpy(acct[1970], "6236640000000020099", 19);
strncpy(acct[1971], "6236640000000020107", 19);
strncpy(acct[1972], "6236640000000020115", 19);
strncpy(acct[1973], "6236640000000020123", 19);
strncpy(acct[1974], "6236640000000020131", 19);
strncpy(acct[1975], "6236640000000020149", 19);
strncpy(acct[1976], "6236640000000020156", 19);
strncpy(acct[1977], "6236640000000020164", 19);
strncpy(acct[1978], "6236640000000020180", 19);
strncpy(acct[1979], "6236640000000020198", 19);
strncpy(acct[1980], "6236640000000020206", 19);
strncpy(acct[1981], "6236640000000020214", 19);
strncpy(acct[1982], "6236640000000020222", 19);
strncpy(acct[1983], "6236640000000020230", 19);
strncpy(acct[1984], "6236640000000020248", 19);
strncpy(acct[1985], "6236640000000020255", 19);
strncpy(acct[1986], "6236640000000020263", 19);
strncpy(acct[1987], "6236640000000020271", 19);
strncpy(acct[1988], "6236640000000020289", 19);
strncpy(acct[1989], "6236640000000020297", 19);
strncpy(acct[1990], "6236640000000020305", 19);
strncpy(acct[1991], "6236640000000020313", 19);
strncpy(acct[1992], "6236640000000020321", 19);
strncpy(acct[1993], "6236640000000020339", 19);
strncpy(acct[1994], "6236640000000020347", 19);
strncpy(acct[1995], "6236640000000020354", 19);
strncpy(acct[1996], "6236640000000020362", 19);
strncpy(acct[1997], "6236640000000020370", 19);
strncpy(acct[1998], "6236640000000020388", 19);
strncpy(acct[1999], "6236640000000020396", 19);
strncpy(acct[2000], "6236640000000020396", 19);

}