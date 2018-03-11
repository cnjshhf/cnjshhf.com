/***************************************************************
 * ����������
 **************************************************************/
#include "tpbus.h"

#define NMAX 10

void *tpProcessThread(void *arg);
void  revCntReset(char *servCode, int cnt);

/***************************************************************
 * ������
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

    /*��������в���*/
    if (argc != 2) {
        printf("Usage: %s <����ID>\n", argv[0]);
        return -1;
    }

    /*��ʼ������*/
    /*if (tpTaskInit(TSK_TRANMNG, "tpTranMng", NULL) != 0) { */
    if (tpTaskInit(atoi(argv[1]), "tpTranMng", NULL) != 0) { 
        LOG_ERR("��ʼ�����̳���");
        return -1;
    }

    /*��ʼ������*/
    if ((ret = mq_init()) != 0) {
        LOG_ERR("mq_init() error: retcode=%d", ret);
        return -1;
    }

    /*�󶨶���*/
    if ((ret = mq_bind(Q_REV, &mqh)) != 0) {
        LOG_ERR("mq_bind() error: retcode=%d", ret);
        LOG_ERR("�󶨶���[%d]����", Q_REV);
        return -1;
    }

    /*�����ݿ�*/
    if (tpSQLiteOpen(&dbh) != 0) {
       LOG_ERR("�����ݿ����");
       return -1;
    }

    /*���������̳߳�*/
    for (i=0; i<gShm.ptpConf->nRevProcs; i++) {
        if (tpThreadCreate(tpProcessThread, NULL) != 0) {
            LOG_ERR("���������߳�ʧ��: %s", strerror(errno));
            return -1;
        }
    }

    tpShmLock(SEM_SAFTMO); /*��ֹ�������ͬʱ��ѯ*/

    /*��ѯ��ʱSAF*/
    while (1) {
        if ((j = tpSafTimeoutFind(dbh, tpSafs, NMAX)) < 0) {
            LOG_ERR("���ҳ�ʱSAF����");
            return -1;
        }

        if (j==0) { /*�޳�ʱSAF*/
            if (tpShmRetouch() != 0) {
                LOG_ERR("ˢ�¹����ڴ�ָ�����");
                continue;
            }
            sleep(gShm.ptpConf->ntsMonSaf);
            continue;
        }

        for (i=0; i<j; i++) {
            tpSetLogPrefix(tpSafs[i].tpId, logPrefix);
            LOG_MSG("��ʱSAF: safId=[%d],tpId=[%d],desQ=[%d],procCode=[%s]", tpSafs[i].safId, tpSafs[i].tpId, tpSafs[i].desQ, tpSafs[i].procCode);

            flag = 0;
            tpMsg.head.tpId  = 0;
            tpMsg.head.safId = tpSafs[i].safId;

            while (1) {
                /*���SAF����*/
                if (tpSafs[i].safNum <= 0) {
                    LOG_MSG("SAF������Ϊ0");
                    break;
                }

                /*��ȡSAF����*/
                if (tpSafMsgLoad(tpSafs[i].safId, &tpMsg) != 0) {
                    LOG_ERR("��ȡSAF���ĳ���");
                    break;
                }
                LOG_MSG("��ȡSAF����...[ok]");
                if (hasLogLevel(DEBUG)) tpHeadDebug(&tpMsg.head);
                if (hasLogLevel(DEBUG)) LOG_HEX(tpMsg.body, tpMsg.head.bodyLen);

                /*����SAF����*/
                tpSafs[i].safNum--;

                /*��ȡ�˵�����*/
                if (!(ptpPort = tpPortShmGet(tpSafs[i].desQ, NULL))) {
                    LOG_ERR("��ȡ�˵�[%d]���ó���", tpSafs[i].desQ);
                    break;
                }

                /*����SAF��ʱʱ��*/
                time(&t);
                tpSafs[i].overTime = t + ptpPort->timeout + ptpPort->safDelay;
                tpSafs[i].overTime += (ptpPort->nMaxSaf - tpSafs[i].safNum) * ptpPort->safDelay;

                /*����SAF��¼*/
                if (tpSafUpdate(dbh, &tpSafs[i]) != 0) {
                    LOG_ERR("����SAF��¼[safId=%d]����", tpSafs[i].safId);
                    break;
                }
    		/*�ط�SAF����*/
                msgLen = tpMsg.head.bodyLen + sizeof(TPHEAD);
                tpMsg.head.beginTime = t+1;
         if ((ret = mq_post(mqh, gShm.ptpConf->dispatch, tpSafs[i].desQ, 0, NULL, (char *)&tpMsg, msgLen, 0)) != 0) {
                    LOG_ERR("mq_post() error: retcode=%d", ret);
                    LOG_ERR("д����[%d]����", tpSafs[i].desQ);
                    LOG_ERR("�ط�SAF���ĳ���");
                    break;
                }
                LOG_MSG("�ط�SAF����...[ok]  tpMsg.head.beginTime =[%ld]",  tpMsg.head.beginTime);
            
                flag = 1;
                break;
            } /*end while*/

            if (!flag) {
                LOG_MSG("����ʧ��");

                /*���ͼ����Ϣ*/
                if (gShm.ptpConf->sysMonFlag) {
                    tpMsg.head.status = TP_SAFFAIL;
                    tpSendToSysMon(&tpMsg.head);
                }
                
                /*����SAF����*/
                snprintf(buf, sizeof(buf), "rm -f %s/rtm/saf/%d.saf", getenv("TPBUS_HOME"), tpSafs[i].safId);
                system(buf);

                /*ɾ��SAF��¼*/
                if (tpSafDelete(dbh, tpSafs[i].safId) != 0) {
                    LOG_ERR("ɾ��SAF��¼[safId=%d]����", tpSafs[i].safId);
                    usleep(1000); /*1ms*/
                }

                /*���ó���������*/
                revCntReset(tpSafs[i].servCode, -1);

                /*��¼����ʧ����ˮ*/
                if (gShm.ptpConf->hisLogFlag) {
                    /*��ȡ������*/
                    if (tpTranCtxLoad(tpSafs[i].tpId, &tpTran) != 0) {
                        LOG_ERR("��ȡ������[tpId=%d]����", tpSafs[i].tpId);
                    } else {
                        ptpLog = &tpTran.tpLog;
                        ptpLog->status = TP_REVFAIL;
                        if (tpLogInsert(dbh, ptpLog) != 0) {
                            LOG_ERR("��¼����ʧ����ˮ[tpId=%d]����", ptpLog->tpId);
                        }
                    }
                }

                /*����������*/
                snprintf(buf, sizeof(buf), "rm -f %s/rtm/ctx/%d.ctx", getenv("TPBUS_HOME"), tpSafs[i].tpId);
                system(buf);
				/*�����첽����ͷ*/
                snprintf(buf, sizeof(buf), "rm -f %s/rtm/tph/*.%d", getenv("TPBUS_HOME"), tpSafs[i].tpId);
                system(buf);
            }
        } /*end for*/
    } /*end while*/
    return 0;
}

