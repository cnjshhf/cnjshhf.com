/***************************************************************
 * 事务管理程序
 **************************************************************/
#include "tpbus.h"

#define NMAX 10

void *tpProcessThread(void *arg);
void  revCntReset(char *servCode, int cnt);

/***************************************************************
 * 主函数
 **************************************************************/
int main(int argc, char *argv[])
{
    int  i,j,ret,flag,msgLen;
    char buf[256],logPrefix[21];
    time_t t;
    TPMSG tpMsg;
    TPTRAN tpTran;
    TPLOG *ptpLog;
    TPSAF tpSafs[NMAX];
    TPPORT *ptpPort;
    MQHDL *mqh;
    DBHDL *dbh;

    /*检查命令行参数*/
    if (argc != 2) {
        printf("Usage: %s <任务ID>\n", argv[0]);
        return -1;
    }

    /*初始化进程*/
    /*if (tpTaskInit(TSK_TRANMNG, "tpTranMng", NULL) != 0) { */
    if (tpTaskInit(atoi(argv[1]), "tpTranMng", NULL) != 0) { 
        LOG_ERR("初始化进程出错");
        return -1;
    }

    /*初始化队列*/
    if ((ret = mq_init()) != 0) {
        LOG_ERR("mq_init() error: retcode=%d", ret);
        return -1;
    }

    /*绑定队列*/
    if ((ret = mq_bind(Q_REV, &mqh)) != 0) {
        LOG_ERR("mq_bind() error: retcode=%d", ret);
        LOG_ERR("绑定队列[%d]出错", Q_REV);
        return -1;
    }

    /*打开数据库*/
    if (tpSQLiteOpen(&dbh) != 0) {
       LOG_ERR("打开数据库出错");
       return -1;
    }

    /*创建处理线程池*/
    for (i=0; i<gShm.ptpConf->nRevProcs; i++) {
        if (tpThreadCreate(tpProcessThread, NULL) != 0) {
            LOG_ERR("创建处理线程失败: %s", strerror(errno));
            return -1;
        }
    }

    tpShmLock(SEM_SAFTMO); /*防止多个进程同时轮询*/

    /*轮询超时SAF*/
    while (1) {
        if ((j = tpSafTimeoutFind(dbh, tpSafs, NMAX)) < 0) {
            LOG_ERR("查找超时SAF出错");
            return -1;
        }

        if (j==0) { /*无超时SAF*/
            if (tpShmRetouch() != 0) {
                LOG_ERR("刷新共享内存指针出错");
                continue;
            }
            sleep(gShm.ptpConf->ntsMonSaf);
            continue;
        }

        for (i=0; i<j; i++) {
            tpSetLogPrefix(tpSafs[i].tpId, logPrefix);
            LOG_MSG("超时SAF: safId=[%d],tpId=[%d],desQ=[%d],procCode=[%s]", tpSafs[i].safId, tpSafs[i].tpId, tpSafs[i].desQ, tpSafs[i].procCode);

            flag = 0;
            tpMsg.head.tpId  = 0;
            tpMsg.head.safId = tpSafs[i].safId;

            while (1) {
                /*检查SAF次数*/
                if (tpSafs[i].safNum <= 0) {
                    LOG_MSG("SAF次数已为0");
                    break;
                }

                /*提取SAF报文*/
                if (tpSafMsgLoad(tpSafs[i].safId, &tpMsg) != 0) {
                    LOG_ERR("提取SAF报文出错");
                    break;
                }
                LOG_MSG("提取SAF报文...[ok]");
                if (hasLogLevel(DEBUG)) tpHeadDebug(&tpMsg.head);
                if (hasLogLevel(DEBUG)) LOG_HEX(tpMsg.body, tpMsg.head.bodyLen);

                /*重置SAF次数*/
                tpSafs[i].safNum--;

                /*读取端点配置*/
                if (!(ptpPort = tpPortShmGet(tpSafs[i].desQ, NULL))) {
                    LOG_ERR("读取端点[%d]配置出错", tpSafs[i].desQ);
                    break;
                }

                /*重置SAF超时时间*/
                time(&t);
                tpSafs[i].overTime = t + ptpPort->timeout + ptpPort->safDelay;
                tpSafs[i].overTime += (ptpPort->nMaxSaf - tpSafs[i].safNum) * ptpPort->safDelay;

                /*更新SAF记录*/
                if (tpSafUpdate(dbh, &tpSafs[i]) != 0) {
                    LOG_ERR("更新SAF记录[safId=%d]出错", tpSafs[i].safId);
                    break;
                }
    		/*重发SAF报文*/
                msgLen = tpMsg.head.bodyLen + sizeof(TPHEAD);
                tpMsg.head.beginTime = t+1;
         if ((ret = mq_post(mqh, gShm.ptpConf->dispatch, tpSafs[i].desQ, 0, NULL, (char *)&tpMsg, msgLen, 0)) != 0) {
                    LOG_ERR("mq_post() error: retcode=%d", ret);
                    LOG_ERR("写队列[%d]出错", tpSafs[i].desQ);
                    LOG_ERR("重发SAF报文出错");
                    break;
                }
                LOG_MSG("重发SAF报文...[ok]  tpMsg.head.beginTime =[%ld]",  tpMsg.head.beginTime);
            
                flag = 1;
                break;
            } /*end while*/

            if (!flag) {
                LOG_MSG("冲正失败");

                /*发送监控信息*/
                if (gShm.ptpConf->sysMonFlag) {
                    tpMsg.head.status = TP_SAFFAIL;
                    tpSendToSysMon(&tpMsg.head);
                }
                
                /*清理SAF报文*/
                snprintf(buf, sizeof(buf), "rm -f %s/rtm/saf/%d.saf", getenv("TPBUS_HOME"), tpSafs[i].safId);
                system(buf);

                /*删除SAF记录*/
                if (tpSafDelete(dbh, tpSafs[i].safId) != 0) {
                    LOG_ERR("删除SAF记录[safId=%d]出错", tpSafs[i].safId);
                    usleep(1000); /*1ms*/
                }

                /*重置冲正计数器*/
                revCntReset(tpSafs[i].servCode, -1);

                /*记录冲正失败流水*/
                if (gShm.ptpConf->hisLogFlag) {
                    /*提取上下文*/
                    if (tpTranCtxLoad(tpSafs[i].tpId, &tpTran) != 0) {
                        LOG_ERR("提取上下文[tpId=%d]出错", tpSafs[i].tpId);
                    } else {
                        ptpLog = &tpTran.tpLog;
                        ptpLog->status = TP_REVFAIL;
                        if (tpLogInsert(dbh, ptpLog) != 0) {
                            LOG_ERR("记录冲正失败流水[tpId=%d]出错", ptpLog->tpId);
                        }
                    }
                }

                /*清理上下文*/
                snprintf(buf, sizeof(buf), "rm -f %s/rtm/ctx/%d.ctx", getenv("TPBUS_HOME"), tpSafs[i].tpId);
                system(buf);
				/*清理异步报文头*/
                snprintf(buf, sizeof(buf), "rm -f %s/rtm/tph/*.%d", getenv("TPBUS_HOME"), tpSafs[i].tpId);
                system(buf);
            }
        } /*end for*/
    } /*end while*/
    return 0;
}

