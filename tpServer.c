/***************************************************************
 * ���ص��ȳ���
 **************************************************************/
#include "tpbus.h"

#define NMAX  10

void *tpProcessThread(void *arg);
int tpProcess(MQHDL *mqh, DBHDL *dbh, TPCTX *ptpCtx);

/***************************************************************
 * ������
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

    /*�����Ȩ���*/
    if (tpLicence() != 0) {
      printf("����Ȩ��ɻ��ѹ���\n");
      return -1;
    }

    /*��������в���*/
    if (argc != 2) {
        printf("Usage: %s <����ID>\n", argv[0]);
        return -1;
    }

    /*��ʼ������*/
    /*if (tpTaskInit(TSK_SERVER, "tpServer", &tpTask) != 0) { */
    if (tpTaskInit(atoi(argv[1]), "tpServer", &tpTask) != 0) { 
        LOG_ERR("��ʼ�����̳���");
        return -1;
    }
    
    /*��ʼ�����ò���*/
    if (tpGetParamInt(tpTask.params, "desQ", &desQ) != 0) desQ = -1;
    if (tpGetParamInt(tpTask.params, "convId", &convId) != 0) convId = -1; 
    LOG_MSG("����[desQ]=[%d]", desQ);
    LOG_MSG("��ʱ���Ӵ����Ĵ����=[%d]",convId);

    /*��ʼ������*/
    if ((ret = mq_init()) != 0) {
        LOG_ERR("mq_init() error: retcode=%d", ret);
        LOG_ERR("��ʼ�����г���");
        return -1;
    }

    /*�󶨶���*/
    if ((ret = mq_bind(Q_SVR, &mqh)) != 0) {
        LOG_ERR("mq_bind() error: retcode=%d", ret);
        LOG_ERR("�󶨶���[%d]����", Q_SVR);
        return -1;
    }

    /*�����ݿ�*/
    if (tpSQLiteOpen(&dbh) != 0) {
       LOG_ERR("�����ݿ����");
       return -1;
    }

    /*���������̳߳�*/
    for (i=0; i<gShm.ptpConf->nMaxProcs; i++) {
        if (tpThreadCreate(tpProcessThread, NULL) != 0) {
            LOG_ERR("���������߳�ʧ��: %s", strerror(errno));
            return -1;
        }
    }

    tpShmLock(SEM_TPTMO);  /*��ֹ�������ͬʱ��ѯ*/   

    /*��ѯ��ʱ����*/
    while (1) {
        /*�ͷų�ʱ����ͷ bgn --add by xgh 20170216*/
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
          LOG_MSG("�ͷų�ʱ����ͷ��¼��id=[%d] qid=[%d] key=[%s]",i,gShm.pShmTphd[i-1].desQ,gShm.pShmTphd[i-1].key);
          tpShmLock(SEM_TPHD);
          i = gShm.pShmTphd[i-1].next;
        }
        tpShmUnlock(SEM_TPHD);
        /*�ͷų�ʱ����ͷ end --add by xgh 20170216*/

        if ((j = tpShmTimeoutFind(tpTranArray, NMAX)) < 0) {
            LOG_ERR("���ҳ�ʱ���׳���");
            return -1;
        }

        if (j==0) { /*�޳�ʱ����*/
            if (tpShmRetouch() != 0) {
                LOG_ERR("ˢ�¹����ڴ�ָ�����");
                continue;
            }
            sleep(gShm.ptpConf->ntsMonLog);
            continue;
        }

        for (i=0; i<j; i++) {
            ptpLog = &tpTranArray[i].tpLog;
            ptpRev = tpTranArray[i].tpRevs;
            ptpLog->status = TP_TIMEOUT;
            LOG_MSG("��ʱ����: tpId=[%d],orgQ=[%d],tranCode=[%s],servCode=[%s],desQ=[%d],procCode=[%s]", ptpLog->tpId, ptpLog->orgQ, ptpLog->tranCode, ptpLog->servCode, ptpLog->desQ, ptpLog->procCode);

            /*DUMP�첽����ͷ*/
            if (ptpLog->headId) {
                if (gShm.pShmTphd[ptpLog->headId-1].tag &&
                    gShm.pShmTphd[ptpLog->headId-1].desQ == ptpLog->desQ &&
                    gShm.pShmTphd[ptpLog->headId-1].tpHead.tpId == ptpLog->tpId) {

                    if (tpHeadDump(gShm.pShmTphd[ptpLog->headId-1].desQ, 
						       gShm.pShmTphd[ptpLog->headId-1].key,
						       &gShm.pShmTphd[ptpLog->headId-1].tpHead) != 0) {
                        LOG_ERR("DUMP�첽����ͷ����");
                        tpHeadShmRelease(ptpLog->tpId, ptpLog->headId);
                        continue;
                    }
                }
                tpHeadShmRelease(ptpLog->tpId, ptpLog->headId);
            }

            if (!(ptpPort = tpPortShmGet(ptpLog->orgQ, NULL))) {
                LOG_ERR("��ȡ�˵�[%d]���ó���", ptpLog->orgQ);
                continue;
            }
            for (k=MAXREVNUM-1; k>=0; k--) { if (ptpRev[k].desQ == ptpLog->desQ) break; }
            if (k>=0 && !ptpRev[k].forceRev) ptpRev[k].desQ = 0;
            if (!ptpPort->convGrp && !ptpRev[0].desQ) continue;

            /*����������*/
            if (tpTranCtxSave(ptpLog->tpId, &tpTranArray[i]) != 0) {
                LOG_ERR("����������[tpId=%d]����", ptpLog->tpId);
                continue;
            }

            /*��֯����ͷ*/
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

            if (gShm.ptpConf->sysMonFlag) tpSendToSysMon(&tpMsg.head); /*���ͼ�ر���*/

            /*������ʱ����*/
            tpMsg.head.bodyLen = 0;
            if ((ret = mq_post(mqh, NULL, Q_REV, 0, NULL, (char *)&tpMsg.head, sizeof(TPHEAD), 0)) != 0) {
                LOG_ERR("mq_post() error: retcode=%d", ret);
                LOG_ERR("д�������[%d]����", Q_REV);
                LOG_ERR("�������������ͳ����������");
                continue;
            }
            LOG_MSG("�������������ͳ�������...[ok]");

            if ( (desQ > 0) && (convId > 0) ) {
                /*��ȡ�˵�����*/
                if (!(ptpPort = tpPortShmGet(desQ, NULL))) {
                    LOG_ERR("��ȡ�˵�[%d]���ó���", desQ);
                    continue;
                }
                if (tpConvShmGet(convId, &pConvGrp) <= 0) {
                    LOG_DBG("ת����[%d]������", convId);
                    continue;
                }
                /*ִ�д��ת����*/
                 tpPreBufFree(&tpCtx.preBuf);
                if (tpMsgConvPack(pConvGrp, &ptpPort->fmtDefs, &tpCtx) != 0) {
                    LOG_ERR("ִ�д��ת����[%d]����", pConvGrp->grpId);
                    continue;
                }
                LOG_MSG("ִ�д��ת����[%d]...[ok]", pConvGrp->grpId);
                if (hasLogLevel(DEBUG)) LOG_HEX(ptpMsg->body, ptpHead->bodyLen);
    
                if ((ret = mq_post(mqh, NULL, desQ, 0, NULL, ptpMsg->body, ptpHead->bodyLen, 0)) != 0) {
                    LOG_ERR("д���Ӵ������[%d]����", desQ);
                    continue;
                }
            }
        }
    }
    return 0;
}

