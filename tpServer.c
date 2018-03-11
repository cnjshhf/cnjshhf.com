/***************************************************************
 * 主控调度程序
 **************************************************************/
#include "tpbus.h"

#define NMAX  10

void *tpProcessThread(void *arg);
int tpProcess(MQHDL *mqh, DBHDL *dbh, TPCTX *ptpCtx);

/***************************************************************
 * 主函数
 **************************************************************/
int main(int argc, char *argv[])
{
    int    i,j,k,ret;
    int    desQ = 0;
    int    convId = 0;
    int    prev, next;  
    time_t t;
    MQHDL  *mqh = NULL;
    DBHDL  *dbh = NULL;
    TPMSG  tpMsg;
    TPLOG  *ptpLog;
    TPREV  *ptpRev;
    TPPORT *ptpPort;
    TPTRAN tpTranArray[NMAX];
    TPTASK tpTask;
    CONVGRP *pConvGrp;
    TPCTX  tpCtx;
    TPMSG  *ptpMsg  = &tpCtx.tpMsg;
    TPHEAD *ptpHead = &ptpMsg->head;

    /*检查授权许可*/
    if (tpLicence() != 0) {
      printf("无授权许可或已过期\n");
      return -1;
    }

    /*检查命令行参数*/
    if (argc != 2) {
        printf("Usage: %s <任务ID>\n", argv[0]);
        return -1;
    }

    /*初始化进程*/
    /*if (tpTaskInit(TSK_SERVER, "tpServer", &tpTask) != 0) { */
    if (tpTaskInit(atoi(argv[1]), "tpServer", &tpTask) != 0) { 
        LOG_ERR("初始化进程出错");
        return -1;
    }
    
    /*初始化配置参数*/
    if (tpGetParamInt(tpTask.params, "desQ", &desQ) != 0) desQ = -1;
    if (tpGetParamInt(tpTask.params, "convId", &convId) != 0) convId = -1; 
    LOG_MSG("参数[desQ]=[%d]", desQ);
    LOG_MSG("超时附加处理报文打包组=[%d]",convId);

    /*初始化队列*/
    if ((ret = mq_init()) != 0) {
        LOG_ERR("mq_init() error: retcode=%d", ret);
        LOG_ERR("初始化队列出错");
        return -1;
    }

    /*绑定队列*/
    if ((ret = mq_bind(Q_SVR, &mqh)) != 0) {
        LOG_ERR("mq_bind() error: retcode=%d", ret);
        LOG_ERR("绑定队列[%d]出错", Q_SVR);
        return -1;
    }

    /*打开数据库*/
    if (tpSQLiteOpen(&dbh) != 0) {
       LOG_ERR("打开数据库出错");
       return -1;
    }

    /*创建处理线程池*/
    for (i=0; i<gShm.ptpConf->nMaxProcs; i++) {
        if (tpThreadCreate(tpProcessThread, NULL) != 0) {
            LOG_ERR("创建处理线程失败: %s", strerror(errno));
            return -1;
        }
    }

    tpShmLock(SEM_TPTMO);  /*防止多个进程同时轮询*/   

    /*轮询超时交易*/
    while (1) {
        /*释放超时报文头 bgn --add by xgh 20170216*/
        time(&t);
        if(tpShmLock(SEM_TPHD) != 0) {
          LOG_ERR("tpShmLock() error");
          return -1;
        }

        i = gShm.pShmLink->tpHead.head;
        while(i) {
          if(gShm.pShmTphd[i-1].tag != 1 || gShm.pShmTphd[i-1].overTime > t) {
            i = gShm.pShmTphd[i-1].next;
            continue;
          }
          tpShmUnlock(SEM_TPHD);
          tpHeadShmRelease(gShm.pShmTphd[i-1].tpHead.tpId, i);
          LOG_MSG("释放超时报文头记录，id=[%d] qid=[%d] key=[%s]",i,gShm.pShmTphd[i-1].desQ,gShm.pShmTphd[i-1].key);
          tpShmLock(SEM_TPHD);
          i = gShm.pShmTphd[i-1].next;
        }
        tpShmUnlock(SEM_TPHD);
        /*释放超时报文头 end --add by xgh 20170216*/

        if ((j = tpShmTimeoutFind(tpTranArray, NMAX)) < 0) {
            LOG_ERR("查找超时交易出错");
            return -1;
        }

        if (j==0) { /*无超时交易*/
            if (tpShmRetouch() != 0) {
                LOG_ERR("刷新共享内存指针出错");
                continue;
            }
            sleep(gShm.ptpConf->ntsMonLog);
            continue;
        }

        for (i=0; i<j; i++) {
            ptpLog = &tpTranArray[i].tpLog;
            ptpRev = tpTranArray[i].tpRevs;
            ptpLog->status = TP_TIMEOUT;
            LOG_MSG("超时交易: tpId=[%d],orgQ=[%d],tranCode=[%s],servCode=[%s],desQ=[%d],procCode=[%s]", ptpLog->tpId, ptpLog->orgQ, ptpLog->tranCode, ptpLog->servCode, ptpLog->desQ, ptpLog->procCode);

            /*DUMP异步报文头*/
            if (ptpLog->headId) {
                if (gShm.pShmTphd[ptpLog->headId-1].tag &&
                    gShm.pShmTphd[ptpLog->headId-1].desQ == ptpLog->desQ &&
                    gShm.pShmTphd[ptpLog->headId-1].tpHead.tpId == ptpLog->tpId) {

                    if (tpHeadDump(gShm.pShmTphd[ptpLog->headId-1].desQ, 
						       gShm.pShmTphd[ptpLog->headId-1].key,
						       &gShm.pShmTphd[ptpLog->headId-1].tpHead) != 0) {
                        LOG_ERR("DUMP异步报文头出错");
                        tpHeadShmRelease(ptpLog->tpId, ptpLog->headId);
                        continue;
                    }
                }
                tpHeadShmRelease(ptpLog->tpId, ptpLog->headId);
            }

            if (!(ptpPort = tpPortShmGet(ptpLog->orgQ, NULL))) {
                LOG_ERR("读取端点[%d]配置出错", ptpLog->orgQ);
                continue;
            }
            for (k=MAXREVNUM-1; k>=0; k--) { if (ptpRev[k].desQ == ptpLog->desQ) break; }
            if (k>=0 && !ptpRev[k].forceRev) ptpRev[k].desQ = 0;
            if (!ptpPort->convGrp && !ptpRev[0].desQ) continue;

            /*保存上下文*/
            if (tpTranCtxSave(ptpLog->tpId, &tpTranArray[i]) != 0) {
                LOG_ERR("保存上下文[tpId=%d]出错", ptpLog->tpId);
                continue;
            }

            /*组织报文头*/
            memset(&tpMsg.head, 0, sizeof(TPHEAD));
            tpMsg.head.tpId = ptpLog->tpId;
            tpMsg.head.startQ = ptpLog->startQ;    /*add by xgh 20161213*/
            tpMsg.head.orgQ = ptpLog->orgQ;
            tpMsg.head.desQ = ptpLog->orgQ;
            strncpy(tpMsg.head.tranCode, ptpLog->tranCode, MAXKEYLEN);
            strncpy(tpMsg.head.servCode, ptpLog->servCode, MAXKEYLEN);
            tpMsg.head.beginTime = ptpLog->beginTime;
            tpMsg.head.msgType = MSGSYS_TIMEOUT;
            tpMsg.head.monFlag = ptpLog->monFlag;
            tpMsg.head.status = ptpLog->status;

            if (gShm.ptpConf->sysMonFlag) tpSendToSysMon(&tpMsg.head); /*发送监控报文*/

            /*触发超时冲正*/
            tpMsg.head.bodyLen = 0;
            if ((ret = mq_post(mqh, NULL, Q_REV, 0, NULL, (char *)&tpMsg.head, sizeof(TPHEAD), 0)) != 0) {
                LOG_ERR("mq_post() error: retcode=%d", ret);
                LOG_ERR("写事务队列[%d]出错", Q_REV);
                LOG_ERR("向事务管理程序发送冲正命令出错");
                continue;
            }
            LOG_MSG("向事务管理程序发送冲正命令...[ok]");

            if ( (desQ > 0) && (convId > 0) ) {
                /*读取端点配置*/
                if (!(ptpPort = tpPortShmGet(desQ, NULL))) {
                    LOG_ERR("读取端点[%d]配置出错", desQ);
                    continue;
                }
                if (tpConvShmGet(convId, &pConvGrp) <= 0) {
                    LOG_DBG("转换组[%d]不存在", convId);
                    continue;
                }
                /*执行打包转换组*/
                 tpPreBufFree(&tpCtx.preBuf);
                if (tpMsgConvPack(pConvGrp, &ptpPort->fmtDefs, &tpCtx) != 0) {
                    LOG_ERR("执行打包转换组[%d]出错", pConvGrp->grpId);
                    continue;
                }
                LOG_MSG("执行打包转换组[%d]...[ok]", pConvGrp->grpId);
                if (hasLogLevel(DEBUG)) LOG_HEX(ptpMsg->body, ptpHead->bodyLen);
    
                if ((ret = mq_post(mqh, NULL, desQ, 0, NULL, ptpMsg->body, ptpHead->bodyLen, 0)) != 0) {
                    LOG_ERR("写附加处理队列[%d]出错", desQ);
                    continue;
                }
            }
        }
    }
    return 0;
}