/***************************************************************
 * ���ó���������
 **************************************************************/
void  revCntReset(char *servCode, int cnt)
{
    int i;

    if (tpShmRetouch() != 0) {
        LOG_ERR("ˢ�¹����ڴ�ָ�����");
        return;
    }
    if ((i = tpServShmGet(servCode, NULL)) <= 0) {
        LOG_ERR("�޷���[%s]ע����Ϣ", servCode);
        return;
    }
	tpShmLock(SEM_SAFCNT);
    gShm.ptpServ[i-1].nReving += cnt;
    gShm.pShmHead->nReving += cnt;
    tpShmUnlock(SEM_SAFCNT);
}

/***************************************************************
 * ���������߳�
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

    /*�󶨶���*/
    if ((ret = mq_bind(Q_REV, &mqh)) != 0) {
        LOG_ERR("mq_bind() error: retcode=%d", ret);
        LOG_ERR("�󶨶���[%d]����", Q_REV);
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
            LOG_ERR("������[%d]����", Q_REV);
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

        if (q != Q_SVR) {
            /*���Ŀ��˵�*/
            if (q != ptpHead->desQ) {
                LOG_ERR("Ŀ��˵�δ���û���ʵ�ʲ���");
                continue;
            }

            /*���˵�����*/
            if (!(ptpPort = tpPortShmGet(ptpHead->desQ, NULL))) {
                LOG_ERR("��ȡ�˵�[%d]���ó���", ptpHead->desQ);
                continue;
            }

            /*��鱨�ĸ�ʽ*/
            if (ptpHead->fmtType != ptpPort->fmtDefs.fmtType) {
                LOG_ERR("���ĸ�ʽδ���û���Ҫ��Ĳ���");
                continue;
            }
        }

        if (tpShmRetouch() != 0) {
            LOG_ERR("ˢ�¹����ڴ�ָ�����");
            continue;
        }

        tpCtx.msgCtx = NULL;
        tpPreBufInit(&tpCtx.preBuf, NULL);
        ptpLog = NULL;
        ptpRev = NULL;

        if (!ptpHead->tpId) { /*�첽ͨѶ�˵���Ӧ*/
            /*����Ԥ���*/
            if (tpPreUnpack(&tpCtx, &ptpPort->fmtDefs) != 0) {
                LOG_ERR("����Ԥ�������");
                tpPreBufFree(&tpCtx.preBuf);
                continue;
            }
            LOG_MSG("����Ԥ���...[ok]");

            /*��ȡ����ͷ*/
            if ((ret = tpHeadLoad(ptpPort, ptpHead)) != 0) {
                if (ret == ENOENT) {
                    LOG_DBG("�Ǳ��ڵ㽻��");
                    tpPreBufFree(&tpCtx.preBuf);
                    continue;
                }
                LOG_ERR("��ȡ�첽ͨѶ�˵�[%d]����ͷ����", ptpHead->desQ);
                tpPreBufFree(&tpCtx.preBuf);
                continue;
            }
            ptpHead->bodyLen = msgLen - sizeof(TPHEAD);
            tpSetLogPrefix(ptpHead->tpId, logPrefix);
            LOG_MSG("��ȡ�첽ͨѶ�˵�[%d]����ͷ...[ok]", ptpHead->desQ);
            if (hasLogLevel(DEBUG)) tpHeadDebug(ptpHead);
        }

        LOG_MSG("dwxiong: ptpHead->msgType[%d]", ptpHead->msgType);  /*add by dwxiong test 2014-11-8 15:16:34*/
        switch (ptpHead->msgType) {
            case MSGSYS_TIMEOUT: /*��ʱ����*/
            case MSGSYS_REVERSE: /*��������*/
                /*��ȡ������*/
                if (tpTranCtxLoad(ptpHead->tpId, &tpTran) != 0) {
                    LOG_ERR("��ȡ������[tpId=%d]����", ptpHead->tpId);

                    /*����������*/
                    snprintf(buf, sizeof(buf), "rm -f %s/rtm/ctx/%d.ctx", getenv("TPBUS_HOME"), ptpHead->tpId);
                    system(buf);
                    /*�����첽����ͷ*/
                    snprintf(buf, sizeof(buf), "rm -f %s/rtm/tph/*.%d", getenv("TPBUS_HOME"), ptpHead->tpId);
                    system(buf);
                    continue;
                }
                ptpLog = &tpTran.tpLog;
                ptpRev = tpTran.tpRevs;
                tpCtx.msgCtx = &tpTran.msgCtx;

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

                    /*ִ�д��ת����*/
                    if (tpMsgConvPack(pConvGrp, &ptpPort->fmtDefs, &tpCtx) != 0) {
                        LOG_ERR("ִ�д��ת����[%d]����", pConvGrp->grpId);
                        break;
                    }
                    tpPreBufFree(&tpCtx.preBuf);
                    LOG_MSG("ִ�д��ת����[%d]...[ok]", pConvGrp->grpId);
                    if (hasLogLevel(DEBUG)) LOG_HEX(ptpMsg->body, ptpHead->bodyLen);
                    if (!ptpHead->bodyLen) break; /*���Կձ���*/

                    /*дԴ������*/
                    ptpHead->msgType = MSGAPP;
                    msgLen = ptpHead->bodyLen + sizeof(TPHEAD);
                    snprintf(clazz, sizeof(clazz), "%d", ptpHead->tpId);
                    if ((ret = mq_post(mqh, gShm.ptpConf->dispatch, ptpHead->orgQ, 0, clazz, (char *)ptpMsg, msgLen, 0)) != 0) {
                        LOG_ERR("mq_post() error: retcode=%d", ret);
                        LOG_ERR("дԴ������[%d]����", ptpHead->orgQ);
                    }
                    LOG_MSG("дԴ������[%d]...[ok]", ptpHead->orgQ);
                    break;
                }
                flag = 1;
                break;

            case MSGREV: /*������Ӧ*/
                flag = 0;
                while (1) {
                    /*��ȡ������*/
                    if (tpTranCtxLoad(ptpHead->tpId, &tpTran) != 0) {
                        LOG_ERR("��ȡ������[tpId=%d]����", ptpHead->tpId);
                        break;
                    }
                    ptpLog = &tpTran.tpLog;
                    ptpRev = tpTran.tpRevs;
                    tpCtx.msgCtx = &tpTran.msgCtx;

                    /*���ҳ��������¼*/
                    for (j=MAXREVNUM-1; j>=0; j--) {
                        if (ptpRev[j].desQ == ptpHead->desQ) break;
                    }
                    if (j<0) {
                        LOG_ERR("û���ҵ�Ŀ��˵������¼");
                        break;
                    }
                    LOG_MSG("dwxiong: ptpPort->respCodeKey[%s]", ptpPort->respCodeKey);  /*add by dwxiong test 2014-11-8 15:16:34*/
                    /*����������Ƿ���ȷ*/
                    if (!ptpPort->respCodeKey[0]) {
                        flag = 1;
                    } else {
                        if (ptpPort->portType != 2) { /*����������˵�*/
                            /*��ȡ�˵�����*/
                            if (!(ptpPort = tpPortShmGet(ptpHead->desQ, NULL))) {
                                LOG_ERR("��ȡ�˵�[%d]���ó���", ptpHead->desQ);
                                break;
                            }

                            if (!tpCtx.preBuf.fmtType) {
                                /*����Ԥ���*/
                                if (tpPreUnpack(&tpCtx, &ptpPort->fmtDefs) != 0) {
                                    LOG_ERR("����Ԥ�������");
                                    break;
                                }
                            }

                            /*���Ľ��*/
                            if (tpConvShmGet(ptpRev[j].unpkGrp, &pConvGrp) <= 0) {
                                LOG_ERR("�������ת����[%d]������", ptpRev[j].unpkGrp);
                                break;
                            }

                            if (tpMsgConvUnpack(pConvGrp, &ptpPort->fmtDefs, &tpCtx) != 0) {
                                LOG_ERR("ִ�н��ת����[%d]����", pConvGrp->grpId);
                                break;
                            }
                            tpPreBufFree(&tpCtx.preBuf);
                        }
                        
                        if ((tmpLen = tpFunExec(ptpPort->respCodeKey, tmpBuf, sizeof(tmpBuf))) < 0) {
                            LOG_ERR("�������������ʽ����");
                            break;
                        }
                        if (tmpBuf[0] != 0 && tmpBuf[0] != '0') flag = 1;
                        LOG_MSG("dwxiong: �������������ʽ[%X], flag=[%d]", tmpBuf[0],flag);  /*add by dwxiong test 2014-11-8 15:16:34*/
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

                /*����SAF����*/
                snprintf(buf, sizeof(buf), "rm -f %s/rtm/saf/%d.saf", getenv("TPBUS_HOME"), ptpHead->safId);
                system(buf);

                /*ɾ��SAF��¼*/
                if (tpSafDelete(dbh, ptpHead->safId) != 0) {
                    LOG_ERR("ɾ��SAF��¼[safId=%d]����", ptpHead->safId);
                    usleep(1000); /*1ms*/
                }

                /*���ó���������*/
                revCntReset(ptpLog->servCode, -1);

                if (!flag) { /*����ʧ��*/
                    /*���ͼ����Ϣ*/
                    if (gShm.ptpConf->sysMonFlag) {
                        ptpHead->status = TP_REVFAIL;
                        tpSendToSysMon(ptpHead);
                    }

                    /*����������*/
                    snprintf(buf, sizeof(buf), "rm -f %s/rtm/ctx/%d.ctx", getenv("TPBUS_HOME"), ptpHead->tpId);
                    system(buf);
                    /*�����첽����ͷ*/
                    snprintf(buf, sizeof(buf), "rm -f %s/rtm/tph/*.%d", getenv("TPBUS_HOME"), ptpHead->tpId);
                    system(buf);

                    /*��¼����ʧ����ˮ*/
                    if (gShm.ptpConf->hisLogFlag && ptpLog != NULL) {
                        ptpLog->status = TP_REVFAIL;
                        if (tpLogInsert(dbh, ptpLog) != 0) {
                            LOG_ERR("��¼����ʧ����ˮ[tpId=%d]����", ptpLog->tpId);
                        }
                    }
                }
                break;

            default:
                flag = 0;
                break;
        } /*end switch*/

        if (!flag) continue;
        
        /*��1��ѭ������֪ͨ�����*/
        /*��2��ѭ��������Ӧ�����*/
        for (i=0,flag=0; i<2; i++) {
            for (j=MAXREVNUM-1; j>=0; j--) {
                if (!ptpRev[j].desQ) continue;
                if (i==0 && !ptpRev[j].noRespRev) continue;

                memset(ptpHead, 0, sizeof(TPHEAD));

                /*����SAF��ˮ��*/
                if (!ptpRev[j].noRespRev) {
                    if ((ptpHead->safId = tpNewSafId()) <= 0) {
                        LOG_ERR("����SAF��ˮ��ʧ��");
                        break;
                    }
                }

                /*׼������ͷ*/
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

                /*��ȡ�˵�����*/
                if (!(ptpPort = tpPortShmGet(ptpHead->desQ, NULL))) {
                    LOG_ERR("��ȡ�˵�[%d]���ó���", ptpHead->desQ);
                    if (ptpRev[j].noRespRev) {
                        ptpRev[j].desQ = 0;
                        continue;
                    }
                    break;
                }

                if (ptpPort->portType == 2) { /*��������˵�*/
                    ptpHead->fmtType = 0;
                    ptpHead->bodyLen = 0;
                } else {
                    /*���Ĵ��*/
                    if (tpConvShmGet(ptpRev[j].packGrp, &pConvGrp) <= 0) {
                        LOG_ERR("�������ת����[%d]������", ptpRev[j].packGrp);
                        if (ptpRev[j].noRespRev) {
                            ptpRev[j].desQ = 0;		
                            continue;
                        }
                        break;
                    }
                    if (tpMsgConvPack(pConvGrp, &ptpPort->fmtDefs, &tpCtx) != 0) {
                        LOG_ERR("ִ�д��ת����[%d]����", pConvGrp->grpId);
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
                    /*����SAF����*/
                    if (tpSafMsgSave(ptpHead->safId, ptpMsg) != 0) {
                        LOG_ERR("����SAF���ĳ���");
                        break;
                    }
          
                    /*����������*/
                    if (tpTranCtxSave(ptpHead->tpId, &tpTran) != 0){
                        LOG_ERR("���������ĳ���");
                        break;
                    }

                    /*����SAF��¼*/
                    tpSaf.safId = ptpHead->safId;
                    tpSaf.tpId = ptpHead->tpId;
                    tpSaf.desQ = ptpHead->desQ;
                    strcpy(tpSaf.servCode, ptpHead->servCode);
                    strcpy(tpSaf.procCode, ptpHead->procCode);
                    tpSaf.safNum = (ptpPort->nMaxSaf <=0)? 0 : (ptpPort->nMaxSaf-1);
                    time(&t);
                    tpSaf.overTime = t + ptpPort->timeout + ptpPort->safDelay;
                    if (tpSafInsert(dbh, &tpSaf) != 0) {
                        LOG_ERR("����SAF��¼����");
                        break;
                    }
                    
                    /*���ó���������*/
                    revCntReset(ptpLog->servCode, 1);
                }

                if (!ptpRev[j].noRespRev) {
                    /*���汨��ͷ*/
                    if (ptpPort->headSaveKey[0]) { /*�첽ͨѶ�˵�ʱ*/
                        if (tpHeadSave(ptpPort, ptpHead) != 0) {
                            LOG_ERR("�����첽ͨѶ�˵�[%d]����ͷ����", ptpHead->desQ);
                            break;
                        }
                    }
                }
                tpPreBufFree(&tpCtx.preBuf);

                /*дĿ�����*/
                msgLen = ptpHead->bodyLen + sizeof(TPHEAD);
                if ((ret = mq_post(mqh, gShm.ptpConf->dispatch, ptpHead->desQ, 0, NULL, (char *)ptpMsg, msgLen, 0)) != 0) {
                    LOG_ERR("mq_post() error: retcode=%d", ret);
                    LOG_ERR("дĿ�����[%d]����", ptpHead->desQ);
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
        
        if (flag) continue; /*�ȴ�������Ӧ*/

        if (ptpHead->safId) {
            /*����SAF����*/
            snprintf(buf, sizeof(buf), "rm -f %s/rtm/saf/%d.saf", getenv("TPBUS_HOME"), ptpHead->safId);
            system(buf);

            /*ɾ��SAF��¼*/
            if (tpSafDelete(dbh, ptpHead->safId) != 0) {
                LOG_ERR("ɾ��SAF��¼[safId=%d]����", ptpHead->safId);
                usleep(1000); /*1ms*/
            }

            /*���ó���������*/
            revCntReset(ptpLog->servCode, -1);
        }

        /*���ͼ����Ϣ*/
        if (gShm.ptpConf->sysMonFlag) {
            ptpHead->status = (j<0)? TP_REVOK : TP_REVFAIL;
            tpSendToSysMon(ptpHead);
        }

        /*����������*/
        snprintf(buf, sizeof(buf), "rm -f %s/rtm/ctx/%d.ctx", getenv("TPBUS_HOME"), ptpLog->tpId);
        system(buf);
        /*�����첽����ͷ*/
        snprintf(buf, sizeof(buf), "rm -f %s/rtm/tph/*.%d", getenv("TPBUS_HOME"), ptpLog->tpId);
        system(buf);

        /*��¼����ʧ����ˮ*/
        if (j>=0) {
            if (gShm.ptpConf->hisLogFlag && ptpLog != NULL) {
                ptpLog->status = TP_REVFAIL;
                if (tpLogInsert(dbh, ptpLog) != 0) {
                    LOG_ERR("��¼����ʧ����ˮ[tpId=%d]����", ptpLog->tpId);
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