/***************************************************************
 * 重置冲正计数器
 **************************************************************/
void  revCntReset(char *servCode, int cnt)
{
    int i;

    if (tpShmRetouch() != 0) {
        LOG_ERR("刷新共享内存指针出错");
        return;
    }
    if ((i = tpServShmGet(servCode, NULL)) <= 0) {
        LOG_ERR("无服务[%s]注册信息", servCode);
        return;
    }
	tpShmLock(SEM_SAFCNT);
    gShm.ptpServ[i-1].nReving += cnt;
    gShm.pShmHead->nReving += cnt;
    tpShmUnlock(SEM_SAFCNT);
}

/***************************************************************
 * 冲正处理线程
 **************************************************************/
void *tpProcessThread(void *arg)
{

    int i,j,q,ret,flag,msgLen,tmpLen;
    char logPrefix[21];
    char buf[256],clazz[MSGCLZLEN+1];
    char tmpBuf[MAXVALLEN+1];

    TPCTX  tpCtx;
    TPMSG  *ptpMsg = &tpCtx.tpMsg;
    TPHEAD  *ptpHead = &ptpMsg->head;

    TPPORT *ptpPort;
    CONVGRP *pConvGrp;
    TPTRAN tpTran;
    TPLOG  *ptpLog;
    TPREV  *ptpRev;
    TPSAF  tpSaf;
    time_t t;

    MQHDL *mqh;
    DBHDL *dbh;

    /*绑定队列*/
    if ((ret = mq_bind(Q_REV, &mqh)) != 0) {
        LOG_ERR("mq_bind() error: retcode=%d", ret);
        LOG_ERR("绑定队列[%d]出错", Q_REV);
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
            LOG_ERR("读队列[%d]出错", Q_REV);
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

        if (q != Q_SVR) {
            /*检查目标端点*/
            if (q != ptpHead->desQ) {
                LOG_ERR("目标端点未设置或与实际不符");
                continue;
            }

            /*检查端点配置*/
            if (!(ptpPort = tpPortShmGet(ptpHead->desQ, NULL))) {
                LOG_ERR("读取端点[%d]配置出错", ptpHead->desQ);
                continue;
            }

            /*检查报文格式*/
            if (ptpHead->fmtType != ptpPort->fmtDefs.fmtType) {
                LOG_ERR("报文格式未设置或与要求的不符");
                continue;
            }
        }

        if (tpShmRetouch() != 0) {
            LOG_ERR("刷新共享内存指针出错");
            continue;
        }

        tpCtx.msgCtx = NULL;
        tpPreBufInit(&tpCtx.preBuf, NULL);
        ptpLog = NULL;
        ptpRev = NULL;

        if (!ptpHead->tpId) { /*异步通讯端点响应*/
            /*报文预解包*/
            if (tpPreUnpack(&tpCtx, &ptpPort->fmtDefs) != 0) {
                LOG_ERR("报文预解包出错");
                tpPreBufFree(&tpCtx.preBuf);
                continue;
            }
            LOG_MSG("报文预解包...[ok]");

            /*提取报文头*/
            if ((ret = tpHeadLoad(ptpPort, ptpHead)) != 0) {
                if (ret == ENOENT) {
                    LOG_DBG("非本节点交易");
                    tpPreBufFree(&tpCtx.preBuf);
                    continue;
                }
                LOG_ERR("提取异步通讯端点[%d]报文头出错", ptpHead->desQ);
                tpPreBufFree(&tpCtx.preBuf);
                continue;
            }
            ptpHead->bodyLen = msgLen - sizeof(TPHEAD);
            tpSetLogPrefix(ptpHead->tpId, logPrefix);
            LOG_MSG("提取异步通讯端点[%d]报文头...[ok]", ptpHead->desQ);
            if (hasLogLevel(DEBUG)) tpHeadDebug(ptpHead);
        }

        LOG_MSG("dwxiong: ptpHead->msgType[%d]", ptpHead->msgType);  /*add by dwxiong test 2014-11-8 15:16:34*/
        switch (ptpHead->msgType) {
            case MSGSYS_TIMEOUT: /*超时冲正*/
            case MSGSYS_REVERSE: /*主动冲正*/
                /*提取上下文*/
                if (tpTranCtxLoad(ptpHead->tpId, &tpTran) != 0) {
                    LOG_ERR("提取上下文[tpId=%d]出错", ptpHead->tpId);

                    /*清理上下文*/
                    snprintf(buf, sizeof(buf), "rm -f %s/rtm/ctx/%d.ctx", getenv("TPBUS_HOME"), ptpHead->tpId);
                    system(buf);
                    /*清理异步报文头*/
                    snprintf(buf, sizeof(buf), "rm -f %s/rtm/tph/*.%d", getenv("TPBUS_HOME"), ptpHead->tpId);
                    system(buf);
                    continue;
                }
                ptpLog = &tpTran.tpLog;
                ptpRev = tpTran.tpRevs;
                tpCtx.msgCtx = &tpTran.msgCtx;

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

                    /*执行打包转换组*/
                    if (tpMsgConvPack(pConvGrp, &ptpPort->fmtDefs, &tpCtx) != 0) {
                        LOG_ERR("执行打包转换组[%d]出错", pConvGrp->grpId);
                        break;
                    }
                    tpPreBufFree(&tpCtx.preBuf);
                    LOG_MSG("执行打包转换组[%d]...[ok]", pConvGrp->grpId);
                    if (hasLogLevel(DEBUG)) LOG_HEX(ptpMsg->body, ptpHead->bodyLen);
                    if (!ptpHead->bodyLen) break; /*忽略空报文*/

                    /*写源发队列*/
                    ptpHead->msgType = MSGAPP;
                    msgLen = ptpHead->bodyLen + sizeof(TPHEAD);
                    snprintf(clazz, sizeof(clazz), "%d", ptpHead->tpId);
                    if ((ret = mq_post(mqh, gShm.ptpConf->dispatch, ptpHead->orgQ, 0, clazz, (char *)ptpMsg, msgLen, 0)) != 0) {
                        LOG_ERR("mq_post() error: retcode=%d", ret);
                        LOG_ERR("写源发队列[%d]出错", ptpHead->orgQ);
                    }
                    LOG_MSG("写源发队列[%d]...[ok]", ptpHead->orgQ);
                    break;
                }
                flag = 1;
                break;

            case MSGREV: /*冲正响应*/
                flag = 0;
                while (1) {
                    /*提取上下文*/
                    if (tpTranCtxLoad(ptpHead->tpId, &tpTran) != 0) {
                        LOG_ERR("提取上下文[tpId=%d]出错", ptpHead->tpId);
                        break;
                    }
                    ptpLog = &tpTran.tpLog;
                    ptpRev = tpTran.tpRevs;
                    tpCtx.msgCtx = &tpTran.msgCtx;

                    /*查找冲正步骤记录*/
                    for (j=MAXREVNUM-1; j>=0; j--) {
                        if (ptpRev[j].desQ == ptpHead->desQ) break;
                    }
                    if (j<0) {
                        LOG_ERR("没有找到目标端点冲正记录");
                        break;
                    }
                    LOG_MSG("dwxiong: ptpPort->respCodeKey[%s]", ptpPort->respCodeKey);  /*add by dwxiong test 2014-11-8 15:16:34*/
                    /*检查冲正结果是否正确*/
                    if (!ptpPort->respCodeKey[0]) {
                        flag = 1;
                    } else {
                        if (ptpPort->portType != 2) { /*非内联服务端点*/
                            /*读取端点配置*/
                            if (!(ptpPort = tpPortShmGet(ptpHead->desQ, NULL))) {
                                LOG_ERR("读取端点[%d]配置出错", ptpHead->desQ);
                                break;
                            }

                            if (!tpCtx.preBuf.fmtType) {
                                /*报文预解包*/
                                if (tpPreUnpack(&tpCtx, &ptpPort->fmtDefs) != 0) {
                                    LOG_ERR("报文预解包出错");
                                    break;
                                }
                            }

                            /*报文解包*/
                            if (tpConvShmGet(ptpRev[j].unpkGrp, &pConvGrp) <= 0) {
                                LOG_ERR("冲正解包转换组[%d]不存在", ptpRev[j].unpkGrp);
                                break;
                            }

                            if (tpMsgConvUnpack(pConvGrp, &ptpPort->fmtDefs, &tpCtx) != 0) {
                                LOG_ERR("执行解包转换组[%d]出错", pConvGrp->grpId);
                                break;
                            }
                            tpPreBufFree(&tpCtx.preBuf);
                        }
                        
                        if ((tmpLen = tpFunExec(ptpPort->respCodeKey, tmpBuf, sizeof(tmpBuf))) < 0) {
                            LOG_ERR("计算冲正结果表达式出错");
                            break;
                        }
                        if (tmpBuf[0] != 0 && tmpBuf[0] != '0') flag = 1;
                        LOG_MSG("dwxiong: 计算冲正结果表达式[%X], flag=[%d]", tmpBuf[0],flag);  /*add by dwxiong test 2014-11-8 15:16:34*/
                        tpFunExec("CTX(rspcode_39)", tmpBuf, sizeof(tmpBuf));
                        LOG_MSG("dwxiong: rspcode_39[%s]", tmpBuf);  /*add by dwxiong test 2014-11-8 15:16:34*/
                        tpFunExec("CTX(orgdata_90_rev)", tmpBuf, sizeof(tmpBuf));
                        LOG_MSG("dwxiong: orgdata_90_rev[%s]", tmpBuf);  /*add by dwxiong test 2014-11-8 15:16:34*/
                        tpFunExec("CTX(cupdaddrsp_44)", tmpBuf, sizeof(tmpBuf));
                        LOG_MSG("dwxiong: cupdaddrsp_44[%s]", tmpBuf);  /*add by dwxiong test 2014-11-8 15:16:34*/
                        tpFunExec("CTX(cardcode_42)", tmpBuf, sizeof(tmpBuf));
                        LOG_MSG("dwxiong: cardcode_42[%s]", tmpBuf);  /*add by dwxiong test 2014-11-8 15:16:34*/
                        tpFunExec("CTX(finstcode_33)", tmpBuf, sizeof(tmpBuf));
                        LOG_MSG("dwxiong: finstcode_33[%s]", tmpBuf);  /*add by dwxiong test 2014-11-8 15:16:34*/
                        tpFunExec("CTX(ainstcode_32)", tmpBuf, sizeof(tmpBuf));
                        LOG_MSG("dwxiong: ainstcode_32[%s]", tmpBuf);  /*add by dwxiong test 2014-11-8 15:16:34*/
                        tpFunExec("CTX(systraceno_11)", tmpBuf, sizeof(tmpBuf));
                        LOG_MSG("dwxiong: systraceno_11[%s]", tmpBuf);  /*add by dwxiong test 2014-11-8 15:16:34*/
                        tpFunExec("CTX(trantime_7_rev)", tmpBuf, sizeof(tmpBuf));
                        LOG_MSG("dwxiong: trantime_7_rev[%s]", tmpBuf);  /*add by dwxiong test 2014-11-8 15:16:34*/
                        tpFunExec("CTX(TRXCODE_rev)", tmpBuf, sizeof(tmpBuf));
                        LOG_MSG("dwxiong: TRXCODE_rev[%s]", tmpBuf);  /*add by dwxiong test 2014-11-8 15:16:34*/
                        
                    }
                    ptpRev[j].desQ = 0;
                    break;
                } /*end while*/

                tpPreBufFree(&tpCtx.preBuf);

                /*清理SAF报文*/
                snprintf(buf, sizeof(buf), "rm -f %s/rtm/saf/%d.saf", getenv("TPBUS_HOME"), ptpHead->safId);
                system(buf);

                /*删除SAF记录*/
                if (tpSafDelete(dbh, ptpHead->safId) != 0) {
                    LOG_ERR("删除SAF记录[safId=%d]出错", ptpHead->safId);
                    usleep(1000); /*1ms*/
                }

                /*重置冲正计数器*/
                revCntReset(ptpLog->servCode, -1);

                if (!flag) { /*冲正失败*/
                    /*发送监控信息*/
                    if (gShm.ptpConf->sysMonFlag) {
                        ptpHead->status = TP_REVFAIL;
                        tpSendToSysMon(ptpHead);
                    }

                    /*清理上下文*/
                    snprintf(buf, sizeof(buf), "rm -f %s/rtm/ctx/%d.ctx", getenv("TPBUS_HOME"), ptpHead->tpId);
                    system(buf);
                    /*清理异步报文头*/
                    snprintf(buf, sizeof(buf), "rm -f %s/rtm/tph/*.%d", getenv("TPBUS_HOME"), ptpHead->tpId);
                    system(buf);

                    /*记录冲正失败流水*/
                    if (gShm.ptpConf->hisLogFlag && ptpLog != NULL) {
                        ptpLog->status = TP_REVFAIL;
                        if (tpLogInsert(dbh, ptpLog) != 0) {
                            LOG_ERR("记录冲正失败流水[tpId=%d]出错", ptpLog->tpId);
                        }
                    }
                }
                break;

            default:
                flag = 0;
                break;
        } /*end switch*/

        if (!flag) continue;
        
        /*第1次循环处理通知类冲正*/
        /*第2次循环处理响应类冲正*/
        for (i=0,flag=0; i<2; i++) {
            for (j=MAXREVNUM-1; j>=0; j--) {
                if (!ptpRev[j].desQ) continue;
                if (i==0 && !ptpRev[j].noRespRev) continue;

                memset(ptpHead, 0, sizeof(TPHEAD));

                /*申请SAF流水号*/
                if (!ptpRev[j].noRespRev) {
                    if ((ptpHead->safId = tpNewSafId()) <= 0) {
                        LOG_ERR("申请SAF流水号失败");
                        break;
                    }
                }

                /*准备报文头*/
                ptpHead->tpId = ptpLog->tpId;
                ptpHead->orgQ = ptpLog->orgQ;
                ptpHead->desQ = ptpRev[j].desQ;
                strcpy(ptpHead->tranCode, ptpLog->tranCode);
                strcpy(ptpHead->servCode, ptpLog->servCode);
                strcpy(ptpHead->procCode, ptpRev[j].procCode);
                ptpHead->beginTime = ptpLog->beginTime;
                ptpHead->msgType = MSGREV;
                ptpHead->monFlag = ptpLog->monFlag;
                ptpHead->status = TP_REVING;

                /*读取端点配置*/
                if (!(ptpPort = tpPortShmGet(ptpHead->desQ, NULL))) {
                    LOG_ERR("读取端点[%d]配置出错", ptpHead->desQ);
                    if (ptpRev[j].noRespRev) {
                        ptpRev[j].desQ = 0;
                        continue;
                    }
                    break;
                }

                if (ptpPort->portType == 2) { /*内联服务端点*/
                    ptpHead->fmtType = 0;
                    ptpHead->bodyLen = 0;
                } else {
                    /*报文打包*/
                    if (tpConvShmGet(ptpRev[j].packGrp, &pConvGrp) <= 0) {
                        LOG_ERR("冲正打包转换组[%d]不存在", ptpRev[j].packGrp);
                        if (ptpRev[j].noRespRev) {
                            ptpRev[j].desQ = 0;		
                            continue;
                        }
                        break;
                    }
                    if (tpMsgConvPack(pConvGrp, &ptpPort->fmtDefs, &tpCtx) != 0) {
                        LOG_ERR("执行打包转换组[%d]出错", pConvGrp->grpId);
                        if (ptpRev[j].noRespRev) {
                            ptpRev[j].desQ = 0;		
                            continue;
                        }
                        break;
                    }
                    if (hasLogLevel(DEBUG)) tpHeadDebug(ptpHead);
                    if (hasLogLevel(DEBUG)) LOG_HEX(ptpMsg->body, ptpHead->bodyLen);
                }

                if (!ptpRev[j].noRespRev) {
                    /*保存SAF报文*/
                    if (tpSafMsgSave(ptpHead->safId, ptpMsg) != 0) {
                        LOG_ERR("保存SAF报文出错");
                        break;
                    }
          
                    /*保存上下文*/
                    if (tpTranCtxSave(ptpHead->tpId, &tpTran) != 0){
                        LOG_ERR("保存上下文出错");
                        break;
                    }

                    /*保存SAF记录*/
                    tpSaf.safId = ptpHead->safId;
                    tpSaf.tpId = ptpHead->tpId;
                    tpSaf.desQ = ptpHead->desQ;
                    strcpy(tpSaf.servCode, ptpHead->servCode);
                    strcpy(tpSaf.procCode, ptpHead->procCode);
                    tpSaf.safNum = (ptpPort->nMaxSaf <=0)? 0 : (ptpPort->nMaxSaf-1);
                    time(&t);
                    tpSaf.overTime = t + ptpPort->timeout + ptpPort->safDelay;
                    if (tpSafInsert(dbh, &tpSaf) != 0) {
                        LOG_ERR("保存SAF记录出错");
                        break;
                    }
                    
                    /*重置冲正计数器*/
                    revCntReset(ptpLog->servCode, 1);
                }

                if (!ptpRev[j].noRespRev) {
                    /*保存报文头*/
                    if (ptpPort->headSaveKey[0]) { /*异步通讯端点时*/
                        if (tpHeadSave(ptpPort, ptpHead) != 0) {
                            LOG_ERR("保存异步通讯端点[%d]报文头出错", ptpHead->desQ);
                            break;
                        }
                    }
                }
                tpPreBufFree(&tpCtx.preBuf);

                /*写目标队列*/
                msgLen = ptpHead->bodyLen + sizeof(TPHEAD);
                if ((ret = mq_post(mqh, gShm.ptpConf->dispatch, ptpHead->desQ, 0, NULL, (char *)ptpMsg, msgLen, 0)) != 0) {
                    LOG_ERR("mq_post() error: retcode=%d", ret);
                    LOG_ERR("写目标队列[%d]出错", ptpHead->desQ);
                    if (ptpRev[j].noRespRev) {
                        ptpRev[j].desQ = 0;
                        continue;
                    }
                    break;
                }
                if (!ptpRev[j].noRespRev) {
                    flag = 1;
                    break;
                }
                ptpRev[j].desQ = 0;
            } /*end for*/
            tpPreBufFree(&tpCtx.preBuf);
        } /*end for*/
        
        if (flag) continue; /*等待冲正响应*/

        if (ptpHead->safId) {
            /*清理SAF报文*/
            snprintf(buf, sizeof(buf), "rm -f %s/rtm/saf/%d.saf", getenv("TPBUS_HOME"), ptpHead->safId);
            system(buf);

            /*删除SAF记录*/
            if (tpSafDelete(dbh, ptpHead->safId) != 0) {
                LOG_ERR("删除SAF记录[safId=%d]出错", ptpHead->safId);
                usleep(1000); /*1ms*/
            }

            /*重置冲正计数器*/
            revCntReset(ptpLog->servCode, -1);
        }

        /*发送监控信息*/
        if (gShm.ptpConf->sysMonFlag) {
            ptpHead->status = (j<0)? TP_REVOK : TP_REVFAIL;
            tpSendToSysMon(ptpHead);
        }

        /*清理上下文*/
        snprintf(buf, sizeof(buf), "rm -f %s/rtm/ctx/%d.ctx", getenv("TPBUS_HOME"), ptpLog->tpId);
        system(buf);
        /*清理异步报文头*/
        snprintf(buf, sizeof(buf), "rm -f %s/rtm/tph/*.%d", getenv("TPBUS_HOME"), ptpLog->tpId);
        system(buf);

        /*记录冲正失败流水*/
        if (j>=0) {
            if (gShm.ptpConf->hisLogFlag && ptpLog != NULL) {
                ptpLog->status = TP_REVFAIL;
                if (tpLogInsert(dbh, ptpLog) != 0) {
                    LOG_ERR("记录冲正失败流水[tpId=%d]出错", ptpLog->tpId);
                }
            }
        }
    }

THD_END:
    if (mqh) mq_detach(mqh);
    if (dbh) tpSQLiteClose(dbh);
    tpThreadExit();
    return NULL;
}