/***************************************************************
 * 交易处理线程
 **************************************************************/
void *tpProcessThread(void *arg)
{
    int    q,ret,headId,msgLen;
    char   logPrefix[21];
    char   clazz[MSGCLZLEN+1];
    MQHDL  *mqh = NULL;
    DBHDL  *dbh = NULL;
    TPPORT *ptpPort;
    CONVGRP *pConvGrp;
    TPCTX  tpCtx;
    TPMSG  *ptpMsg  = &tpCtx.tpMsg;
    TPHEAD *ptpHead = &ptpMsg->head;

    /*绑定队列*/
    if ((ret = mq_bind(Q_SVR, &mqh)) != 0) {
        LOG_ERR("mq_bind() error: retcode=%d", ret);
        LOG_ERR("绑定队列[%d]出错", Q_SVR);
        goto THD_END;
    }

    /*打开数据库*/
    if (tpSQLiteOpen(&dbh) != 0) {
       LOG_ERR("打开数据库出错");
       goto THD_END;
    }

    /*设置上下文引用*/
    tpCtxSetTSD(&tpCtx);

    /*循环读队列*/
    while (1) {
        msgLen = sizeof(TPMSG);
        if ((ret = mq_pend(mqh, NULL, &q, NULL, NULL, (char *)ptpMsg, &msgLen, 0)) != 0) {
            LOG_ERR("mq_pend() error: retcode=%d", ret);
            LOG_ERR("读队列[%d]出错", Q_SVR);
            goto THD_END;
        }
        if (ptpHead->tpId) tpSetLogPrefix(ptpHead->tpId, logPrefix);
        LOG_MSG("收到来自队列[%d]的报文", q);
        if (hasLogLevel(DEBUG)) tpHeadDebug(ptpHead);
        if (hasLogLevel(DEBUG)) LOG_HEX(ptpMsg->body, ptpHead->bodyLen);

        /*检查报文长度*/
        if (msgLen != (ptpHead->bodyLen + sizeof(TPHEAD))) {
            LOG_ERR("报文体长度未设置或与实际不符");
            continue;
        }

        if (q == Q_SVR) q = ptpHead->desQ; /*虚拟端点返回*/

        /*检查端点配置*/
        if (!(ptpPort = tpPortShmGet(q, NULL))) {
            LOG_ERR("读取端点[%d]配置出错", q);
            continue;
        }

        /*检查报文格式*/
        if (ptpHead->fmtType != ptpPort->fmtDefs.fmtType) {
            LOG_ERR("报文格式未设置或与要求的不符");
            continue;
        }

        if (tpShmRetouch() != 0) {
            LOG_ERR("刷新共享内存指针出错");
            continue;
        } 

        tpCtx.msgCtx = NULL;
        tpPreBufInit(&tpCtx.preBuf, NULL);

        if (!ptpHead->tpId) { /*异步通讯端点响应*/
            /*检查目标端点*/
            if (q != ptpHead->desQ) {
                LOG_ERR("目标端点未设置或与实际不符");
                continue;
            }

            /*报文预解包*/
            if (tpPreUnpack(&tpCtx, &ptpPort->fmtDefs) != 0) {
                LOG_ERR("报文预解包出错");
                tpPreBufFree(&tpCtx.preBuf);
                continue;
            }
            LOG_MSG("报文预解包...[ok]");

            /*提取报文头*/
            if ((ret = tpHeadShmGet(ptpPort, ptpHead)) < 0) {   /*流水号重复修改 !=0 修改成 <0*/
                if (ret != SHM_NONE) {
                    LOG_ERR("提取异步通讯端点[%d]报文头出错", ptpHead->desQ);
                    tpPreBufFree(&tpCtx.preBuf);
                    continue;
                }
                /*本地共享内存中没有找到异步报文头,可能是冲正交易或非本节点机交易*/
                if ((ret = mq_post(mqh, NULL, Q_REV, 0, NULL, (char *)ptpMsg, msgLen, 0)) != 0) {
                    LOG_ERR("mq_post() error: retcode=%d", ret);
                    LOG_ERR("写事务队列[%d]出错", Q_REV);
                    LOG_ERR("转发冲正报文[%d]==>[%d]出错", ptpHead->desQ, Q_REV);
                }
                LOG_MSG("转发冲正报文[%d]==>[%d]...[ok]", ptpHead->desQ, Q_REV);
                tpPreBufFree(&tpCtx.preBuf);
                continue;
            }
            ptpHead->bodyLen = msgLen - sizeof(TPHEAD);
            tpSetLogPrefix(ptpHead->tpId, logPrefix);
            LOG_MSG("提取异步通讯端点[%d]报文头...[ok]", ptpHead->desQ);
            if (hasLogLevel(DEBUG)) tpHeadDebug(ptpHead);
            if (ret > 0) tpHeadShmRelease(ptpHead->tpId, ret);  /*xgh add 2017014:多次保存报文头需要每次释放掉*/
        } else {
            if (!ptpHead->servCode[0]) {
                /*检查请求端点*/
                if (q != ptpHead->orgQ) {
                    LOG_ERR("请求端点未设置或与实际不符");
                    continue;
                }
            } else {
                /*检查目标端点*/
                if (q != ptpHead->desQ) {
                    LOG_ERR("目标端点未设置或与实际不符");
                    continue;
                }
            }
        }
 
        /*转发冲正报文*/
        if (MSGREV == ptpHead->msgType) {
            if ((ret = mq_post(mqh, NULL, Q_REV, 0, NULL, (char *)ptpMsg, msgLen, 0)) != 0) {
                LOG_ERR("mq_post() error: retcode=%d", ret);
                LOG_ERR("写事务队列[%d]出错", Q_REV);
                LOG_ERR("转发冲正报文[%d]==>[%d]出错", ptpHead->desQ, Q_REV);
            }
            LOG_MSG("转发冲正报文[%d]==>[%d]...[ok]", ptpHead->desQ, Q_REV);
            tpPreBufFree(&tpCtx.preBuf);
            continue;
        }
	 
        /*执行处理程序*/
        ptpHead->status = tpProcess(mqh, dbh, &tpCtx);
        if (gShm.ptpConf->sysMonFlag) tpSendToSysMon(ptpHead); /*发送监控报文*/
        switch (ptpHead->status) {
            case TP_OK:
            case TP_RUNING:
            case TP_REVING:
                break;
            case TP_INVAL: /*此种报错不再返回通知*/
            case TP_ERROR: /*等待超时冲正*/
                tpTranSetFree(ptpHead->tpId, ptpHead->shmIndex);
                break;
            case TP_BUSY:
            case TP_DUPHDKEY:
                /*通知发起端*/
                while (1) {
                    /*读取端点配置*/
                    if (!(ptpPort = tpPortShmGet(ptpHead->orgQ, NULL))) {
                        LOG_ERR("读取端点[%d]配置出错", ptpHead->orgQ);
                        break;
                    }
                    if (ptpPort->convGrp <= 0) {
                        LOG_DBG("发起端[%d]未配置通知转换组", ptpHead->orgQ); 
                        break;
                    }
                    if (tpConvShmGet(ptpPort->convGrp, &pConvGrp) <= 0) {
                        LOG_DBG("发起端[%d]通知转换组[%d]不存在", ptpHead->orgQ, ptpPort->convGrp);
                        break;
                    }
                    LOG_MSG("通知发起端[%d] USING [%d]", ptpHead->orgQ, pConvGrp->grpId);

                    ptpHead->desQ = ptpHead->orgQ;
                    ptpHead->procCode[0] = 0;
                    ptpHead->msgType = MSGAPP;

                    /*执行打包转换组*/
                    tpPreBufFree(&tpCtx.preBuf);
                    if (tpMsgConvPack(pConvGrp, &ptpPort->fmtDefs, &tpCtx) != 0) {
                        LOG_ERR("执行打包转换组[%d]出错", pConvGrp->grpId);
                        break;
                    }
                    LOG_MSG("执行打包转换组[%d]...[ok]", pConvGrp->grpId);
                    if (hasLogLevel(DEBUG)) LOG_HEX(ptpMsg->body, ptpHead->bodyLen);
                    if (!ptpHead->bodyLen) break; /*忽略空报文*/

                    /*写源发队列*/
                    msgLen = ptpHead->bodyLen + sizeof(TPHEAD);
                    snprintf(clazz, sizeof(clazz), "%d", ptpHead->tpId);
                    if ((ret = mq_post(mqh, gShm.ptpConf->dispatch, ptpHead->orgQ, 0, clazz, (char *)ptpMsg, msgLen, 0)) != 0) {
                        LOG_ERR("mq_post() error: retcode=%d", ret);
                        LOG_ERR("写源发队列[%d]出错", ptpHead->orgQ);
                    }
                    LOG_MSG("写源发队列[%d]...[ok]", ptpHead->orgQ);

                    if(TP_DUPHDKEY == ptpHead->status)
                        tpTranSetFree(ptpHead->tpId, ptpHead->shmIndex); /*add by xujun 20161215*/
                    break;
                }
                break;
            default:
                LOG_MSG("???");
                break;
        } /*end switch*/

        headId = (ptpHead->shmIndex <= 0)? 0 : gShm.pShmTran[ptpHead->shmIndex-1].tpTran.tpLog.headId;
        tpPreBufFree(&tpCtx.preBuf);
        if (ptpHead->status == TP_RUNING || ptpHead->status == TP_ERROR) continue;

        /*释放上下文资源*/
        if (ptpHead->shmIndex && tpTranShmRelease(ptpHead->tpId, ptpHead->shmIndex) != 0) {
            LOG_ERR("释放上下文资源出错");
        }
        /*  xgh del  多次保存报文头需要每次释放掉 此处注释掉
        if (headId > 0) tpHeadShmRelease(ptpHead->tpId, headId);
        */
    } /*end while*/

THD_END:
    if (mqh) mq_detach(mqh);
    if (dbh) tpSQLiteClose(dbh);
    tpThreadExit();
    return NULL;
}