/***************************************************************
 * ���״����߳�
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

    /*�󶨶���*/
    if ((ret = mq_bind(Q_SVR, &mqh)) != 0) {
        LOG_ERR("mq_bind() error: retcode=%d", ret);
        LOG_ERR("�󶨶���[%d]����", Q_SVR);
        goto THD_END;
    }

    /*�����ݿ�*/
    if (tpSQLiteOpen(&dbh) != 0) {
       LOG_ERR("�����ݿ����");
       goto THD_END;
    }

    /*��������������*/
    tpCtxSetTSD(&tpCtx);

    /*ѭ��������*/
    while (1) {
        msgLen = sizeof(TPMSG);
        if ((ret = mq_pend(mqh, NULL, &q, NULL, NULL, (char *)ptpMsg, &msgLen, 0)) != 0) {
            LOG_ERR("mq_pend() error: retcode=%d", ret);
            LOG_ERR("������[%d]����", Q_SVR);
            goto THD_END;
        }
        if (ptpHead->tpId) tpSetLogPrefix(ptpHead->tpId, logPrefix);
        LOG_MSG("�յ����Զ���[%d]�ı���", q);
        if (hasLogLevel(DEBUG)) tpHeadDebug(ptpHead);
        if (hasLogLevel(DEBUG)) LOG_HEX(ptpMsg->body, ptpHead->bodyLen);

        /*��鱨�ĳ���*/
        if (msgLen != (ptpHead->bodyLen + sizeof(TPHEAD))) {
            LOG_ERR("�����峤��δ���û���ʵ�ʲ���");
            continue;
        }

        if (q == Q_SVR) q = ptpHead->desQ; /*����˵㷵��*/

        /*���˵�����*/
        if (!(ptpPort = tpPortShmGet(q, NULL))) {
            LOG_ERR("��ȡ�˵�[%d]���ó���", q);
            continue;
        }

        /*��鱨�ĸ�ʽ*/
        if (ptpHead->fmtType != ptpPort->fmtDefs.fmtType) {
            LOG_ERR("���ĸ�ʽδ���û���Ҫ��Ĳ���");
            continue;
        }

        if (tpShmRetouch() != 0) {
            LOG_ERR("ˢ�¹����ڴ�ָ�����");
            continue;
        } 

        tpCtx.msgCtx = NULL;
        tpPreBufInit(&tpCtx.preBuf, NULL);

        if (!ptpHead->tpId) { /*�첽ͨѶ�˵���Ӧ*/
            /*���Ŀ��˵�*/
            if (q != ptpHead->desQ) {
                LOG_ERR("Ŀ��˵�δ���û���ʵ�ʲ���");
                continue;
            }

            /*����Ԥ���*/
            if (tpPreUnpack(&tpCtx, &ptpPort->fmtDefs) != 0) {
                LOG_ERR("����Ԥ�������");
                tpPreBufFree(&tpCtx.preBuf);
                continue;
            }
            LOG_MSG("����Ԥ���...[ok]");

            /*��ȡ����ͷ*/
            if ((ret = tpHeadShmGet(ptpPort, ptpHead)) < 0) {   /*��ˮ���ظ��޸� !=0 �޸ĳ� <0*/
                if (ret != SHM_NONE) {
                    LOG_ERR("��ȡ�첽ͨѶ�˵�[%d]����ͷ����", ptpHead->desQ);
                    tpPreBufFree(&tpCtx.preBuf);
                    continue;
                }
                /*���ع����ڴ���û���ҵ��첽����ͷ,�����ǳ������׻�Ǳ��ڵ������*/
                if ((ret = mq_post(mqh, NULL, Q_REV, 0, NULL, (char *)ptpMsg, msgLen, 0)) != 0) {
                    LOG_ERR("mq_post() error: retcode=%d", ret);
                    LOG_ERR("д�������[%d]����", Q_REV);
                    LOG_ERR("ת����������[%d]==>[%d]����", ptpHead->desQ, Q_REV);
                }
                LOG_MSG("ת����������[%d]==>[%d]...[ok]", ptpHead->desQ, Q_REV);
                tpPreBufFree(&tpCtx.preBuf);
                continue;
            }
            ptpHead->bodyLen = msgLen - sizeof(TPHEAD);
            tpSetLogPrefix(ptpHead->tpId, logPrefix);
            LOG_MSG("��ȡ�첽ͨѶ�˵�[%d]����ͷ...[ok]", ptpHead->desQ);
            if (hasLogLevel(DEBUG)) tpHeadDebug(ptpHead);
            if (ret > 0) tpHeadShmRelease(ptpHead->tpId, ret);  /*xgh add 2017014:��α��汨��ͷ��Ҫÿ���ͷŵ�*/
        } else {
            if (!ptpHead->servCode[0]) {
                /*�������˵�*/
                if (q != ptpHead->orgQ) {
                    LOG_ERR("����˵�δ���û���ʵ�ʲ���");
                    continue;
                }
            } else {
                /*���Ŀ��˵�*/
                if (q != ptpHead->desQ) {
                    LOG_ERR("Ŀ��˵�δ���û���ʵ�ʲ���");
                    continue;
                }
            }
        }
 
        /*ת����������*/
        if (MSGREV == ptpHead->msgType) {
            if ((ret = mq_post(mqh, NULL, Q_REV, 0, NULL, (char *)ptpMsg, msgLen, 0)) != 0) {
                LOG_ERR("mq_post() error: retcode=%d", ret);
                LOG_ERR("д�������[%d]����", Q_REV);
                LOG_ERR("ת����������[%d]==>[%d]����", ptpHead->desQ, Q_REV);
            }
            LOG_MSG("ת����������[%d]==>[%d]...[ok]", ptpHead->desQ, Q_REV);
            tpPreBufFree(&tpCtx.preBuf);
            continue;
        }
	 
        /*ִ�д������*/
        ptpHead->status = tpProcess(mqh, dbh, &tpCtx);
        if (gShm.ptpConf->sysMonFlag) tpSendToSysMon(ptpHead); /*���ͼ�ر���*/
        switch (ptpHead->status) {
            case TP_OK:
            case TP_RUNING:
            case TP_REVING:
                break;
            case TP_INVAL: /*���ֱ����ٷ���֪ͨ*/
            case TP_ERROR: /*�ȴ���ʱ����*/
                tpTranSetFree(ptpHead->tpId, ptpHead->shmIndex);
                break;
            case TP_BUSY:
            case TP_DUPHDKEY:
                /*֪ͨ�����*/
                while (1) {
                    /*��ȡ�˵�����*/
                    if (!(ptpPort = tpPortShmGet(ptpHead->orgQ, NULL))) {
                        LOG_ERR("��ȡ�˵�[%d]���ó���", ptpHead->orgQ);
                        break;
                    }
                    if (ptpPort->convGrp <= 0) {
                        LOG_DBG("�����[%d]δ����֪ͨת����", ptpHead->orgQ); 
                        break;
                    }
                    if (tpConvShmGet(ptpPort->convGrp, &pConvGrp) <= 0) {
                        LOG_DBG("�����[%d]֪ͨת����[%d]������", ptpHead->orgQ, ptpPort->convGrp);
                        break;
                    }
                    LOG_MSG("֪ͨ�����[%d] USING [%d]", ptpHead->orgQ, pConvGrp->grpId);

                    ptpHead->desQ = ptpHead->orgQ;
                    ptpHead->procCode[0] = 0;
                    ptpHead->msgType = MSGAPP;

                    /*ִ�д��ת����*/
                    tpPreBufFree(&tpCtx.preBuf);
                    if (tpMsgConvPack(pConvGrp, &ptpPort->fmtDefs, &tpCtx) != 0) {
                        LOG_ERR("ִ�д��ת����[%d]����", pConvGrp->grpId);
                        break;
                    }
                    LOG_MSG("ִ�д��ת����[%d]...[ok]", pConvGrp->grpId);
                    if (hasLogLevel(DEBUG)) LOG_HEX(ptpMsg->body, ptpHead->bodyLen);
                    if (!ptpHead->bodyLen) break; /*���Կձ���*/

                    /*дԴ������*/
                    msgLen = ptpHead->bodyLen + sizeof(TPHEAD);
                    snprintf(clazz, sizeof(clazz), "%d", ptpHead->tpId);
                    if ((ret = mq_post(mqh, gShm.ptpConf->dispatch, ptpHead->orgQ, 0, clazz, (char *)ptpMsg, msgLen, 0)) != 0) {
                        LOG_ERR("mq_post() error: retcode=%d", ret);
                        LOG_ERR("дԴ������[%d]����", ptpHead->orgQ);
                    }
                    LOG_MSG("дԴ������[%d]...[ok]", ptpHead->orgQ);

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

        /*�ͷ���������Դ*/
        if (ptpHead->shmIndex && tpTranShmRelease(ptpHead->tpId, ptpHead->shmIndex) != 0) {
            LOG_ERR("�ͷ���������Դ����");
        }
        /*  xgh del  ��α��汨��ͷ��Ҫÿ���ͷŵ� �˴�ע�͵�
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
 * ���״�����
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

    if (!ptpHead->servCode[0]) { /*�½���*/
        LOG_MSG("�½�������...");

        /*��ȡ�˵�����*/
        if (!(ptpPort = tpPortShmGet(ptpHead->orgQ, NULL))) {
            LOG_ERR("��ȡ�˵�[%d]���ó���", ptpHead->orgQ);
            return TP_INVAL; 
        }

        time(&t);
        if (t > (ptpHead->beginTime + ptpPort->timeout)) {
            LOG_ERR("�յ�����ʱ�ѳ�ʱ");
            return TP_BUSY;
        }

        /*����Ԥ���*/
        if (tpPreUnpack(ptpCtx, &ptpPort->fmtDefs) != 0) {
            LOG_ERR("����Ԥ�������");
            return TP_INVAL;
        }
        LOG_MSG("����Ԥ���...[ok]");

        /*��ȡ������*/
        if ((tmpLen = tpFunExec(ptpPort->tranCodeKey, ptpHead->tranCode, MAXKEYLEN+1)) < 0) {
            LOG_ERR("��ȡ���������");
            return TP_INVAL;
        }
        LOG_MSG("��ȡ������...[ok]");

        /*����ӳ��*/
        if (tpMappShmGet(ptpHead->orgQ, ptpHead->tranCode, &ptpMapp) <= 0) {
            LOG_ERR("����ӳ��[%d,%s]����", ptpHead->orgQ, ptpHead->tranCode);
            return TP_INVAL;
        }
        LOG_MSG("����ӳ��: [%s] <== [%d,%s]", ptpMapp->servCode, ptpHead->orgQ, ptpHead->tranCode);

        strncpy(ptpHead->servCode, ptpMapp->servCode, MAXKEYLEN);
        ptpServ = &gShm.ptpServ[ptpMapp->servIndex-1];
        ptpHead->monFlag = ptpServ->monFlag;

        /*���������Ŀռ�*/
        if ((ptpHead->shmIndex = tpTranShmAlloc(ptpMapp->servIndex)) <= 0) {
            LOG_ERR("�����Ŀռ�����");
            return TP_BUSY;
        }
        LOG_MSG("���������Ŀռ�[shmIndex=%d]...[ok]", ptpHead->shmIndex);

        ptpTran = &(gShm.pShmTran[ptpHead->shmIndex-1].tpTran);
        ptpCtx->msgCtx = &ptpTran->msgCtx;
        tpMsgCtxClear(ptpCtx->msgCtx);

        /*��ˮ��¼*/
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
        ptpTran->tpLog.packGrp = ptpMapp->packGrp; /*���ڷ��ط����ʱ���*/
        ptpTran->tpLog.unpkGrp = 0;                /*�����м䲽���ʱ���*/
        ptpTran->tpLog.beginTime = ptpHead->beginTime;
        ptpTran->tpLog.overTime = ptpHead->beginTime + ptpPort->timeout;
        ptpTran->tpLog.servIndex = ptpMapp->servIndex;
        ptpTran->tpLog.monFlag = ptpServ->monFlag;
        ptpTran->tpLog.status = TP_RUNING;

        for (i=0; i<MAXREVNUM; i++) ptpTran->tpRevs[i].desQ = 0;

        /*ִ�н��ת����*/
        pConvGrp = &gShm.pConvGrp[ptpMapp->unpkGrp-1];
        if (tpMsgConvUnpack(pConvGrp, &ptpPort->fmtDefs, ptpCtx) != 0) {
            LOG_ERR("ִ�н��ת����[%d]����", pConvGrp->grpId);
            return TP_ERROR;
        }
        tpPreBufFree(&ptpCtx->preBuf);
        LOG_MSG("ִ�н��ת����[%d]...[ok]", pConvGrp->grpId);
        if (hasLogLevel(DEBUG)) tpHashDebug(ptpCtx->msgCtx);

        /*������� Ϊ��֪ͨ���ش���ɹ��������*/ 
        if (ptpServ->nMaxReq <= ptpServ->nActive+ptpServ->nReving) {
            LOG_ERR("����[%s]������[%d+%d]�����޶�ֵ[%d]", ptpServ->servCode,
                     ptpServ->nActive, ptpServ->nReving, ptpServ->nMaxReq);
            return TP_BUSY;
        }

    } else { /*�������Ӧ����*/
        /*�����ˮ��¼*/
        if ((ret = tpTranSetBusy(ptpHead->tpId, ptpHead->shmIndex)) != 0) {
            if (ret != SHM_NONE) {
                LOG_ERR("???"); 
                return TP_ERROR;
            }
            LOG_WRN("��ˮ��¼[tpId=%d]������", ptpHead->tpId);
            
            /*�ٵ���Ӧ*/
            msgLen = ptpHead->bodyLen + sizeof(TPHEAD);
            if ((ret = mq_post(mqh, NULL, Q_REV, 0, NULL, (char *)ptpMsg, msgLen, 0)) != 0) {
                LOG_ERR("mq_post() error: retcode=%d", ret);
                LOG_ERR("д�������[%d]����", Q_REV);
                LOG_ERR("ת���ٵ���Ӧ���ĸ��������������");
            }
            LOG_MSG("ת���ٵ���Ӧ���ĸ�����������");
            return TP_REVING;
        }
        ptpTran = &(gShm.pShmTran[ptpHead->shmIndex-1].tpTran);
        ptpCtx->msgCtx = &ptpTran->msgCtx;

        /*��ȡ�˵�����*/
        if (!(ptpPort = tpPortShmGet(ptpHead->desQ, NULL))) {
            LOG_ERR("��ȡ�˵�[%d]���ó���", ptpHead->desQ);
            return TP_ERROR;
        }

        /*���Ľ��*/
        if (ptpPort->portType != 2) { /*����������˵�*/
            if (!ptpCtx->preBuf.fmtType) {
                /*����Ԥ���*/
                if (tpPreUnpack(ptpCtx, &ptpPort->fmtDefs) != 0) {
                    LOG_ERR("����Ԥ�������");
                    return TP_ERROR;
                }
                LOG_MSG("����Ԥ���...[ok]");
            }

            /*ִ�н��ת����*/
            pConvGrp = &gShm.pConvGrp[ptpTran->tpLog.unpkGrp-1];
            if (tpMsgConvUnpack(pConvGrp, &ptpPort->fmtDefs, ptpCtx) != 0) {
                LOG_ERR("ִ�н��ת����[%d]����", pConvGrp->grpId);
                return TP_ERROR;
            }
            tpPreBufFree(&ptpCtx->preBuf);
            LOG_MSG("ִ�н��ת����[%d]...[ok]", pConvGrp->grpId);
            if (hasLogLevel(DEBUG)) tpHashDebug(ptpCtx->msgCtx);
        }
    }

    pRoutGrp = &gShm.pRoutGrp[ptpTran->tpLog.routGrp-1];
    LOG_MSG("·����=[%d]...", pRoutGrp->grpId);

    /*ִ��·�ɽű�*/
    for (i=ptpTran->tpLog.routLine; i<pRoutGrp->nStmts; ) {
        pRoutStmt = &gShm.pRoutStmt[i+pRoutGrp->nStart];
        switch (pRoutStmt->action) {
            case DO_NONE: /*do nothing*/
                break;

            case DO_JUMP: /*������ת*/
                if (!pRoutStmt->express[0]) {
                    i = pRoutStmt->jump - pRoutGrp->nStart;
                    continue;
                } else {
                    if ((tmpLen = tpFunExec(pRoutStmt->express, tmpBuf, sizeof(tmpBuf))) < 0) {
                        LOG_ERR("������ת�������ʽ����");
                        return TP_ERROR;
                    }
                    if (tmpBuf[0] != 0 && tmpBuf[0] != '0') {
                        i = pRoutStmt->jump - pRoutGrp->nStart; 
                        continue;
                    }
                }
                break;

            case DO_VAR: /*����ʱ����*/
                if ((tmpLen = tpFunExec(pRoutStmt->express, tmpBuf, sizeof(tmpBuf))) < 0) {
                    LOG_ERR("������ʱ����ֵ����");
                    return TP_ERROR;
                }
                if (tpMsgCtxHashPut(ptpCtx->msgCtx, pRoutStmt->fldName, tmpBuf, tmpLen) != 0) {
                    LOG_ERR("����ʱ����[%s]����", pRoutStmt->fldName);
                    return TP_ERROR;
                }
                break;

            case DO_ALLOC: /*Ԥ����ռ�(�������ʱ����)*/
                if ((tmpLen = tpFunExec(pRoutStmt->express, tmpBuf, sizeof(tmpBuf))) < 0) {
                    LOG_ERR("����Ԥ����ռ��С����");
                    return TP_ERROR;
                }
                if (tpMsgCtxHashAlloc(ptpCtx->msgCtx, pRoutStmt->fldName, atoi(tmpBuf)) != 0) {
                    LOG_ERR("Ϊ��ʱ����[%s]Ԥ����ռ�[%d]����", pRoutStmt->fldName, atoi(tmpBuf));
                    return TP_ERROR;
                }
                break;

            case DO_SET: /*����������*/
                if ((tmpLen = tpFunExec(pRoutStmt->express, tmpBuf, sizeof(tmpBuf))) < 0) {
                    LOG_ERR("������������ֵ���ʽ����");
                    return TP_ERROR;
                }
                if (!(key = tpGetCompKey(pRoutStmt->fldName, keyBuf))) {
                    LOG_ERR("����ѭ���������ʽ����");
                    return TP_ERROR;
                }
                if (tpMsgCtxHashPut(ptpCtx->msgCtx, key, tmpBuf, tmpLen) != 0) {
                    LOG_ERR("����������[%s]����", key);
                    return TP_ERROR;
                }
                break;

            case DO_EXEC: /*ִ�нű�����*/
                if ((tmpLen = tpFunExec(pRoutStmt->express, tmpBuf, sizeof(tmpBuf))) < 0) {
                    LOG_ERR("ִ�нű���������");
                    return TP_ERROR;
                }
                break;

            case DO_SEND: /*���ͱ���*/
                ptpHead->desQ = pRoutStmt->desQ;
                pConvGrp = &gShm.pConvGrp[atoi(pRoutStmt->procCode)-1];
                LOG_MSG("SEND [%d] USING [%d]", ptpHead->desQ, pConvGrp->grpId);

                /*��ȡ�˵�����*/
                if (!(ptpPort = tpPortShmGet(ptpHead->desQ, NULL))) {
                    LOG_ERR("��ȡ�˵�[%d]���ó���", ptpHead->desQ);
                    return TP_ERROR;
                }

                /*ִ�д��ת����*/
                if (tpMsgConvPack(pConvGrp, &ptpPort->fmtDefs, ptpCtx) != 0) {
                    LOG_ERR("ִ�д��ת����[%d]����", pConvGrp->grpId);
                    return TP_ERROR;
                }
                tpPreBufFree(&ptpCtx->preBuf);
                LOG_MSG("ִ�д��ת����[%d]...[ok]", pConvGrp->grpId);
                if (hasLogLevel(DEBUG)) LOG_HEX(ptpMsg->body, ptpHead->bodyLen);

                /*дĿ�����*/
                msgLen = ptpHead->bodyLen + sizeof(TPHEAD);
                if ((ret = mq_post(mqh, gShm.ptpConf->dispatch, ptpHead->desQ, 0, NULL, (char *)ptpMsg, msgLen, 0)) != 0) {
                    LOG_ERR("mq_post() error: retcode=%d", ret);
                    LOG_ERR("дĿ�����[%d]����", ptpHead->desQ);
                    return TP_ERROR;
                }
                LOG_MSG("дĿ�����[%d]...[ok]", ptpHead->desQ);
                break;

            case DO_CALL: /*���÷���*/
                ptpHead->desQ = pRoutStmt->desQ;
                strncpy(ptpHead->procCode, pRoutStmt->procCode, MAXKEYLEN);

                /*��ȡ�˵�����*/
                if (!(ptpPort = tpPortShmGet(ptpHead->desQ, NULL))) {
                    LOG_ERR("��ȡ�˵�[%d]���ó���", ptpHead->desQ);
                    return TP_ERROR;
                }

                if (ptpPort->portType == 2) { /*��������˵�*/
                    LOG_MSG("CALL [%d] [%s]", ptpHead->desQ, pRoutStmt->procCode);
                } else {
                    ptpProc = &gShm.ptpProc[pRoutStmt->procIndex-1];
                    pConvGrp = &gShm.pConvGrp[ptpProc->packGrp-1];
                    LOG_MSG("CALL [%d] USING [%d]", ptpHead->desQ, pConvGrp->grpId);
                }

                /*����й����ĳ�������, �ó�����Ϣ*/
                if (pRoutStmt->revFlag) {
                    for (j=0; j<MAXREVNUM; j++) {
                        if (!ptpTran->tpRevs[j].desQ) break;
                        if (!ptpPort->multiRev && ptpHead->desQ == ptpTran->tpRevs[j].desQ) break; /*��֧��ͬ�˵��ظ�����*/
                    }
                    if (j < MAXREVNUM) {
                        ptpTran->tpRevs[j].desQ = ptpHead->desQ;
                        strncpy(ptpTran->tpRevs[j].procCode, pRoutStmt->procCode, MAXKEYLEN);
                        ptpTran->tpRevs[j].packGrp = ptpProc->revPackGrp;
                        ptpTran->tpRevs[j].unpkGrp = ptpProc->revUnpkGrp;
                        ptpTran->tpRevs[j].forceRev = pRoutStmt->forceRev;
                        ptpTran->tpRevs[j].noRespRev = pRoutStmt->noRespRev;
                        LOG_MSG("WITH REVERSE USING [%d] [%d]", ptpProc->revPackGrp, ptpProc->revUnpkGrp);
                    } else { LOG_WRN("MAXREVNUM����ֵ[%d]��С", MAXREVNUM); }
                }

                ptpTran->tpLog.desQ = ptpHead->desQ;
                strncpy(ptpTran->tpLog.procCode, ptpHead->procCode, MAXKEYLEN);
                ptpTran->tpLog.routLine = i+1;
                ptpTran->tpLog.unpkGrp = ptpProc->unpkGrp;

                if (ptpPort->portType == 2) { /*��������˵�*/
                    ptpHead->fmtType = 0;
                    ptpHead->bodyLen = 0;
                } else {
                    /*ִ�д��ת����*/
                    if (tpMsgConvPack(pConvGrp, &ptpPort->fmtDefs, ptpCtx) != 0) {
                        LOG_ERR("ִ�д��ת����[%d]����", pConvGrp->grpId);
                        if (pRoutStmt->revFlag && j < MAXREVNUM) ptpTran->tpRevs[j].desQ = 0;
                        return TP_ERROR;
                    }
                    LOG_MSG("ִ�д��ת����[%d]...[ok]", pConvGrp->grpId);
                    if (hasLogLevel(DEBUG)) LOG_HEX(ptpMsg->body, ptpHead->bodyLen);

                    /*���汨��ͷ*/
                    if (ptpPort->headSaveKey[0]) { /*�첽ͨѶ�˵�ʱ*/
                        ptpTran->tpLog.headId = tpHeadShmPut(ptpTran->tpLog.headId, ptpPort, ptpHead);
                        if (ptpTran->tpLog.headId <= 0) {
                            LOG_ERR("�����첽ͨѶ�˵�[%d]����ͷ����", ptpHead->desQ);
                            if (pRoutStmt->revFlag && j < MAXREVNUM) ptpTran->tpRevs[j].desQ = 0;
                            tpPreBufFree(&ptpCtx->preBuf);
                            if(ptpTran->tpLog.headId  == SHM_DUPHDKEY) return TP_DUPHDKEY; /*add by xujun 20161215*/
                            return TP_ERROR;
                        }
                    }
                    tpPreBufFree(&ptpCtx->preBuf);
                }

                tpTranSetFree(ptpHead->tpId, ptpHead->shmIndex);

                /*дĿ�����*/
                msgLen = ptpHead->bodyLen + sizeof(TPHEAD);
                if (ptpPort->portType == 1) { /*Ӧ��Ŀ��˵�*/
                    ret = mq_post(mqh, gShm.ptpConf->dispatch, ptpHead->desQ, 0, NULL, (char *)ptpMsg, msgLen, 0);
                } else if (ptpPort->portType == 2) { /*����Ŀ��˵�*/
                    ret = mq_post(mqh, NULL, ptpHead->desQ, 0, NULL, (char *)ptpMsg, msgLen, 0);
                } else if (ptpPort->portType == 3) { /*����Ŀ��˵�*/
                    ret = mq_post(mqh, NULL, Q_SVR, 0, NULL, (char *)ptpMsg, msgLen, 0);
                } else {
                    LOG_ERR("Ŀ��˵����Ͳ���ȷ");
                    return TP_ERROR;
                }
                if (ret != 0) {
                    LOG_ERR("mq_post() error: retcode=%d", ret);
                    LOG_ERR("дĿ�����[%d]����", ptpHead->desQ);
                    if (pRoutStmt->revFlag && j < MAXREVNUM) ptpTran->tpRevs[j].desQ = 0;
                    return TP_ERROR;
                }
                LOG_MSG("дĿ�����[%d]...[ok]", ptpHead->desQ);

                return TP_RUNING;
                break;

            case DO_REVERSE: /*����*/
                LOG_MSG("REVERSE...");
                ptpTran->tpLog.status = TP_REVING;

                /*�����ǰ���������¼(�򷵻ز���ȷ���������)*/
                for (j=MAXREVNUM-1; j>=0; j--) {
                    if (ptpTran->tpLog.desQ == ptpTran->tpRevs[j].desQ) break;
                }
                if (j>=0) ptpTran->tpRevs[j].desQ = 0;

                /*����������*/
                if (tpTranCtxSave(ptpHead->tpId, ptpTran) != 0) {
                    LOG_ERR("���������ĳ���");
                    return TP_REVING;
                }

                /*��֯����ͷ*/
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

                /*���ͳ�������*/
                if ((ret = mq_post(mqh, NULL, Q_REV, 0, NULL, (char *)&tpHead, sizeof(TPHEAD), 0)) != 0) {
                    LOG_ERR("mq_post() error: retcode=%d", ret);
                    LOG_ERR("д�������[%d]����", Q_REV);
                    LOG_ERR("�������������ͳ����������");
                }
                LOG_MSG("�������������ͳ�������...[ok]");
                return TP_REVING;
                break;

            case DO_RETURN: /*���ط����*/
                if (strcmp(pRoutStmt->express, "NONE") == 0) return TP_OK;
                if (ptpTran->tpLog.packGrp <= 0) {
                    LOG_ERR("����[%s]�޷��ش��ת��������", ptpHead->servCode);
                    return TP_ERROR;
                }
                ptpHead->retFlag = atoi(pRoutStmt->express);
                pConvGrp = &gShm.pConvGrp[ptpTran->tpLog.packGrp-1];
                LOG_MSG("RETURN [%d](retFlag=%d) USING [%d]", ptpHead->orgQ, ptpHead->retFlag, pConvGrp->grpId);

                ptpHead->desQ = ptpHead->orgQ;
                ptpHead->procCode[0] = 0;

                /*��ȡ�˵�����*/
                if (!(ptpPort = tpPortShmGet(ptpHead->orgQ, NULL))) {
                    LOG_ERR("��ȡ�˵�[%d]���ó���", ptpHead->orgQ);
                    return TP_ERROR;
                }

                /*ִ�д��ת����*/
                if (tpMsgConvPack(pConvGrp, &ptpPort->fmtDefs ,ptpCtx) != 0) {
                    LOG_ERR("ִ�д��ת����[%d]����", pConvGrp->grpId);
                    return TP_ERROR;
                }
                tpPreBufFree(&ptpCtx->preBuf);
                LOG_MSG("ִ�д��ת����[%d]...[ok]", pConvGrp->grpId);
                if (hasLogLevel(DEBUG)) LOG_HEX(ptpMsg->body, ptpHead->bodyLen);

                /*дԴ������*/
                msgLen = ptpHead->bodyLen + sizeof(TPHEAD);
                snprintf(clazz, sizeof(clazz), "%d", ptpHead->tpId);
                if ((ret = mq_post(mqh, gShm.ptpConf->dispatch, ptpHead->orgQ, 0, clazz, (char *)ptpMsg, msgLen, 0)) != 0) {
                    LOG_ERR("mq_post() error: retcode=%d", ret);
                    LOG_ERR("дԴ������[%d]����", ptpHead->orgQ);
                    return TP_ERROR;
                }
                LOG_MSG("дԴ������[%d]...[ok]", ptpHead->orgQ);
                return TP_OK;
                break;

            default:
                break;
        } /*switch end*/
        i++;
    } /* for end*/
    return TP_OK;
}

