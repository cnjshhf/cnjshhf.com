/***************************************************************
 * 公共函数库
 **************************************************************/
#include "tpbus.h"

static pthread_mutex_t m_mtx;
static char b64encmap[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

/***************************************************************
 * 检查授权许可
 **************************************************************/
int tpLicence()
{
  return 0;
}

/***************************************************************
 * 设置调试文件
 **************************************************************/
void tpSetLogFile(const char *file)
{
  char buf[256];

  if(file[0] == '/')
  {
    setLogFile(file);
  }
  else
  {
    snprintf(buf, sizeof(buf), "%s/log/%s", getenv("TPBUS_HOME"), file);
    setLogFile(buf);
  }
}

/**************************************************************
 * 获取进程号
 *************************************************************/
int tpGetPid(char *proc)
{
  FILE *pp;
  int  pid;
  char buf[256];
  char cmd[256];

  snprintf(buf, sizeof(buf), "ps -u `whoami` -o comm,pid | grep %s", proc);
  if((pp = popen(buf, "r")) != NULL)
  {
    while(fgets(buf, sizeof(buf), pp) != NULL)
    {
      sscanf(buf, "%s %d", cmd, &pid);
      if(strcmp(cmd, proc) == 0)
      {
        pclose(pp);
        return pid;
      }
    }
  }
  pclose(pp);
  return -1;
}

/**************************************************************
 * 初始化任务
 *************************************************************/
int tpTaskInit(int taskId, char *program, TPTASK *taskCopy)
{
  int q, ret;
  char *p, buf[256];
  char fmtBuf[21], tmpBuf[256];
  time_t t;
  TPTASK *ptpTask = NULL;

  if(tpShmTouch() != 0)
  {
    printf("tpShmTouch() error\n");
    return -1;
  }

  /*读取任务配置*/
  if(taskId > 0)
  {
    if(!(ptpTask = tpTaskShmGet(taskId, NULL)))
    {
      printf("读取任务[%d]配置出错", taskId);
      return -1;
    }
    if(taskCopy) memcpy(taskCopy, ptpTask, sizeof(TPTASK));
  }

  /*初始化日志设置*/
  if(program && program[0])
  {
    for(p = program + strlen(program) - 1; p >= program; p--)
    {
      if(*p == '/') break;
    }
    p += 1;

    tmpBuf[0] = 0;
    if(getenv("LOG_DATE_FMT"))
    {
      time(&t);
      snprintf(tmpBuf, sizeof(tmpBuf), "_%s", getTimeString(t, getenv("LOG_DATE_FMT"), fmtBuf));
    }

    if(!ptpTask || ptpTask->bindQ <= 0)
    {
      /*sprintf(buf, "%s%s.log", p, tmpBuf); */
      sprintf(buf, "%s.log", p);
    }
    else
    {
      /* sprintf(buf, "%s_%d%s.log", p, ptpTask->bindQ, tmpBuf);*/
             sprintf(buf, "%s_%d_%d.log", p, ptpTask->bindQ, taskId );    /*update by xgh*/
        }
    tpSetLogFile(buf);
  }
  if(ptpTask) tpSetLogLevel(ptpTask->logLevel);

  logPrefixEnable();
  setLogCallBack(tpSendEvent);

  /*初始化LIBXML2库(为线程安全)*/
  for(q = 1; q <= NMAXBINDQ; q++)
  {
    if(q != gShm.ptpPort[q-1].bindQ) continue;
    if(gShm.ptpPort[q-1].fmtDefs.fmtType == MSG_XML) xmlInitParser();
  }

  /*初始化线程数据*/
  if((ret = tpThreadInitTSD(&TSD_CTX_KEY)) != 0)
  {
    LOG_ERR("Failed to init TSD: %s", strerror(ret));
    return -1;
  }

  /*设置信号*/
  signal(SIGCLD,  SIG_IGN);
  signal(SIGINT,  SIG_IGN);
  signal(SIGHUP,  SIG_IGN);
  signal(SIGQUIT, SIG_IGN);
  signal(SIGTSTP, SIG_IGN);
  signal(SIGPIPE, SIG_IGN);

  return 0;
}

/***************************************************************
 * 设置日志输出前缀
 **************************************************************/
void tpSetLogPrefix(int tpId, char *prefix)
{
  sprintf(prefix, "%010d|", tpId);
  setLogPrefix(prefix);
}

/***************************************************************
 * 提取字符串型配置参数
 **************************************************************/
int tpGetParamString(const char *params, const char *name, char *value)
{
  int tmpLen, offset = 0;
  const char *p;

  while(1)
  {
    p = params + offset;
    while(params[offset++]) {}
    if(!(*p)) break;
    tmpLen = ((unsigned char)params[offset]) * 255 + ((unsigned char)params[offset+1]);
    offset += 2;
    if(strcmp(p, name) == 0)
    {
      memcpy(value, params + offset, tmpLen);
      value[tmpLen] = 0;
      return 0;
    }
    offset += tmpLen;
  }
  return -1;
}

/***************************************************************
 * 提取整型配置参数
 **************************************************************/
int tpGetParamInt(const char *params, const char *name, int *value)
{
  char buf[256];

  if(tpGetParamString(params, name, buf) != 0) return -1;
  *value = atoi(buf);
  return 0;
}

/***************************************************************
 * 打印报文头调试信息
 **************************************************************/
void tpHeadDebug(TPHEAD *ptpHead)
{
  FILE *fp;
  char *prefix;

  if(!(fp = logOutStart())) return;
  if(!(prefix = getLogPrefix())) prefix = "\0";

  fprintf(fp, "%s*** 报文头开始 ***\n", prefix);
  fprintf(fp, "%s%-20s = [%d]\n",  prefix, "tpId", ptpHead->tpId);
  fprintf(fp, "%s%-20s = [%d]\n",  prefix, "startQ", ptpHead->startQ);    /*add by xgh 20161114*/
  fprintf(fp, "%s%-20s = [%d]\n",  prefix, "orgQ", ptpHead->orgQ);
  fprintf(fp, "%s%-20s = [%d]\n",  prefix, "desQ", ptpHead->desQ);
  fprintf(fp, "%s%-20s = [%s]\n",  prefix, "tranCode", ptpHead->tranCode);
  fprintf(fp, "%s%-20s = [%s]\n",  prefix, "servCode", ptpHead->servCode);
  fprintf(fp, "%s%-20s = [%s]\n",  prefix, "procCode", ptpHead->procCode);
  fprintf(fp, "%s%-20s = [%ld]\n", prefix, "beginTime", ptpHead->beginTime);
  fprintf(fp, "%s%-20s = [%d]\n",  prefix, "msgType", ptpHead->msgType);
  fprintf(fp, "%s%-20s = [%d]\n",  prefix, "fmtType", ptpHead->fmtType);
  fprintf(fp, "%s%-20s = [%d]\n",  prefix, "bodyLen", ptpHead->bodyLen);
  fprintf(fp, "%s%-20s = [%d]\n",  prefix, "bodyOffset", ptpHead->bodyOffset);
  fprintf(fp, "%s%-20s = [%d]\n",  prefix, "shmIndex", ptpHead->shmIndex);
  fprintf(fp, "%s%-20s = [%d]\n",  prefix, "safId", ptpHead->safId);
  fprintf(fp, "%s%-20s = [%d]\n",  prefix, "retFlag", ptpHead->retFlag);
  fprintf(fp, "%s%-20s = [%d]\n",  prefix, "status", ptpHead->status);
  fprintf(fp, "%s*** 报文头结束 ***\n", prefix);

  logOutEnd(fp);
}

/***************************************************************
 * 打印LIST调试信息
 **************************************************************/
void tpListDebug(MSGCTX *pMsgCtx)
{
  FILE *fp;
  int  i, len;
  int  nMax = 32;
  char *prefix, buf[MAXMSGLEN];

  if(!(fp = logOutStart())) return;
  if(!(prefix = getLogPrefix())) prefix = "\0";

  fprintf(fp, "%s*** LIST开始 ***\n", prefix);
  for(i = 0; i < MAXFLDNUM; i++)
  {
    if((len = tpMsgCtxGet(pMsgCtx, i, buf, sizeof(buf))) <= 0) continue;
    buf[(len>nMax)? nMax:len] = 0;
    fprintf(fp, "%s#%04d = (%04d) [%s]\n", prefix, i + 1, len, buf);
  }
  fprintf(fp, "%s*** LIST结束 ***\n", prefix);

  logOutEnd(fp);
}

/***************************************************************
 * 打印HASH调试信息
 **************************************************************/
void tpHashDebug(MSGCTX *pMsgCtx)
{
  FILE *fp;
  int  i, j, len;
  int  nMax = 32;
  char *prefix, buf[MAXMSGLEN];

  if(!(fp = logOutStart())) return;
  if(!(prefix = getLogPrefix())) prefix = "\0";

  fprintf(fp, "%s*** HASH开始 ***\n", prefix);
  for(i = 0; i < HASH_SIZE; i++)
  {
    if(!pMsgCtx->flds[i].name[0]) continue;
    if((len = tpMsgCtxGet(pMsgCtx, i, buf, sizeof(buf))) < 0) break;
    if(pMsgCtx->flds[i].name[0] == '_')
    {
      fprintf(fp, "%s%-20s = (%04d) [%s]\n", prefix, pMsgCtx->flds[i].name, len, "******");
    }
    else
    {
      buf[(len>nMax)? nMax:len] = 0;
      fprintf(fp, "%s%-20s = (%04d) [%s]\n", prefix, pMsgCtx->flds[i].name, len, buf);
    }
    j = pMsgCtx->flds[i].next;
    while(j > 0)
    {
      if((len = tpMsgCtxGet(pMsgCtx, j - 1, buf, sizeof(buf))) < 0) break;
      if(pMsgCtx->flds[j-1].name[0] == '_')
      {
        fprintf(fp, "%s%-20s = (%04d) [%s]\n", prefix, pMsgCtx->flds[j-1].name, len, "******");
      }
      else
      {
        buf[(len>nMax)? nMax:len] = 0;
        fprintf(fp, "%s%-20s = (%04d) [%s]\n", prefix, pMsgCtx->flds[j-1].name, len, buf);
      }
      j = pMsgCtx->flds[j-1].next;
    }
    if(len < 0) break;
  }
  fprintf(fp, "%s*** HASH结束 ***\n", prefix);

  logOutEnd(fp);
}

/***************************************************************
 * tpSendEvent
 **************************************************************/
void tpSendEvent(const char *buf)
{
  int sockfd;
  TPMSG tpMsg;

  memset(&tpMsg.head, 0, sizeof(TPHEAD));
  tpMsg.head.msgType = MSGMON_EVT;
  tpMsg.head.bodyLen = strlen(buf);
  memcpy(tpMsg.body, buf, tpMsg.head.bodyLen);

  if((sockfd = udp_init(NULL, 0)) == -1) return;
  udp_send(sockfd, (const char *)&tpMsg, tpMsg.head.bodyLen + sizeof(TPHEAD), NULL, gShm.ptpConf->sysMonPort);
  close(sockfd);
}

/***************************************************************
 * tpSendToSysMon
 **************************************************************/
void tpSendToSysMon(TPHEAD *ptpHead)
{
  int sockfd, flag = 0;
  TPTRAN tpTran, *ptpTran;
  TPMSG *ptpMsg = (TPMSG *)ptpHead;
  struct timeb tb;

  ptpHead->bodyLen = 0;
  switch(ptpHead->status)
  {
  case TP_OK:
    flag = 1;
    ftime(&tb);
    ptpHead->milliTime = tb.millitm - ptpHead->milliTime;
    ptpHead->milliTime += 1000 * (tb.time - ptpHead->beginTime);
    ptpHead->desQ = 0;
    if(ptpHead->monFlag)
    {
      ptpTran = &(gShm.pShmTran[ptpHead->shmIndex-1].tpTran);
      ptpHead->bodyLen = tpMsgCtxPack(&ptpTran->msgCtx, ptpMsg->body, MAXMSGLEN - sizeof(TPHEAD));
    }
    break;
  case TP_REVOK:
  case TP_REVFAIL:
  case TP_TIMEOUT:
    flag = 1;
    ptpHead->milliTime = 0;
    if(ptpHead->status == TP_REVOK) ptpHead->desQ = 0;
    if(ptpHead->monFlag)
    {
      if(tpTranCtxLoad(ptpHead->tpId, &tpTran) != 0) break;
      ptpHead->bodyLen = tpMsgCtxPack(&tpTran.msgCtx, ptpMsg->body, MAXMSGLEN - sizeof(TPHEAD));
    }
    break;
  case TP_SAFFAIL:
    if(!ptpHead->tpId)
    {
      if(tpSafMsgLoad(ptpHead->safId, ptpMsg) != 0) break;
    }
    flag = 1;
    if(ptpHead->monFlag)
    {
      if(tpTranCtxLoad(ptpHead->tpId, &tpTran) != 0) break;
      ptpHead->bodyLen = tpMsgCtxPack(&tpTran.msgCtx, ptpMsg->body, MAXMSGLEN - sizeof(TPHEAD));
    }
    break;
  case TP_BUSY:
  case TP_INVAL:
  case TP_ERROR:
  case TP_RUNING:
  case TP_REVING:
    break;
  default:
    break;
  }
  if(!flag) return;
  if((sockfd = udp_init(NULL, 0)) == -1) return;
  udp_send(sockfd, (const char *)ptpMsg, ptpHead->bodyLen + sizeof(TPHEAD), NULL, gShm.ptpConf->sysMonPort);
  close(sockfd);
}

/***************************************************************
 * tpHostSelect
 **************************************************************/
static int tpHostSelect()
{
  if(!gShm.ptpConf->dispatch[0]) return 0;

  /*根据负载均衡算法选择目标主机(TODO)*/
  return 0;
}

/***************************************************************
 * 初始化报文头
 **************************************************************/
int tpNewHead(int q, TPHEAD *ptpHead)
{
  int i;
  TPPORT *ptpPort;
  struct timeb tb;

  memset(ptpHead, 0, sizeof(TPHEAD));

  if(!(ptpPort = tpPortShmGet(q, NULL)))
  {
    LOG_ERR("读取端点[%d]配置出错", q);
    return -1;
  }

  if(!ptpPort->portType)    /*请求端点*/
  {
    if((ptpHead->tpId = tpNewId()) <= 0)
    {
      LOG_ERR("分配流水号出错");
      return -1;
    }
    if((i = tpHostSelect()) < 0)
    {
      LOG_ERR("没有找到可用的服务器");
      return -1;
    }
    ptpHead->tpId += (gShm.pShmHead->dispatch * 10 + i) * 100000000;

    ptpHead->orgQ = q;
    ptpHead->fmtType = ptpPort->fmtDefs.fmtType;
    ptpHead->msgType = MSGAPP;
    ftime(&tb);
    ptpHead->beginTime = tb.time;
    ptpHead->milliTime = tb.millitm;
  }
  else     /*服务端点(仅针对异步通讯时)*/
  {
    ptpHead->desQ = q;
    ptpHead->fmtType = ptpPort->fmtDefs.fmtType;
  }
  return 0;
}

/***************************************************************
 * 发送服务请求(用于请求端通讯程序)
 **************************************************************/
int tpSend(MQHDL *mqh, TPMSG *ptpMsg, int flag)
{
  int   i, ret, msgLen;
  char *rqm, clazz[MSGCLZLEN+1];

  i = (ptpMsg->head.tpId / 100000000) % 10;
  rqm = (i == 0) ? NULL : gShm.ptpConf->hosts[i-1].qm;
  clazz[0] = 0;
  if(flag) snprintf(clazz, sizeof(clazz), "%d", ptpMsg->head.tpId);
  msgLen = ptpMsg->head.bodyLen + sizeof(TPHEAD);
  if((ret = mq_post(mqh, rqm, Q_SVR, 0, clazz, (char *)ptpMsg, msgLen, 0)) != 0)
  {
    LOG_ERR("mq_post() error: retcode=%d", ret);
    LOG_ERR("写主控队列[%d]出错", Q_SVR);
    return -1;
  }
  LOG_MSG("写主控队列[%d]...[ok]", Q_SVR);
  return 0;
}

/***************************************************************
 * 接收服务响应(用于请求端通讯程序)
 **************************************************************/
int tpRecv(MQHDL *mqh, TPMSG *ptpMsg, int timeout, int flag)
{
  int  q, ret, msgLen;
  char clazz[MSGCLZLEN+1];

  clazz[0] = 0;
  if(flag) snprintf(clazz, sizeof(clazz), "%d", ptpMsg->head.tpId);
  msgLen = sizeof(TPMSG);
  if((ret = mq_pend(mqh, NULL, &q, NULL, clazz, (char *)ptpMsg, &msgLen, timeout)) != 0)
  {
    LOG_ERR("mq_pend() error: retcode=%d", ret);
    LOG_ERR("读绑定队列[%d]出错", mqh->qid);
    return -1;
  }
  if(q == Q_SVR)
  {
    LOG_MSG("收到来自主控程序的报文");
  }
  else if(q == Q_REV)
  {
    LOG_MSG("收到来自事务程序的报文");
  }
  else
  {
    LOG_ERR("收到来自队列[%d]的非法报文", q);
    return -1;
  }
  return 0;
}

/***************************************************************
 * 发送服务请求并等待响应返回(用于请求端通讯程序)
 **************************************************************/
int tpCall(MQHDL *mqh, TPMSG *ptpMsg, int timeout)
{
  if(tpSend(mqh, ptpMsg, 1) != 0) return -1;
  return tpRecv(mqh, ptpMsg, timeout, 1);
}

/***************************************************************
 * 接收服务请求(用于服务端通讯程序)
 **************************************************************/
int tpAccept(MQHDL *mqh, TPMSG *ptpMsg)
{
  int q, ret, msgLen;

  msgLen = sizeof(TPMSG);
  if((ret = mq_pend(mqh, NULL, &q, NULL, NULL, (char *)ptpMsg, &msgLen, 0)) != 0)
  {
    LOG_ERR("mq_pend() error: retcode=%d", ret);
    LOG_ERR("读绑定队列[%d]出错", mqh->qid);
    return -1;
  }
  if(q == Q_SVR)
  {
    LOG_MSG("收到来自主控程序的报文");
  }
  else if(q == Q_REV)
  {
    LOG_MSG("收到来自事务程序的报文");
  }
  else
  {
    LOG_ERR("收到来自队列[%d]的非法报文", q);
    return -1;
  }
  return 0;
}

/***************************************************************
 * 返回服务响应(用于服务端通讯程序)
 **************************************************************/
int tpReturn(MQHDL *mqh, TPMSG *ptpMsg)
{
  char *rqm;
  int i, j, ret, msgLen;

  msgLen = ptpMsg->head.bodyLen + sizeof(TPHEAD);

  if(ptpMsg->head.tpId)
  {
    i = (ptpMsg->head.tpId / 100000000) % 10;
    rqm = (i == 0) ? NULL : gShm.ptpConf->hosts[i-1].qm;
    if((ret = mq_post(mqh, rqm, Q_SVR, 0, NULL, (char *)ptpMsg, msgLen, 0)) != 0)
    {
      LOG_ERR("mq_post() error: retcode=%d", ret);
      LOG_ERR("写主控队列[%d]出错", Q_SVR);
      return -1;
    }
  }
  else
  {
    if(!gShm.ptpConf->dispatch[0])    /*非集群模式*/
    {
      if((ret = mq_post(mqh, NULL, Q_SVR, 0, NULL, (char *)ptpMsg, msgLen, 0)) != 0)
      {
        LOG_ERR("mq_post() error: retcode=%d", ret);
        LOG_ERR("写主控队列[%d]出错", Q_SVR);
        return -1;
      }
    }
    else     /*集群模式*/
    {
      for(i = 0, j = 0; i < NMAXHOSTS; i++)
      {
        if(!gShm.ptpConf->hosts[i].id) continue;
        if(!gShm.ptpConf->hosts[i].status) continue;
        rqm = gShm.ptpConf->hosts[i].qm;
        if((ret = mq_post(mqh, rqm, Q_SVR, 0, NULL, (char *)ptpMsg, msgLen, 0)) != 0)
        {
          LOG_ERR("mq_post() error: retcode=%d", ret);
          LOG_ERR("写主控队列[%d]出错", Q_SVR);
          return -1;
        }
        j++;
      }
      if(j == 0)
      {
        LOG_ERR("没有找到可用的服务器");
        return -1;
      }
    }
  }
  LOG_MSG("写主控队列[%d]...[ok]", Q_SVR);
  return 0;
}

/***************************************************************
 * DUMP异步报文头
 **************************************************************/
int tpHeadDump(int q, char *key, TPHEAD *ptpHead)
{
  FILE *fp;
  char pathFile[256];

  snprintf(pathFile, sizeof(pathFile), "%s/rtm/tph/%d_%s.%d", getenv("TPBUS_HOME"), q, key, ptpHead->tpId);
  if(!(fp = fopen(pathFile, "w")))
  {
    LOG_ERR("打开文件[%s]出错: %s", pathFile, strerror(errno));
    return -1;
  }

  if(sizeof(TPHEAD) != fwrite(ptpHead, 1, sizeof(TPHEAD), fp))
  {
    LOG_ERR("写文件[%s]数据不完整", pathFile);
    fclose(fp);
    return -1;
  }
  fclose(fp);
  return 0;
}

/***************************************************************
 * 保存异步报文头
 **************************************************************/
int tpHeadSave(TPPORT *ptpPort, TPHEAD *ptpHead)
{
  char tmpBuf[MAXEXPLEN+1];

  if(tpFunExec(ptpPort->headSaveKey, tmpBuf, sizeof(tmpBuf)) <= 0)
  {
    LOG_ERR("计算报文头存取关键字出错");
    return -1;
  }
  return tpHeadDump(ptpPort->bindQ, tmpBuf, ptpHead);
}

/***************************************************************
 * 提取异步报文头
 **************************************************************/
int tpHeadLoad(TPPORT *ptpPort, TPHEAD *ptpHead)
{
  FILE *fp, *pp;
  char buf[256], pathFile[256];
  char tmpBuf[MAXEXPLEN+1];

  if(tpFunExec(ptpPort->headLoadKey, tmpBuf, sizeof(tmpBuf)) <= 0)
  {
    LOG_ERR("计算报文头存取关键字出错");
    return -1;
  }

  snprintf(buf, sizeof(buf), "ls %s/rtm/tph/%d_%s.*",
           getenv("TPBUS_HOME"), ptpPort->bindQ, tmpBuf);
  if((pp = popen(buf, "r")) == NULL)
  {
    LOG_ERR("popen() error: %s", strerror(errno));
    return -1;
  }
  while(fgets(pathFile, sizeof(pathFile), pp) != NULL)
  {
    if(!(fp = fopen(strTrim(pathFile), "r")))
    {
      LOG_ERR("打开文件[%s]出错: %s", pathFile, strerror(errno));
      pclose(pp);
      return -1;
    }
    if(sizeof(TPHEAD) != fread(ptpHead, 1, sizeof(TPHEAD), fp))
    {
      LOG_ERR("读文件[%s]数据不完整", pathFile);
      fclose(fp);
      pclose(pp);
      return -1;
    }
    fclose(fp);
    if(ptpHead->tpId == atoi(strchr(pathFile, '.') + 1))
    {
      pclose(pp);
      return 0;
    }
  }
  pclose(pp);
  return ENOENT;
}

/***************************************************************
 * 保存交易上下文
 **************************************************************/
int tpTranCtxSave(int tpId, TPTRAN *ptpTran)
{
  FILE *fp;
  char pathFile[256];

  snprintf(pathFile, sizeof(pathFile), "%s/rtm/ctx/%d.ctx", getenv("TPBUS_HOME"), tpId);
  if(!(fp = fopen(pathFile, "w")))
  {
    LOG_ERR("打开文件[%s]出错: %s", pathFile, strerror(errno));
    return -1;
  }

  if(sizeof(TPTRAN) != fwrite(ptpTran, 1, sizeof(TPTRAN), fp))
  {
    LOG_MSG("写文件[%s]数据不完整", pathFile);
    fclose(fp);
    return -1;
  }
  fclose(fp);
  return 0;
}

/***************************************************************
 * 提取交易上下文
 **************************************************************/
int tpTranCtxLoad(int tpId, TPTRAN *ptpTran)
{
  FILE *fp;
  char pathFile[256];

  snprintf(pathFile, sizeof(pathFile), "%s/rtm/ctx/%d.ctx", getenv("TPBUS_HOME"), tpId);
  if(!(fp = fopen(pathFile, "r")))
  {
    LOG_ERR("打开文件[%s]出错: %s", pathFile, strerror(errno));
    return -1;
  }

  if(sizeof(TPTRAN) != fread(ptpTran, 1, sizeof(TPTRAN), fp))
  {
    LOG_MSG("读文件[%s]数据不完整", pathFile);
    fclose(fp);
    return -1;
  }
  fclose(fp);
  return 0;
}

/***************************************************************
 * 保存SAF报文
 **************************************************************/
int tpSafMsgSave(int safId, TPMSG *ptpMsg)
{
  FILE *fp;
  int  len;
  char pathFile[256];

  snprintf(pathFile, sizeof(pathFile), "%s/rtm/saf/%d.saf", getenv("TPBUS_HOME"), safId);
  if(!(fp = fopen(pathFile, "w")))
  {
    LOG_ERR("打开文件[%s]出错: %s", pathFile, strerror(errno));
    return -1;
  }

  len = ptpMsg->head.bodyLen + sizeof(TPHEAD);
  if(len != fwrite(ptpMsg, 1, len, fp))
  {
    LOG_MSG("写文件[%s]不完整", pathFile);
    fclose(fp);
    return -1;
  }
  fclose(fp);
  return 0;
}

/***************************************************************
 * 提取SAF报文
 **************************************************************/
int tpSafMsgLoad(int safId, TPMSG *ptpMsg)
{
  FILE *fp;
  int  len;
  char pathFile[256];

  snprintf(pathFile, sizeof(pathFile), "%s/rtm/saf/%d.saf", getenv("TPBUS_HOME"), safId);
  if(!(fp = fopen(pathFile, "r")))
  {
    LOG_ERR("打开文件[%s]出错: %s", pathFile, strerror(errno));
    return -1;
  }

  len = fread(ptpMsg, 1, sizeof(TPMSG), fp);
  if(len != (ptpMsg->head.bodyLen + sizeof(TPHEAD)))
  {
    LOG_MSG("从文件[%s]提取的数据长度不正确", pathFile);
    fclose(fp);
    return -1;
  }
  fclose(fp);
  return 0;
}

/***************************************************************
 * 初始化共享内存指针
 **************************************************************/
int tpShmTouch()
{
  int   shmKey, shmId;
  char *pShm;


  shmKey = (getenv("TPBUS_SHM_KEY")) ? atoi(getenv("TPBUS_SHM_KEY")) : SHM_KEY;

  if((shmId = shmget((key_t)shmKey, 0, IPC_EXCL | 0600)) == -1)
  {
    LOG_ERR("shmget() error: %s", strerror(errno));
    return -1;
  }
  if((gShm.pShmHead = (SHM_HEAD *)shmat(shmId, NULL, SHM_RND)) == (SHM_HEAD *) - 1)
  {
    LOG_ERR("shmat() error: %s", strerror(errno));
    return -1;
  }

  if((pShm = shmat(gShm.pShmHead->shmCtxId, NULL, SHM_RND)) == (char *) - 1)
  {
    LOG_ERR("shmat() error: %s", strerror(errno));
    shmdt(gShm.pShmHead);
    return -1;
  }
  gShm.pShmLink = (SHM_LINK *)pShm;
  gShm.pShmTran = (SHM_TRAN *)(pShm + gShm.pShmLink->tpTran.offset);
  gShm.pShmTphd = (SHM_TPHD *)(pShm + gShm.pShmLink->tpHead.offset);

  if((pShm = shmat(gShm.pShmHead->shmCfgId, NULL, SHM_RND)) == (char *) - 1)
  {
    LOG_ERR("shmat() error: %s", strerror(errno));
    shmdt(gShm.pShmHead);
    shmdt(gShm.pShmLink);
    return -1;
  }
  gShm.shmId     = gShm.pShmHead->shmCfgId;
  gShm.pShmList  = (SHM_LIST *)pShm;
  gShm.ptpConf   = (TPCONF *)(pShm + sizeof(SHM_LIST));
  gShm.ptpTask   = (TPTASK *)(pShm + gShm.pShmList->tpTask.offset);
  gShm.ptpPort   = (TPPORT *)(pShm + gShm.pShmList->tpPort.offset);
  gShm.ptpServ   = (TPSERV *)(pShm + gShm.pShmList->tpServ.offset);
  gShm.ptpProc   = (TPPROC *)(pShm + gShm.pShmList->tpProc.offset);
  gShm.ptpMapp   = (TPMAPP *)(pShm + gShm.pShmList->tpMapp.offset);
  gShm.ptpMoni   = (TPMONI *)(pShm + gShm.pShmList->tpMoni.offset);
  gShm.pBitmap   = (BITMAP *)(pShm + gShm.pShmList->bitmap.offset);
  gShm.pRoutGrp  = (ROUTGRP *)(pShm + gShm.pShmList->routGrp.offset);
  gShm.pRoutStmt = (ROUTSTMT *)(pShm + gShm.pShmList->routStmt.offset);
  gShm.pConvGrp  = (CONVGRP *)(pShm + gShm.pShmList->convGrp.offset);
  gShm.pConvStmt = (CONVSTMT *)(pShm + gShm.pShmList->convStmt.offset);

  gShm.pBizProc  = (void *)gShm.pConvStmt;
  gShm.pBizProc += sizeof(CONVSTMT) * gShm.pShmList->convStmt.nItems;

  pthread_mutex_init(&m_mtx, NULL);
  return 0;
}

/***************************************************************
 * 刷新共享内存指针
 **************************************************************/
int tpShmRetouch()
{
  char *pShm;

  pthread_mutex_lock(&m_mtx);

  while(!gShm.pShmHead->online) usleep(10);
  if(gShm.shmId == gShm.pShmHead->shmCfgId)
  {
    pthread_mutex_unlock(&m_mtx);
    return 0;
  }
  shmdt(gShm.pShmList);

  if((pShm = shmat(gShm.pShmHead->shmCfgId, NULL, SHM_RND)) == (char *) - 1)
  {
    LOG_ERR("shmat() error: %s", strerror(errno));
    pthread_mutex_unlock(&m_mtx);
    return -1;
  }
  gShm.shmId     = gShm.pShmHead->shmCfgId;
  gShm.pShmList  = (SHM_LIST *)pShm;
  gShm.ptpConf   = (TPCONF *)(pShm + sizeof(SHM_LIST));
  gShm.ptpTask   = (TPTASK *)(pShm + gShm.pShmList->tpTask.offset);
  gShm.ptpPort   = (TPPORT *)(pShm + gShm.pShmList->tpPort.offset);
  gShm.ptpServ   = (TPSERV *)(pShm + gShm.pShmList->tpServ.offset);
  gShm.ptpProc   = (TPPROC *)(pShm + gShm.pShmList->tpProc.offset);
  gShm.ptpMapp   = (TPMAPP *)(pShm + gShm.pShmList->tpMapp.offset);
  gShm.ptpMoni   = (TPMONI *)(pShm + gShm.pShmList->tpMoni.offset);
  gShm.pBitmap   = (BITMAP *)(pShm + gShm.pShmList->bitmap.offset);
  gShm.pRoutGrp  = (ROUTGRP *)(pShm + gShm.pShmList->routGrp.offset);
  gShm.pRoutStmt = (ROUTSTMT *)(pShm + gShm.pShmList->routStmt.offset);
  gShm.pConvGrp  = (CONVGRP *)(pShm + gShm.pShmList->convGrp.offset);
  gShm.pConvStmt = (CONVSTMT *)(pShm + gShm.pShmList->convStmt.offset);

  gShm.pBizProc  = (void *)gShm.pConvStmt;
  gShm.pBizProc += sizeof(CONVSTMT) * gShm.pShmList->convStmt.nItems;

  pthread_mutex_unlock(&m_mtx);
  return 0;
}

/***************************************************************
 * tpShmLock
 **************************************************************/
int tpShmLock(int num)
{
  struct sembuf buf;

  buf.sem_num = num;
  buf.sem_op  = -1;
  buf.sem_flg = SEM_UNDO;
  if(semop(gShm.pShmHead->semId, &buf, 1) == -1)
  {
    LOG_ERR("semop() error: %s", strerror(errno));
    return -1;
  }
  return 0;
}

/***************************************************************
 * tpShmUnlock
 **************************************************************/
int tpShmUnlock(int num)
{
  struct sembuf buf;

  buf.sem_num = num;
  buf.sem_op  = 1;
  buf.sem_flg = SEM_UNDO;
  if(semop(gShm.pShmHead->semId, &buf, 1) == -1)
  {
    LOG_ERR("semop() error: %s", strerror(errno));
    return -1;
  }
  return 0;
}

/***************************************************************
 * 读取任务配置信息
 **************************************************************/
TPTASK *tpTaskShmGet(int taskId, TPTASK *taskCopy)
{
  int i;

  if(taskId <= 0) return NULL;
  for(i = 0; i < gShm.pShmList->tpTask.nItems; i++)
  {
    if(taskId == gShm.ptpTask[i].taskId)
    {
      if(!taskCopy) return &gShm.ptpTask[i];
      memcpy(taskCopy, &gShm.ptpTask[i], sizeof(TPTASK));
    }
  }
  return NULL;
}

/***************************************************************
 * 读取端点配置信息
 **************************************************************/
TPPORT *tpPortShmGet(int q, TPPORT *portCopy)
{
  if(q <= 0 || q > NMAXBINDQ) return NULL;
  if(q != gShm.ptpPort[q-1].bindQ) return NULL;
  if(!portCopy) return &gShm.ptpPort[q-1];
  memcpy(portCopy, &gShm.ptpPort[q-1], sizeof(TPPORT));
  return portCopy;
}

/***************************************************************
 * 读取服务注册信息
 **************************************************************/
int tpServShmGet(char *servCode, TPSERV **pptpServ)
{
  int i;

  if(!servCode || !servCode[0]) return -1;
  i = hashValue(servCode, HASH_SIZE) + 1;

  while(i > 0)
  {
    if(strcmp(servCode, gShm.ptpServ[i-1].servCode) == 0)
    {
      if(pptpServ) *pptpServ = &gShm.ptpServ[i-1];
      return i;
    }
    i = gShm.ptpServ[i-1].next;
  }
  return -1;
}

/***************************************************************
 * 读取元服务信息
 **************************************************************/
int tpProcShmGet(int desQ, char *procCode, TPPROC **pptpProc)
{
  int i = desQ;

  if(!procCode || !procCode[0]) return -1;

  while(i > 0)
  {
    if(strcmp(procCode, gShm.ptpProc[i-1].procCode) == 0)
    {
      if(pptpProc) *pptpProc = &gShm.ptpProc[i-1];
      return i;
    }
    i = gShm.ptpProc[i-1].next;
  }
  return -1;
}

/***************************************************************
 * 读取服务映射配置信息
 **************************************************************/
int tpMappShmGet(int orgQ, char *tranCode, TPMAPP **pptpMapp)
{
  int i = orgQ, j = 0;

  while(i > 0)
  {
    if(!strchr(gShm.ptpMapp[i-1].tranCodeReg, '.'))
    {
      if(strcmp(tranCode, gShm.ptpMapp[i-1].tranCodeReg) == 0)
      {
        if(pptpMapp) *pptpMapp = &gShm.ptpMapp[i-1];
        return i;
      }
    }
    else
    {
      if(regMatch(tranCode, gShm.ptpMapp[i-1].tranCodeReg) == 0) j = i;
    }
    i = gShm.ptpMapp[i-1].next;
  }
  if(j > 0)
  {
    if(pptpMapp) *pptpMapp = &gShm.ptpMapp[j-1];
    return j;
  }
  return -1;
}

/***************************************************************
 * 读取ISO8583位图配置信息
 **************************************************************/
int bitmapShmGet(char *bitmapId, BITMAP **ppBitmap)
{
  int i;

  for(i = 1; i <= gShm.pShmList->bitmap.nItems; i++)
  {
    if(strcmp(bitmapId, gShm.pBitmap[i-1].id) == 0)
    {
      if(ppBitmap) *ppBitmap = &gShm.pBitmap[i-1];
      return i;
    }
  }
  return -1;
}

/***************************************************************
 * 读取路由组配置信息
 **************************************************************/
int tpRoutShmGet(int grpId, ROUTGRP **ppRoutGrp)
{
  int i;

  if(grpId <= 0) return -1;
  i = (grpId % HASH_SIZE) + 1;

  while(i > 0)
  {
    if(grpId == gShm.pRoutGrp[i-1].grpId)
    {
      if(ppRoutGrp) *ppRoutGrp = &gShm.pRoutGrp[i-1];
      return i;
    }
    i = gShm.pRoutGrp[i-1].next;
  }
  return -1;
}

/***************************************************************
 * 读取格式转换组配置信息
 **************************************************************/
int tpConvShmGet(int grpId, CONVGRP **ppConvGrp)
{
  int i;

  if(grpId <= 0) return -1;
  i = (grpId % HASH_SIZE) + 1;

  while(i > 0)
  {
    if(grpId == gShm.pConvGrp[i-1].grpId)
    {
      if(ppConvGrp) *ppConvGrp = &gShm.pConvGrp[i-1];
      return i;
    }
    i = gShm.pConvGrp[i-1].next;
  }
  return -1;
}

/***************************************************************
 * 获取平台流水号
 **************************************************************/
int tpNewId()
{
  int  ret;
  FILE *fp;
  char tmpBuf[256];

  if(tpShmLock(SEM_TPID) != 0) return -1;
  if(gShm.pShmHead->tpId < MAXSRLNUM)
  {
    gShm.pShmHead->tpId++;
  }
  else
  {
    gShm.pShmHead->tpId = 1;
  }
  if(!(gShm.pShmHead->tpId % 1000))    /*每1000笔保存1次流水号*/
  {
    snprintf(tmpBuf, sizeof(tmpBuf), "%s/rtm/tpId.dump", getenv("TPBUS_HOME"));
    if(!(fp = fopen(tmpBuf, "w")))
    {
      LOG_ERR("fopen(%s) error: %s", tmpBuf, strerror(errno));
    }
    else
    {
      fprintf(fp, "%d\n", gShm.pShmHead->tpId);
      fclose(fp);
    }
  }
  ret = gShm.pShmHead->tpId;
  tpShmUnlock(SEM_TPID);
  return ret;
}

/***************************************************************
 * 获取SAF流水号
 **************************************************************/
int tpNewSafId()
{
  int ret;

  if(tpShmLock(SEM_SAFID) != 0) return -1;
  if(gShm.pShmHead->safId < MAXSRLNUM) gShm.pShmHead->safId++;
  else gShm.pShmHead->safId = 1;
  ret = gShm.pShmHead->safId;
  tpShmUnlock(SEM_SAFID);
  return ret;
}

/***************************************************************
 * 申请共享内存上下文空间
 **************************************************************/
int tpTranShmAlloc(int servIndex)
{
  int i, tail;

  if(tpShmLock(SEM_CTX) != 0)
  {
    LOG_ERR("tpShmLock() error");
    return -1;
  }

  i = gShm.pShmLink->tpTran.free;
  while(1)
  {
    if(!gShm.pShmTran[i-1].tag) break;
    if(++i > gShm.ptpConf->nMaxTrans) i = 1;
    if(i == gShm.pShmLink->tpTran.free)
    {
      tpShmUnlock(SEM_CTX);
      LOG_ERR("共享内存空间已满");
      return SHM_FULL;
    }
  }
  gShm.pShmTran[i-1].tag = 2;
  gShm.pShmTran[i-1].next = 0;
  tail = gShm.pShmLink->tpTran.tail;
  gShm.pShmTran[i-1].prev = tail;
  if(tail > 0) gShm.pShmTran[tail-1].next = i;
  if(gShm.pShmLink->tpTran.head == 0) gShm.pShmLink->tpTran.head = i;
  gShm.pShmLink->tpTran.tail = i;
  gShm.pShmLink->tpTran.free = (i < gShm.ptpConf->nMaxTrans) ? i + 1 : 1;
  gShm.ptpServ[servIndex-1].nActive++;
  gShm.pShmHead->nActive++;
  tpShmUnlock(SEM_CTX);
  return i;
}

/***************************************************************
 * 释放共享内存上下文空间
 **************************************************************/
int tpTranShmRelease(int tpId, int shmRef)
{
  int k, prev, next;

  if(shmRef <= 0 || shmRef > gShm.ptpConf->nMaxTrans)
  {
    LOG_ERR("共享内存引用值[%d]不正确", shmRef);
    return -1;
  }
  if(tpShmLock(SEM_CTX) != 0)
  {
    LOG_ERR("tpShmLock() error");
    return -1;
  }
  if(!gShm.pShmTran[shmRef-1].tag)
  {
    tpShmUnlock(SEM_CTX);
    LOG_ERR("共享内存已标记为释放");
    return -1;
  }
  if(tpId != gShm.pShmTran[shmRef-1].tpTran.tpLog.tpId)
  {
    tpShmUnlock(SEM_CTX);
    LOG_ERR("流水号与共享内存中的不一致");
    return -1;
  }

  gShm.pShmTran[shmRef-1].tag = 0;
  k = gShm.pShmTran[shmRef-1].tpTran.tpLog.servIndex;
  gShm.pShmLink->tpTran.free = shmRef;
  prev = gShm.pShmTran[shmRef-1].prev;
  next = gShm.pShmTran[shmRef-1].next;
  if(shmRef == gShm.pShmLink->tpTran.head && shmRef == gShm.pShmLink->tpTran.tail)
  {
    gShm.pShmLink->tpTran.head = 0;
    gShm.pShmLink->tpTran.tail = 0;
  }
  else
  {
    if(shmRef == gShm.pShmLink->tpTran.head)
    {
      if(next > 0) gShm.pShmTran[next-1].prev = 0;
      gShm.pShmLink->tpTran.head = next;
    }
    else if(shmRef == gShm.pShmLink->tpTran.tail)
    {
      if(prev > 0) gShm.pShmTran[prev-1].next = 0;
      gShm.pShmLink->tpTran.tail = prev;
    }
    else
    {
      gShm.pShmTran[prev-1].next = next;
      gShm.pShmTran[next-1].prev = prev;
    }
  }
  gShm.ptpServ[k-1].nActive--;
  gShm.pShmHead->nActive--;
  tpShmUnlock(SEM_CTX);
  return 0;
}

/***************************************************************
 * 流水记录标记为使用状态
 **************************************************************/
int tpTranSetBusy(int tpId, int shmRef)
{
  if(shmRef <= 0 || shmRef > gShm.ptpConf->nMaxTrans)
  {
    LOG_ERR("共享内存引用值[%d]不正确", shmRef);
    return -1;
  }
  if(tpShmLock(SEM_CTX) != 0)
  {
    LOG_ERR("tpShmLock() error");
    return -1;
  }
  if(gShm.pShmTran[shmRef-1].tag == 1)
  {
    if(tpId == gShm.pShmTran[shmRef-1].tpTran.tpLog.tpId)
    {
      gShm.pShmTran[shmRef-1].tag = 2;
      tpShmUnlock(SEM_CTX);
      return 0;
    }
  }
  tpShmUnlock(SEM_CTX);
  return SHM_NONE;
}

/***************************************************************
 * 流水记录标记为空闲状态
 **************************************************************/
int tpTranSetFree(int tpId, int shmRef)
{
  if(shmRef <= 0 || shmRef > gShm.ptpConf->nMaxTrans)
  {
    LOG_ERR("共享内存引用值[%d]不正确", shmRef);
    return -1;
  }
  if(tpShmLock(SEM_CTX) != 0)
  {
    LOG_ERR("tpShmLock() error");
    return -1;
  }
  if(gShm.pShmTran[shmRef-1].tag == 2)
  {
    if(tpId == gShm.pShmTran[shmRef-1].tpTran.tpLog.tpId)
    {
      gShm.pShmTran[shmRef-1].tag = 1;
      tpShmUnlock(SEM_CTX);
      return 0;
    }
  }
  tpShmUnlock(SEM_CTX);
  return SHM_NONE;
}

/***************************************************************
 * 共享内存异步报文头链表位置定位
 **************************************************************/
int tpHeadShmLocate(int portId,char *key,int *prevId,int *nextId)
{
  int   i,j,cnt,flag;

  if(gShm.pShmLink->tpHead.cnt == 0) {
    *prevId = 0;
    *nextId = 0;
    return 0;
  }

  i   = gShm.pShmLink->tpHead.head;
  cnt = gShm.pShmLink->tpHead.cnt;
  while(1) {
    if(portId == gShm.pShmTphd[i-1].desQ &&
        strcmp(key, gShm.pShmTphd[i-1].key) == 0) {
      *prevId = *nextId = i;
      return SHM_DUPHDKEY;
    }

    if( (portId > gShm.pShmTphd[i-1].desQ) ||
        ( (portId == gShm.pShmTphd[i-1].desQ) && strcmp(key, gShm.pShmTphd[i-1].key) > 0) ) {
      flag = 1;
      for(j=0;j<cnt/2;j++) {
        if (i == gShm.pShmLink->tpHead.tail) break;
        i = gShm.pShmTphd[i-1].next;
      }
    }
    else{
      flag = 0;
      for(j=0;j<cnt/2;j++) {
        if (i == gShm.pShmLink->tpHead.head) break;
        i = gShm.pShmTphd[i-1].prev;
      }
    }

    if(cnt ==1) break;
    cnt = (cnt + 1)/2;
  }

  if(flag) {
    *prevId = i;
    *nextId = gShm.pShmTphd[i-1].next;
  }
  else {
    *prevId = gShm.pShmTphd[i-1].prev;
    *nextId = i;
  }

  return 0;
}

/***************************************************************
 * 申请共享内存异步报文头空间
 **************************************************************/
int tpHeadShmAlloc(int portId,char *key)
{
  int i, tail;
  int ret,prevId,nextId;

  i = gShm.pShmLink->tpHead.free;
  while(1)
  {
    if(!gShm.pShmTphd[i-1].tag) break;
    if(++i > gShm.ptpConf->nMaxTrans) i = 1;
    if(i == gShm.pShmLink->tpHead.free)
    {
      LOG_ERR("异步报文头空间已满");
      return SHM_FULL;
    }
  }
  
  /*链表排序，重置链表*/
  ret = tpHeadShmLocate(portId,key,&prevId,&nextId);
  if (SHM_DUPHDKEY == ret) { /*键值重复*/
    LOG_ERR("端点[%d]的异步报文头KEY[%s]已存在",portId,key);
    return ret;
  }
  	
  gShm.pShmTphd[i-1].tag = 1;
  gShm.pShmTphd[i-1].prev = prevId;
  gShm.pShmTphd[i-1].next = nextId;
  if(prevId) gShm.pShmTphd[prevId-1].next = i;
  if(nextId) gShm.pShmTphd[nextId-1].prev = i;
  if(0 == prevId) gShm.pShmLink->tpHead.head = i;
  if(0 == nextId) gShm.pShmLink->tpHead.tail = i;
  gShm.pShmLink->tpHead.free = (i < gShm.ptpConf->nMaxTrans) ? i + 1 : 1;
  gShm.pShmLink->tpHead.cnt++;
  
  return i;
}

/***************************************************************
 * 向共享内存中存入异步报文头
 **************************************************************/
int tpHeadShmPut(int headId, TPPORT *ptpPort, TPHEAD *ptpHead)
{
  int  i;
  char tmpBuf[MAXEXPLEN+1];
  struct timeb tb;

  if(tpFunExec(ptpPort->headSaveKey, tmpBuf, sizeof(tmpBuf)) <= 0)
  {
    LOG_ERR("计算报文头存取关键字出错");
    return -1;
  }

  LOG_MSG("headSaveKey: save key=[%s]",tmpBuf); 

  if(tpShmLock(SEM_TPHD) != 0)
  {
    LOG_ERR("tpShmLock() error");
    return -1;
  }
  
  headId = tpHeadShmAlloc(ptpPort->bindQ,tmpBuf);
  if(headId < 0) {
    tpShmUnlock(SEM_TPHD);
    return headId;
  }
  
  ftime(&tb);
  gShm.pShmTphd[headId-1].overTime = tb.time + ptpPort->timeout;
  gShm.pShmTphd[headId-1].desQ = ptpPort->bindQ;
  strncpy(gShm.pShmTphd[headId-1].key, tmpBuf, sizeof(gShm.pShmTphd[headId-1].key) - 1);
  memcpy(&gShm.pShmTphd[headId-1].tpHead, ptpHead, sizeof(TPHEAD));
  
  tpShmUnlock(SEM_TPHD);
  return headId;
}

/***************************************************************
 * 从共享内存中提取异步报文头
 **************************************************************/
int tpHeadShmGet(TPPORT *ptpPort, TPHEAD *ptpHead)
{
  int  i,ret;
  int  prevId,nextId;
  char tmpBuf[MAXEXPLEN+1];

  if(tpFunExec(ptpPort->headLoadKey, tmpBuf, sizeof(tmpBuf)) <= 0)
  {
    LOG_ERR("计算报文头存取关键字出错");
    return -1;
  }

  LOG_MSG("headLoadKey: load key=[%s]",tmpBuf);

  if(tpShmLock(SEM_TPHD) != 0)
  {
    LOG_ERR("tpShmLock() error");
    return -1;
  }
  i = gShm.pShmLink->tpHead.head;

  ret = tpHeadShmLocate(ptpPort->bindQ,tmpBuf,&prevId,&nextId);
  if (SHM_DUPHDKEY != ret) {
    tpShmUnlock(SEM_TPHD);
    return SHM_NONE;
  }
  
  memcpy(ptpHead, &gShm.pShmTphd[prevId-1].tpHead, sizeof(TPHEAD));
  tpShmUnlock(SEM_TPHD);
  return prevId;
}

/***************************************************************
 * 释放共享内存中的异步报文头
 **************************************************************/
int tpHeadShmRelease(int tpId, int headId)
{
  int prev, next;

  if(headId <= 0 || headId > gShm.ptpConf->nMaxTrans)
  {
    LOG_ERR("共享内存引用值[%d]不正确", headId);
    return -1;
  }
  if(tpShmLock(SEM_TPHD) != 0)
  {
    LOG_ERR("tpShmLock() error");
    return -1;
  }
  if(!gShm.pShmTphd[headId-1].tag)
  {
    tpShmUnlock(SEM_TPHD);
    LOG_ERR("共享内存已标记为释放");
    return -1;
  }
  if(tpId != gShm.pShmTphd[headId-1].tpHead.tpId)
  {
    tpShmUnlock(SEM_TPHD);
    LOG_ERR("流水号与共享内存中的不一致");
    return -1;
  }

  gShm.pShmTphd[headId-1].tag = 0;
  gShm.pShmLink->tpHead.free = headId;
  prev = gShm.pShmTphd[headId-1].prev;
  next = gShm.pShmTphd[headId-1].next;
  if(headId == gShm.pShmLink->tpHead.head && headId == gShm.pShmLink->tpHead.tail)
  {
    gShm.pShmLink->tpHead.head = 0;
    gShm.pShmLink->tpHead.tail = 0;
  }
  else
  {
    if(headId == gShm.pShmLink->tpHead.head)
    {
      if(next > 0) gShm.pShmTphd[next-1].prev = 0;
      gShm.pShmLink->tpHead.head = next;
    }
    else if(headId == gShm.pShmLink->tpHead.tail)
    {
      if(prev > 0) gShm.pShmTphd[prev-1].next = 0;
      gShm.pShmLink->tpHead.tail = prev;
    }
    else
    {
      gShm.pShmTphd[prev-1].next = next;
      gShm.pShmTphd[next-1].prev = prev;
    }
  }
  gShm.pShmLink->tpHead.cnt--;
  tpShmUnlock(SEM_TPHD);
  return 0;
}

/***************************************************************
 * 查找超时交易处理记录
 **************************************************************/
int tpShmTimeoutFind(TPTRAN arr[], int nMax)
{
  int  i, j = 0, k;
  int  prev, next;
  time_t t;

  time(&t);

  if(tpShmLock(SEM_CTX) != 0)
  {
    LOG_ERR("tpShmLock() error");
    return -1;
  }

  i = gShm.pShmLink->tpTran.head;
  while(i && j < nMax)
  {
    if(gShm.pShmTran[i-1].tag != 1 || gShm.pShmTran[i-1].tpTran.tpLog.overTime > t)
    {
      i = gShm.pShmTran[i-1].next;
      continue;
    }

    /*复制超时上下文*/
    memcpy(&arr[j++], &gShm.pShmTran[i-1].tpTran, sizeof(TPTRAN));

    /*释放上下文空间*/
    gShm.pShmTran[i-1].tag = 0;
    k = gShm.pShmTran[i-1].tpTran.tpLog.servIndex;
    gShm.pShmLink->tpTran.free = i;
    prev = gShm.pShmTran[i-1].prev;
    next = gShm.pShmTran[i-1].next;
    if(i == gShm.pShmLink->tpTran.head && i == gShm.pShmLink->tpTran.tail)
    {
      gShm.pShmLink->tpTran.head = 0;
      gShm.pShmLink->tpTran.tail = 0;
    }
    else
    {
      if(i == gShm.pShmLink->tpTran.head)
      {
        if(next > 0) gShm.pShmTran[next-1].prev = 0;
        gShm.pShmLink->tpTran.head = next;
      }
      else if(i == gShm.pShmLink->tpTran.tail)
      {
        if(prev > 0) gShm.pShmTran[prev-1].next = 0;
        gShm.pShmLink->tpTran.tail = prev;
      }
      else
      {
        gShm.pShmTran[prev-1].next = next;
        gShm.pShmTran[next-1].prev = prev;
      }
    }
    gShm.ptpServ[k-1].nActive--;
    gShm.pShmHead->nActive--;
    i = gShm.pShmTran[i-1].next;
  }
  tpShmUnlock(SEM_CTX);
  return j;
}


/***************************************************************
 * 打开平台数据库
 **************************************************************/
int tpSQLiteOpen(DBHDL **dbh)
{
  char fileName[256];

  snprintf(fileName, sizeof(fileName), "%s/rtm/tpbus.db", getenv("TPBUS_HOME"));
  return sqliteOpen(fileName, dbh);
}

/***************************************************************
 * 新增流水记录
 **************************************************************/
int tpLogInsert(DBHDL *dbh, TPLOG *ptpLog)
{
  char sql[256];

  snprintf(sql, sizeof(sql), "insert into tp_log values(%d,%d,'%s','%s',%ld,%d)",
           ptpLog->tpId,
           ptpLog->orgQ,
           ptpLog->tranCode,
           ptpLog->servCode,
           ptpLog->beginTime,
           ptpLog->status);

  if(sqliteExecute(dbh, sql) != 0) return -1;
  return 0;
}

/***************************************************************
 * 删除流水记录
 **************************************************************/
int tpLogDelete(DBHDL *dbh, int tpId)
{
  char sql[256];

  snprintf(sql, sizeof(sql), "delete from tp_log where tp_id = %d", tpId);
  if(sqliteExecute(dbh, sql) != 0) return -1;
  return 0;
}

/***************************************************************
 * 更新流水记录
 **************************************************************/
int tpLogUpdate(DBHDL *dbh, TPLOG *ptpLog)
{
  char sql[256];

  snprintf(sql, sizeof(sql), "update tp_log set status = %d where tp_id = %d",
           ptpLog->status, ptpLog->tpId);

  if(sqliteExecute(dbh, sql) != 0) return -1;
  return 0;
}

/***************************************************************
 * 新增SAF记录
 **************************************************************/
int tpSafInsert(DBHDL *dbh, TPSAF *ptpSaf)
{
  int  ret;
  char sql[256];

  snprintf(sql, sizeof(sql), "insert into tp_saf values(%d, %d, %d, '%s', '%s', %d, %ld)",
           ptpSaf->safId,
           ptpSaf->tpId,
           ptpSaf->desQ,
           ptpSaf->servCode,
           ptpSaf->procCode,
           ptpSaf->safNum,
           ptpSaf->overTime);

  if((ret = sqliteExecute(dbh, sql)) == 0) return 0;
  if(ret != SQLITE_CONSTRAINT) return -1;
  if(tpSafDelete(dbh, ptpSaf->safId) != 0) return -1;
  if(sqliteExecute(dbh, sql) != 0) return -1;
  return 0;
}

/***************************************************************
 * 删除SAF记录
 **************************************************************/
int tpSafDelete(DBHDL *dbh, int safId)
{
  char sql[256];

  snprintf(sql, sizeof(sql), "delete from tp_saf where saf_id = %d", safId);
  if(sqliteExecute(dbh, sql) != 0) return -1;
  return 0;
}

/***************************************************************
 * 更新SAF记录
 **************************************************************/
int tpSafUpdate(DBHDL *dbh, TPSAF *ptpSaf)
{
  char sql[256];

  snprintf(sql, sizeof(sql), "update tp_saf set saf_num = %d, over_time = %ld where saf_id = %d",
           ptpSaf->safNum, ptpSaf->overTime, ptpSaf->safId);

  if(sqliteExecute(dbh, sql) != 0) return -1;
  return 0;
}

/***************************************************************
 * 查找超时SAF记录
 **************************************************************/
int tpSafTimeoutFind(DBHDL *dbh, TPSAF arr[], int nMax)
{
  int  i = 0;
  char sql[256];
  sqlite3_stmt *pStmt;
  time_t t;

  time(&t);
  snprintf(sql, sizeof(sql), "select * from tp_saf where over_time <= %ld order by saf_id", t);

  if(sqlitePrepare(dbh, sql, &pStmt) != 0) return -1;
  while(i < nMax && SQLITE_ROW == sqliteFetch(pStmt))
  {
    arr[i].safId = sqliteGetInt(pStmt, 0);
    arr[i].tpId = sqliteGetInt(pStmt, 1);
    arr[i].desQ = sqliteGetInt(pStmt, 2);
    sqliteGetString(pStmt, 3, arr[i].servCode, MAXKEYLEN + 1);
    sqliteGetString(pStmt, 4, arr[i].procCode, MAXKEYLEN + 1);
    arr[i].safNum = sqliteGetInt(pStmt, 5);
    arr[i].overTime = sqliteGetInt(pStmt, 6);
    i++;
  }

  sqliteRelease(pStmt);
  return i;
}

static char GetCharIndex(char c)
{
  if((c >= 'A') && (c <= 'Z'))
    return c - 'A';
  else if((c >= 'a') && (c <= 'z'))
    return c - 'a' + 26;
  else if((c >= '0') && (c <= '9'))
    return c - '0' + 52;
  else if(c == '+')
    return 62;
  else if(c == '/')
    return 63;
  else if(c == '=')
    return 0;
  return 0;
}

/*Base64 解码，参数：结果，密文，密文长度 */
/*Base64 解码函数*/
int Base64Decode(char *lpString, const char *lpSrc, int sLen)
{
  char    lpCode[4];
  int     vLen = 0;
  /*索引指针 */
  char    *ind;
  /*初始化指针*/
  ind     = (char *)lpSrc;

  /*Base64编码长度必定是4的倍数，包括'='*/
  if(sLen % 4)
  {
    lpString[0] = '\0';
    return -1;
  }
  /*不足三个字符，忽略*/
  while(sLen > 2)
  {
    lpCode[0] = GetCharIndex(lpSrc[0]);
    lpCode[1] = GetCharIndex(lpSrc[1]);
    lpCode[2] = GetCharIndex(lpSrc[2]);
    lpCode[3] = GetCharIndex(lpSrc[3]);

    *lpString++ = (lpCode[0] << 2) | (lpCode[1] >> 4);
    *lpString++ = (lpCode[1] << 4) | (lpCode[2] >> 2);
    *lpString++ = (lpCode[2] << 6) | (lpCode[3]);

    lpSrc += 4;
    sLen -= 4;
    vLen += 3;
  }
  if(lpCode[3] == 0x00) vLen--;
  if(lpCode[2] == 0x00) vLen--;
  lpString[vLen] = 0x00;
  return vLen;
}

/*
 * * Encode a buffer into base64 format
 * * Return -1   encode fail
 * *        n > 0 encoded msg len
 * */
/** base64 encoding map for shifted byte **/

/*Base64 编码，参数：结果，原文，报文长度 */
/*Base64 编码函数*/
int Base64Encode(char *dst, const char *src, int slen)
{
  int i, n, nl;
  unsigned int C1, C2, C3;
  const char *p;
  char *ptr = dst;

  if(0 == slen)
    return -1;

  n = slen / 3;
  nl = slen % 3;

  /* 3-bytes groups left 1/2 bytes,need added in */
  if(nl)
    n += 1;

  p = src;
  for(i = 0; i < n; i++)
  {
    C1 = *p++;
    if(1 == nl && (n - 1) == i)
      C2 = 0;
    else
      C2 = *p++;
    if(nl && (n - 1) == i)
      C3 = 0;
    else
      C3 = *p++;


    *ptr++ = b64encmap[(C1 >> 2) & 0x3F];
    *ptr++ = b64encmap[(((C1 << 6) >> 2) & 0x3F) | ((C2 >> 4) & 0x0F)];
    *ptr++ = b64encmap[(((C2 << 4) >> 2) & 0x3F) | ((C3 >> 6) & 0x03)];
    *ptr++ = b64encmap[C3 & 0x3F];
  }
  *ptr = '\0';
  switch(nl)
  {
  case 1:
    *(ptr - 2) = '=';
  case 2:
    *(ptr - 1) = '=';
    break;
  default:
    break;
  }
  return n << 2;
}