/***************************************************************
 * 交易处理函数
 **************************************************************/
int tpProcess(MQHDL *mqh, DBHDL *dbh, TPCTX *ptpCtx)
{
    int      i,j,ret;
    int      msgLen,tmpLen;
    char     clazz[MSGCLZLEN+1];
    char     *key,keyBuf[MAXKEYLEN+1];
    char     tmpBuf[MAXVALLEN+1];
    time_t   t;
    TPPORT   *ptpPort;
    TPMAPP   *ptpMapp;
    TPSERV   *ptpServ;
    TPPROC   *ptpProc;
    CONVGRP  *pConvGrp;
    ROUTGRP  *pRoutGrp;
    ROUTSTMT *pRoutStmt;
    TPTRAN   *ptpTran;
    TPMSG    *ptpMsg = &ptpCtx->tpMsg;
    TPHEAD   *ptpHead = &ptpMsg->head,tpHead;

    if (!ptpHead->servCode[0]) { /*新交易*/
        LOG_MSG("新交易请求...");

        /*读取端点配置*/
        if (!(ptpPort = tpPortShmGet(ptpHead->orgQ, NULL))) {
            LOG_ERR("读取端点[%d]配置出错", ptpHead->orgQ);
            return TP_INVAL; 
        }

        time(&t);
        if (t > (ptpHead->beginTime + ptpPort->timeout)) {
            LOG_ERR("收到请求时已超时");
            return TP_BUSY;
        }

        /*报文预解包*/
        if (tpPreUnpack(ptpCtx, &ptpPort->fmtDefs) != 0) {
            LOG_ERR("报文预解包出错");
            return TP_INVAL;
        }
        LOG_MSG("报文预解包...[ok]");

        /*提取交易码*/
        if ((tmpLen = tpFunExec(ptpPort->tranCodeKey, ptpHead->tranCode, MAXKEYLEN+1)) < 0) {
            LOG_ERR("提取交易码出错");
            return TP_INVAL;
        }
        LOG_MSG("提取交易码...[ok]");

        /*服务映射*/
        if (tpMappShmGet(ptpHead->orgQ, ptpHead->tranCode, &ptpMapp) <= 0) {
            LOG_ERR("服务映射[%d,%s]出错", ptpHead->orgQ, ptpHead->tranCode);
            return TP_INVAL;
        }
        LOG_MSG("服务映射: [%s] <== [%d,%s]", ptpMapp->servCode, ptpHead->orgQ, ptpHead->tranCode);

        strncpy(ptpHead->servCode, ptpMapp->servCode, MAXKEYLEN);
        ptpServ = &gShm.ptpServ[ptpMapp->servIndex-1];
        ptpHead->monFlag = ptpServ->monFlag;

        /*申请上下文空间*/
        if ((ptpHead->shmIndex = tpTranShmAlloc(ptpMapp->servIndex)) <= 0) {
            LOG_ERR("上下文空间已满");
            return TP_BUSY;
        }
        LOG_MSG("申请上下文空间[shmIndex=%d]...[ok]", ptpHead->shmIndex);

        ptpTran = &(gShm.pShmTran[ptpHead->shmIndex-1].tpTran);
        ptpCtx->msgCtx = &ptpTran->msgCtx;
        tpMsgCtxClear(ptpCtx->msgCtx);

        /*流水记录*/
        ptpTran->tpLog.tpId = ptpHead->tpId;
        ptpTran->tpLog.headId = 0;
        ptpTran->tpLog.startQ = ptpHead->startQ;    /*add by xgh 20161213*/
        ptpTran->tpLog.orgQ = ptpHead->orgQ;
        ptpTran->tpLog.desQ = 0;
        strncpy(ptpTran->tpLog.tranCode, ptpHead->tranCode, MAXKEYLEN);
        strncpy(ptpTran->tpLog.servCode, ptpHead->servCode, MAXKEYLEN);
        ptpTran->tpLog.procCode[0] = 0;
        ptpTran->tpLog.routGrp = ptpServ->routGrp;
        ptpTran->tpLog.routLine = 0;
        ptpTran->tpLog.packGrp = ptpMapp->packGrp; /*用于返回发起端时打包*/
        ptpTran->tpLog.unpkGrp = 0;                /*用于中间步骤回时解包*/
        ptpTran->tpLog.beginTime = ptpHead->beginTime;
        ptpTran->tpLog.overTime = ptpHead->beginTime + ptpPort->timeout;
        ptpTran->tpLog.servIndex = ptpMapp->servIndex;
        ptpTran->tpLog.monFlag = ptpServ->monFlag;
        ptpTran->tpLog.status = TP_RUNING;

        for (i=0; i<MAXREVNUM; i++) ptpTran->tpRevs[i].desQ = 0;

        /*执行解包转换组*/
        pConvGrp = &gShm.pConvGrp[ptpMapp->unpkGrp-1];
        if (tpMsgConvUnpack(pConvGrp, &ptpPort->fmtDefs, ptpCtx) != 0) {
            LOG_ERR("执行解包转换组[%d]出错", pConvGrp->grpId);
            return TP_ERROR;
        }
        tpPreBufFree(&ptpCtx->preBuf);
        LOG_MSG("执行解包转换组[%d]...[ok]", pConvGrp->grpId);
        if (hasLogLevel(DEBUG)) tpHashDebug(ptpCtx->msgCtx);

        /*流量检查 为了通知返回打包成功放在最后*/ 
        if (ptpServ->nMaxReq <= ptpServ->nActive+ptpServ->nReving) {
            LOG_ERR("服务[%s]请求数[%d+%d]超过限定值[%d]", ptpServ->servCode,
                     ptpServ->nActive, ptpServ->nReving, ptpServ->nMaxReq);
            return TP_BUSY;
        }

    } else { /*服务端响应报文*/
        /*检查流水记录*/
        if ((ret = tpTranSetBusy(ptpHead->tpId, ptpHead->shmIndex)) != 0) {
            if (ret != SHM_NONE) {
                LOG_ERR("???"); 
                return TP_ERROR;
            }
            LOG_WRN("流水记录[tpId=%d]不存在", ptpHead->tpId);
            
            /*迟到响应*/
            msgLen = ptpHead->bodyLen + sizeof(TPHEAD);
            if ((ret = mq_post(mqh, NULL, Q_REV, 0, NULL, (char *)ptpMsg, msgLen, 0)) != 0) {
                LOG_ERR("mq_post() error: retcode=%d", ret);
                LOG_ERR("写事务队列[%d]出错", Q_REV);
                LOG_ERR("转发迟到响应报文给事务管理程序出错");
            }
            LOG_MSG("转发迟到响应报文给事务管理程序");
            return TP_REVING;
        }
        ptpTran = &(gShm.pShmTran[ptpHead->shmIndex-1].tpTran);
        ptpCtx->msgCtx = &ptpTran->msgCtx;

        /*读取端点配置*/
        if (!(ptpPort = tpPortShmGet(ptpHead->desQ, NULL))) {
            LOG_ERR("读取端点[%d]配置出错", ptpHead->desQ);
            return TP_ERROR;
        }

        /*报文解包*/
        if (ptpPort->portType != 2) { /*非内联服务端点*/
            if (!ptpCtx->preBuf.fmtType) {
                /*报文预解包*/
                if (tpPreUnpack(ptpCtx, &ptpPort->fmtDefs) != 0) {
                    LOG_ERR("报文预解包出错");
                    return TP_ERROR;
                }
                LOG_MSG("报文预解包...[ok]");
            }

            /*执行解包转换组*/
            pConvGrp = &gShm.pConvGrp[ptpTran->tpLog.unpkGrp-1];
            if (tpMsgConvUnpack(pConvGrp, &ptpPort->fmtDefs, ptpCtx) != 0) {
                LOG_ERR("执行解包转换组[%d]出错", pConvGrp->grpId);
                return TP_ERROR;
            }
            tpPreBufFree(&ptpCtx->preBuf);
            LOG_MSG("执行解包转换组[%d]...[ok]", pConvGrp->grpId);
            if (hasLogLevel(DEBUG)) tpHashDebug(ptpCtx->msgCtx);
        }
    }

    pRoutGrp = &gShm.pRoutGrp[ptpTran->tpLog.routGrp-1];
    LOG_MSG("路由组=[%d]...", pRoutGrp->grpId);

    /*执行路由脚本*/
    for (i=ptpTran->tpLog.routLine; i<pRoutGrp->nStmts; ) {
        pRoutStmt = &gShm.pRoutStmt[i+pRoutGrp->nStart];
        switch (pRoutStmt->action) {
            case DO_NONE: /*do nothing*/
                break;

            case DO_JUMP: /*条件跳转*/
                if (!pRoutStmt->express[0]) {
                    i = pRoutStmt->jump - pRoutGrp->nStart;
                    continue;
                } else {
                    if ((tmpLen = tpFunExec(pRoutStmt->express, tmpBuf, sizeof(tmpBuf))) < 0) {
                        LOG_ERR("计算跳转条件表达式出错");
                        return TP_ERROR;
                    }
                    if (tmpBuf[0] != 0 && tmpBuf[0] != '0') {
                        i = pRoutStmt->jump - pRoutGrp->nStart; 
                        continue;
                    }
                }
                break;

            case DO_VAR: /*置临时变量*/
                if ((tmpLen = tpFunExec(pRoutStmt->express, tmpBuf, sizeof(tmpBuf))) < 0) {
                    LOG_ERR("计算临时变量值出错");
                    return TP_ERROR;
                }
                if (tpMsgCtxHashPut(ptpCtx->msgCtx, pRoutStmt->fldName, tmpBuf, tmpLen) != 0) {
                    LOG_ERR("置临时变量[%s]出错", pRoutStmt->fldName);
                    return TP_ERROR;
                }
                break;

            case DO_ALLOC: /*预分配空间(仅针对临时变量)*/
                if ((tmpLen = tpFunExec(pRoutStmt->express, tmpBuf, sizeof(tmpBuf))) < 0) {
                    LOG_ERR("计算预分配空间大小出错");
                    return TP_ERROR;
                }
                if (tpMsgCtxHashAlloc(ptpCtx->msgCtx, pRoutStmt->fldName, atoi(tmpBuf)) != 0) {
                    LOG_ERR("为临时变量[%s]预分配空间[%d]出错", pRoutStmt->fldName, atoi(tmpBuf));
                    return TP_ERROR;
                }
                break;

            case DO_SET: /*置上下文域*/
                if ((tmpLen = tpFunExec(pRoutStmt->express, tmpBuf, sizeof(tmpBuf))) < 0) {
                    LOG_ERR("计算上下文域值表达式出错");
                    return TP_ERROR;
                }
                if (!(key = tpGetCompKey(pRoutStmt->fldName, keyBuf))) {
                    LOG_ERR("计算循环域名表达式出错");
                    return TP_ERROR;
                }
                if (tpMsgCtxHashPut(ptpCtx->msgCtx, key, tmpBuf, tmpLen) != 0) {
                    LOG_ERR("置上下文域[%s]出错", key);
                    return TP_ERROR;
                }
                break;

            case DO_EXEC: /*执行脚本函数*/
                if ((tmpLen = tpFunExec(pRoutStmt->express, tmpBuf, sizeof(tmpBuf))) < 0) {
                    LOG_ERR("执行脚本函数出错");
                    return TP_ERROR;
                }
                break;

            case DO_SEND: /*发送报文*/
                ptpHead->desQ = pRoutStmt->desQ;
                pConvGrp = &gShm.pConvGrp[atoi(pRoutStmt->procCode)-1];
                LOG_MSG("SEND [%d] USING [%d]", ptpHead->desQ, pConvGrp->grpId);

                /*读取端点配置*/
                if (!(ptpPort = tpPortShmGet(ptpHead->desQ, NULL))) {
                    LOG_ERR("读取端点[%d]配置出错", ptpHead->desQ);
                    return TP_ERROR;
                }

                /*执行打包转换组*/
                if (tpMsgConvPack(pConvGrp, &ptpPort->fmtDefs, ptpCtx) != 0) {
                    LOG_ERR("执行打包转换组[%d]出错", pConvGrp->grpId);
                    return TP_ERROR;
                }
                tpPreBufFree(&ptpCtx->preBuf);
                LOG_MSG("执行打包转换组[%d]...[ok]", pConvGrp->grpId);
                if (hasLogLevel(DEBUG)) LOG_HEX(ptpMsg->body, ptpHead->bodyLen);

                /*写目标队列*/
                msgLen = ptpHead->bodyLen + sizeof(TPHEAD);
                if ((ret = mq_post(mqh, gShm.ptpConf->dispatch, ptpHead->desQ, 0, NULL, (char *)ptpMsg, msgLen, 0)) != 0) {
                    LOG_ERR("mq_post() error: retcode=%d", ret);
                    LOG_ERR("写目标队列[%d]出错", ptpHead->desQ);
                    return TP_ERROR;
                }
                LOG_MSG("写目标队列[%d]...[ok]", ptpHead->desQ);
                break;

            case DO_CALL: /*调用服务*/
                ptpHead->desQ = pRoutStmt->desQ;
                strncpy(ptpHead->procCode, pRoutStmt->procCode, MAXKEYLEN);

                /*读取端点配置*/
                if (!(ptpPort = tpPortShmGet(ptpHead->desQ, NULL))) {
                    LOG_ERR("读取端点[%d]配置出错", ptpHead->desQ);
                    return TP_ERROR;
                }

                if (ptpPort->portType == 2) { /*内联服务端点*/
                    LOG_MSG("CALL [%d] [%s]", ptpHead->desQ, pRoutStmt->procCode);
                } else {
                    ptpProc = &gShm.ptpProc[pRoutStmt->procIndex-1];
                    pConvGrp = &gShm.pConvGrp[ptpProc->packGrp-1];
                    LOG_MSG("CALL [%d] USING [%d]", ptpHead->desQ, pConvGrp->grpId);
                }

                /*如果有关联的冲正服务, 置冲正信息*/
                if (pRoutStmt->revFlag) {
                    for (j=0; j<MAXREVNUM; j++) {
                        if (!ptpTran->tpRevs[j].desQ) break;
                        if (!ptpPort->multiRev && ptpHead->desQ == ptpTran->tpRevs[j].desQ) break; /*不支持同端点重复冲正*/
                    }
                    if (j < MAXREVNUM) {
                        ptpTran->tpRevs[j].desQ = ptpHead->desQ;
                        strncpy(ptpTran->tpRevs[j].procCode, pRoutStmt->procCode, MAXKEYLEN);
                        ptpTran->tpRevs[j].packGrp = ptpProc->revPackGrp;
                        ptpTran->tpRevs[j].unpkGrp = ptpProc->revUnpkGrp;
                        ptpTran->tpRevs[j].forceRev = pRoutStmt->forceRev;
                        ptpTran->tpRevs[j].noRespRev = pRoutStmt->noRespRev;
                        LOG_MSG("WITH REVERSE USING [%d] [%d]", ptpProc->revPackGrp, ptpProc->revUnpkGrp);
                    } else { LOG_WRN("MAXREVNUM定义值[%d]过小", MAXREVNUM); }
                }

                ptpTran->tpLog.desQ = ptpHead->desQ;
                strncpy(ptpTran->tpLog.procCode, ptpHead->procCode, MAXKEYLEN);
                ptpTran->tpLog.routLine = i+1;
                ptpTran->tpLog.unpkGrp = ptpProc->unpkGrp;

                if (ptpPort->portType == 2) { /*内联服务端点*/
                    ptpHead->fmtType = 0;
                    ptpHead->bodyLen = 0;
                } else {
                    /*执行打包转换组*/
                    if (tpMsgConvPack(pConvGrp, &ptpPort->fmtDefs, ptpCtx) != 0) {
                        LOG_ERR("执行打包转换组[%d]出错", pConvGrp->grpId);
                        if (pRoutStmt->revFlag && j < MAXREVNUM) ptpTran->tpRevs[j].desQ = 0;
                        return TP_ERROR;
                    }
                    LOG_MSG("执行打包转换组[%d]...[ok]", pConvGrp->grpId);
                    if (hasLogLevel(DEBUG)) LOG_HEX(ptpMsg->body, ptpHead->bodyLen);

                    /*保存报文头*/
                    if (ptpPort->headSaveKey[0]) { /*异步通讯端点时*/
                        ptpTran->tpLog.headId = tpHeadShmPut(ptpTran->tpLog.headId, ptpPort, ptpHead);
                        if (ptpTran->tpLog.headId <= 0) {
                            LOG_ERR("保存异步通讯端点[%d]报文头出错", ptpHead->desQ);
                            if (pRoutStmt->revFlag && j < MAXREVNUM) ptpTran->tpRevs[j].desQ = 0;
                            tpPreBufFree(&ptpCtx->preBuf);
                            if(ptpTran->tpLog.headId  == SHM_DUPHDKEY) return TP_DUPHDKEY; /*add by xujun 20161215*/
                            return TP_ERROR;
                        }
                    }
                    tpPreBufFree(&ptpCtx->preBuf);
                }

                tpTranSetFree(ptpHead->tpId, ptpHead->shmIndex);

                /*写目标队列*/
                msgLen = ptpHead->bodyLen + sizeof(TPHEAD);
                if (ptpPort->portType == 1) { /*应用目标端点*/
                    ret = mq_post(mqh, gShm.ptpConf->dispatch, ptpHead->desQ, 0, NULL, (char *)ptpMsg, msgLen, 0);
                } else if (ptpPort->portType == 2) { /*内联目标端点*/
                    ret = mq_post(mqh, NULL, ptpHead->desQ, 0, NULL, (char *)ptpMsg, msgLen, 0);
                } else if (ptpPort->portType == 3) { /*虚拟目标端点*/
                    ret = mq_post(mqh, NULL, Q_SVR, 0, NULL, (char *)ptpMsg, msgLen, 0);
                } else {
                    LOG_ERR("目标端点类型不正确");
                    return TP_ERROR;
                }
                if (ret != 0) {
                    LOG_ERR("mq_post() error: retcode=%d", ret);
                    LOG_ERR("写目标队列[%d]出错", ptpHead->desQ);
                    if (pRoutStmt->revFlag && j < MAXREVNUM) ptpTran->tpRevs[j].desQ = 0;
                    return TP_ERROR;
                }
                LOG_MSG("写目标队列[%d]...[ok]", ptpHead->desQ);

                return TP_RUNING;
                break;

            case DO_REVERSE: /*冲正*/
                LOG_MSG("REVERSE...");
                ptpTran->tpLog.status = TP_REVING;

                /*清除当前步骤冲正记录(因返回不正确而无需冲正)*/
                for (j=MAXREVNUM-1; j>=0; j--) {
                    if (ptpTran->tpLog.desQ == ptpTran->tpRevs[j].desQ) break;
                }
                if (j>=0) ptpTran->tpRevs[j].desQ = 0;

                /*保存上下文*/
                if (tpTranCtxSave(ptpHead->tpId, ptpTran) != 0) {
                    LOG_ERR("保存上下文出错");
                    return TP_REVING;
                }

                /*组织报文头*/
                memset(&tpHead, 0, sizeof(TPHEAD));
                tpHead.tpId = ptpTran->tpLog.tpId;
                tpHead.startQ = ptpTran->tpLog.startQ;    /*add by xgh 20161213*/
                tpHead.orgQ = ptpTran->tpLog.orgQ;
                tpHead.desQ = ptpTran->tpLog.orgQ;
                strncpy(tpHead.tranCode, ptpTran->tpLog.tranCode, MAXKEYLEN);
                strncpy(tpHead.servCode, ptpTran->tpLog.servCode, MAXKEYLEN);
                tpHead.beginTime = ptpTran->tpLog.beginTime;
                tpHead.msgType = MSGSYS_REVERSE;
                tpHead.monFlag = ptpTran->tpLog.monFlag;
                tpHead.status = ptpTran->tpLog.status;

                /*发送冲正命令*/
                if ((ret = mq_post(mqh, NULL, Q_REV, 0, NULL, (char *)&tpHead, sizeof(TPHEAD), 0)) != 0) {
                    LOG_ERR("mq_post() error: retcode=%d", ret);
                    LOG_ERR("写事务队列[%d]出错", Q_REV);
                    LOG_ERR("向事务管理程序发送冲正命令出错");
                }
                LOG_MSG("向事务管理程序发送冲正命令...[ok]");
                return TP_REVING;
                break;

            case DO_RETURN: /*返回发起端*/
                if (strcmp(pRoutStmt->express, "NONE") == 0) return TP_OK;
                if (ptpTran->tpLog.packGrp <= 0) {
                    LOG_ERR("服务[%s]无返回打包转换组配置", ptpHead->servCode);
                    return TP_ERROR;
                }
                ptpHead->retFlag = atoi(pRoutStmt->express);
                pConvGrp = &gShm.pConvGrp[ptpTran->tpLog.packGrp-1];
                LOG_MSG("RETURN [%d](retFlag=%d) USING [%d]", ptpHead->orgQ, ptpHead->retFlag, pConvGrp->grpId);

                ptpHead->desQ = ptpHead->orgQ;
                ptpHead->procCode[0] = 0;

                /*读取端点配置*/
                if (!(ptpPort = tpPortShmGet(ptpHead->orgQ, NULL))) {
                    LOG_ERR("读取端点[%d]配置出错", ptpHead->orgQ);
                    return TP_ERROR;
                }

                /*执行打包转换组*/
                if (tpMsgConvPack(pConvGrp, &ptpPort->fmtDefs ,ptpCtx) != 0) {
                    LOG_ERR("执行打包转换组[%d]出错", pConvGrp->grpId);
                    return TP_ERROR;
                }
                tpPreBufFree(&ptpCtx->preBuf);
                LOG_MSG("执行打包转换组[%d]...[ok]", pConvGrp->grpId);
                if (hasLogLevel(DEBUG)) LOG_HEX(ptpMsg->body, ptpHead->bodyLen);

                /*写源发队列*/
                msgLen = ptpHead->bodyLen + sizeof(TPHEAD);
                snprintf(clazz, sizeof(clazz), "%d", ptpHead->tpId);
                if ((ret = mq_post(mqh, gShm.ptpConf->dispatch, ptpHead->orgQ, 0, clazz, (char *)ptpMsg, msgLen, 0)) != 0) {
                    LOG_ERR("mq_post() error: retcode=%d", ret);
                    LOG_ERR("写源发队列[%d]出错", ptpHead->orgQ);
                    return TP_ERROR;
                }
                LOG_MSG("写源发队列[%d]...[ok]", ptpHead->orgQ);
                return TP_OK;
                break;

            default:
                break;
        } /*switch end*/
        i++;
    } /* for end*/
    return TP_OK;
}

