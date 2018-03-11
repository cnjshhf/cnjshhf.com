/**************************************************************
 * 报文转发适配器，用于来源报文请求和响应公用一个通讯链路时
 *************************************************************/
#include "queue.h"
#include "tpbus.h"

TPTASK m_tpTask;


int  m_src_q;                  /*来源队列*/
int  m_des_q;                  /*目的队列*/         /*add by dwxiong 2014-11-2 14:43:20*/
int  m_nMaxThds;               /*最大并发数*/
int  m_tphead;                 /*是否初始化报文头*/ /*add by dwxiong 2015-9-18 15:11:24*/

int   tpParamInit();
void *tpProcessThread(void *arg);

/**************************************************************
 * 主函数
 *************************************************************/
int main(int argc, char *argv[])
{
    int  i,ret;

    /*检查命令行参数*/
    if (argc != 2) {
        printf("Usage: %s <任务ID>\n", argv[0]);
        return -1;
    }

    /*初始化进程*/
    if (tpTaskInit(atoi(argv[1]), "tpAdaptChg", &m_tpTask) != 0) {
        printf("初始化进程出错\n");
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

    /*初始化工作线程池*/
    for (i=0; i<m_nMaxThds; i++) {
        if (tpThreadCreate(tpProcessThread, NULL) != 0) {
            LOG_ERR("创建处理线程失败: %s", strerror(errno));
            return -1;
        }
    }

    while (1) {
        sleep(100);
    }
}

/***************************************************************
 * 初始化配置参数
 **************************************************************/
int tpParamInit()
{
    if (tpGetParamInt(m_tpTask.params, "src_q", &m_src_q) != 0) m_src_q = Q_SVR;
    LOG_MSG("参数[src_q]=[%d]", m_src_q);

    if (tpGetParamInt(m_tpTask.params, "des_q", &m_des_q) != 0) m_des_q = Q_SVR;
    LOG_MSG("参数[des_q]=[%d]", m_des_q);

    if (tpGetParamInt(m_tpTask.params, "tphead", &m_tphead) != 0) m_tphead = 1;
    LOG_MSG("参数[tphead]=[%d]", m_tphead);

    if (tpGetParamInt(m_tpTask.params, "nMaxThds", &m_nMaxThds) != 0) m_nMaxThds = 5;
    LOG_MSG("参数[nMaxThds]=[%d]", m_nMaxThds);

    return 0;
}

/***************************************************************
 * 处理线程
 **************************************************************/
void *tpProcessThread(void *arg)
{
    int    ret,msgLen;
    int    q;
    TPMSG tpMsg;
    MQHDL  *mqh = NULL;

    TPCTX  tpCtx;
    TPHEAD *ptpHead = &tpCtx.tpMsg.head;
	TPPORT	*ptpPort;
    int nId,len,i ;
    char result[512];
    TPCTX *pCtx;

    char typ_procode_concode[100];
    char type_1[20];
	char acctno_2[100];
	char procode_3[20];
	char condcode_25[20];
	char ainstcode_32[100];
	char finstcode_33[100];
	char retrino_37[100];
	char accptermid_41[100];
	char cardcode_42[100];
	char reinstcode_100[100];

    /*设置上下文引用*/
    tpCtxSetTSD(&tpCtx);


    /*绑定队列*/
    if ((ret = mq_bind(m_tpTask.bindQ, &mqh)) != 0) {
        LOG_ERR("mq_bind() error: retcode=%d", ret);
        LOG_ERR("绑定队列[%d]出错", m_tpTask.bindQ);
        tpThreadExit();
        return NULL;
    }

    while (1) {
        msgLen = sizeof(TPMSG);
        memset(&tpMsg, 0x00, sizeof(tpMsg));
        LOG_MSG("待处理");
        /*读待处理队列*/
        if ((ret = mq_pend(mqh, NULL, &q, NULL, NULL, (char *)&tpMsg, &msgLen, 0)) != 0) {
            LOG_ERR("mq_pend() error: retcode=%d", ret);
            LOG_ERR("读队列[%d]出错", m_tpTask.bindQ);
            mq_detach(mqh);
            tpThreadExit();
            return NULL;
        }
		LOG_MSG("得到数据");



        LOG_MSG("收到来自队列[%d]的报文", q);
        if (hasLogLevel(DEBUG)) tpHeadDebug(&tpMsg.head);
        if (hasLogLevel(DEBUG)) LOG_HEX(tpMsg.body, tpMsg.head.bodyLen);

        /* if (m_src_q == q) { */           /*mod by dwxiong 2014-11-8 13:42:34*/
        if (m_src_q == q || Q_REV == q ) {

			/********************************************* add  by Yuan.y 2015年11月19日18:08:23 START*/
			memcpy(&tpCtx.tpMsg, &tpMsg, sizeof(TPMSG));

			tpCtx.msgCtx = NULL;
	        tpPreBufInit(&tpCtx.preBuf, NULL);

	        LOG_MSG("交易报文来源[%d]",  tpCtx.tpMsg.head.orgQ);

	        LOG_MSG("读队列[%d]来源[%d]",  m_tpTask.bindQ, q);
			/*读取端点配置*/
	        /*if (!(ptpPort = tpPortShmGet(300 , NULL))) {
	        */
	        if (!(ptpPort = tpPortShmGet(tpCtx.tpMsg.head.orgQ , NULL))) {
	            LOG_ERR("读取端点[%d]配置出错", ptpHead->orgQ);
	            continue;
	        }

	        /*报文预解包*/
	        if (tpPreUnpack(&tpCtx, &ptpPort->fmtDefs) != 0) {
	            LOG_ERR("报文预解包出错");
	            continue;
	        }
	        LOG_MSG("报文预解包...[ok]");

	    	memset(result, 0x00, sizeof(result));

		    pCtx = tpCtxGetTSD();
		    if (!pCtx || !pCtx->preBuf.ptr) {
		        LOG_ERR("上下文不正确");
		        continue;
		    }

			LOG_MSG("域值存储块[%s]",((MSGCTX *)(pCtx->preBuf.ptr))->buf);
			LOG_MSG("域值存储块[%d]",((MSGCTX *)(pCtx->preBuf.ptr))->flds[24].offset);

			for(i=0;i<MAXFLDNUM;i++){
				if( 0 != ((MSGCTX *)(pCtx->preBuf.ptr))->flds[i].length )
				LOG_MSG("域ID[%03d]域名[%20s] 域值偏移量[%03d]  域值长度[%03d] 域值空间大小[%03d] 下一同键值域[%03d]",
				i+1,
				((MSGCTX *)(pCtx->preBuf.ptr))->flds[i].name   ,
				((MSGCTX *)(pCtx->preBuf.ptr))->flds[i].offset ,
				((MSGCTX *)(pCtx->preBuf.ptr))->flds[i].length ,
				((MSGCTX *)(pCtx->preBuf.ptr))->flds[i].size   ,
				((MSGCTX *)(pCtx->preBuf.ptr))->flds[i].next
				);
			}

			/*
			char   name[MAXKEYLEN+1];   域名
	        int    offset;              域值偏移量
	        int    length;              域值长度
	        int    size;                域值空间大小
	        short  next;                下一同键值域
			*/
		    nId = 1;
		    memset(type_1, 0x00, sizeof(type_1));
		    if (nId <= 0 || nId > NMAXBITS) {
		        LOG_ERR("域ID值[%d]不正确", nId);
		        continue;
		    }
		    if ((len = tpMsgCtxGet(pCtx->preBuf.ptr, nId-1, type_1, 100)) < 0) {
		        LOG_ERR("tpMsgCtxGet(%d) error", nId-1);
		        continue;
		    }
		    LOG_MSG("报文[%03d]域.长度[%04d]=[%s]",nId, ((MSGCTX *)(pCtx->preBuf.ptr))->flds[nId-1].length, type_1);

		    nId = 2;
		    memset(acctno_2, 0x00, sizeof(acctno_2));
		    if (nId <= 0 || nId > NMAXBITS) {
		        LOG_ERR("域ID值[%d]不正确", nId);
		        continue;
		    }
		    if ((len = tpMsgCtxGet(pCtx->preBuf.ptr, nId-1, acctno_2, 100)) < 0) {
		        LOG_ERR("tpMsgCtxGet(%d) error", nId-1);
		        continue;
		    }
		    LOG_MSG("报文[%03d]域.长度[%04d]=[%s]",nId, ((MSGCTX *)(pCtx->preBuf.ptr))->flds[nId-1].length, acctno_2);

		    nId = 3;
		    memset(procode_3, 0x00, sizeof(procode_3));
		    if (nId <= 0 || nId > NMAXBITS) {
		        LOG_ERR("域ID值[%d]不正确", nId);
		        continue;
		    }
		    if ((len = tpMsgCtxGet(pCtx->preBuf.ptr, nId-1, procode_3, 100)) < 0) {
		        LOG_ERR("tpMsgCtxGet(%d) error", nId-1);
		        continue;
		    }
		    LOG_MSG("报文[%03d]域.长度[%04d]=[%s]",nId, ((MSGCTX *)(pCtx->preBuf.ptr))->flds[nId-1].length, procode_3);

		    nId = 25;
		    memset(condcode_25, 0x00, sizeof(condcode_25));
		    if (nId <= 0 || nId > NMAXBITS) {
		        LOG_ERR("域ID值[%d]不正确", nId);
		        continue;
		    }
		    if ((len = tpMsgCtxGet(pCtx->preBuf.ptr, nId-1, condcode_25, 100)) < 0) {
		        LOG_ERR("tpMsgCtxGet(%d) error", nId-1);
		        continue;
		    }
		    LOG_MSG("报文[%03d]域.长度[%04d]=[%s]",nId, ((MSGCTX *)(pCtx->preBuf.ptr))->flds[nId-1].length, condcode_25);

		    nId = 32;
		    memset(ainstcode_32, 0x00, sizeof(ainstcode_32));
		    if (nId <= 0 || nId > NMAXBITS) {
		        LOG_ERR("域ID值[%d]不正确", nId);
		        continue;
		    }
		    if ((len = tpMsgCtxGet(pCtx->preBuf.ptr, nId-1, ainstcode_32, 100)) < 0) {
		        LOG_ERR("tpMsgCtxGet(%d) error", nId-1);
		        continue;
		    }
		    LOG_MSG("报文[%03d]域.长度[%04d]=[%s]",nId, ((MSGCTX *)(pCtx->preBuf.ptr))->flds[nId-1].length, ainstcode_32);

		    nId = 33;
		    memset(finstcode_33, 0x00, sizeof(finstcode_33));
		    if (nId <= 0 || nId > NMAXBITS) {
		        LOG_ERR("域ID值[%d]不正确", nId);
		        continue;
		    }
		    if ((len = tpMsgCtxGet(pCtx->preBuf.ptr, nId-1, finstcode_33, 100)) < 0) {
		        LOG_ERR("tpMsgCtxGet(%d) error", nId-1);
		        continue;
		    }
		    LOG_MSG("报文[%03d]域.长度[%04d]=[%s]",nId, ((MSGCTX *)(pCtx->preBuf.ptr))->flds[nId-1].length, finstcode_33);

		    nId = 37;
		    memset(retrino_37, 0x00, sizeof(retrino_37));
		    if (nId <= 0 || nId > NMAXBITS) {
		        LOG_ERR("域ID值[%d]不正确", nId);
		        continue;
		    }
		    if ((len = tpMsgCtxGet(pCtx->preBuf.ptr, nId-1, retrino_37, 100)) < 0) {
		        LOG_ERR("tpMsgCtxGet(%d) error", nId-1);
		        continue;
		    }
		    LOG_MSG("报文[%03d]域.长度[%04d]=[%s]",nId, ((MSGCTX *)(pCtx->preBuf.ptr))->flds[nId-1].length, retrino_37);

		    nId = 41;
		    memset(accptermid_41, 0x00, sizeof(accptermid_41));
		    if (nId <= 0 || nId > NMAXBITS) {
		        LOG_ERR("域ID值[%d]不正确", nId);
		        continue;
		    }
		    if ((len = tpMsgCtxGet(pCtx->preBuf.ptr, nId-1, accptermid_41, 100)) < 0) {
		        LOG_ERR("tpMsgCtxGet(%d) error", nId-1);
		        continue;
		    }
		    LOG_MSG("报文[%03d]域.长度[%04d]=[%s]",nId, ((MSGCTX *)(pCtx->preBuf.ptr))->flds[nId-1].length, accptermid_41);

		    nId = 42;
		    memset(cardcode_42, 0x00, sizeof(cardcode_42));
		    if (nId <= 0 || nId > NMAXBITS) {
		        LOG_ERR("域ID值[%d]不正确", nId);
		        continue;
		    }
		    if ((len = tpMsgCtxGet(pCtx->preBuf.ptr, nId-1, cardcode_42, 100)) < 0) {
		        LOG_ERR("tpMsgCtxGet(%d) error", nId-1);
		        continue;
		    }
		    LOG_MSG("报文[%03d]域.长度[%04d]=[%s]",nId, ((MSGCTX *)(pCtx->preBuf.ptr))->flds[nId-1].length, cardcode_42);

		    nId = 100;
		    memset(reinstcode_100, 0x00, sizeof(reinstcode_100));
		    if (nId <= 0 || nId > NMAXBITS) {
		        LOG_ERR("域ID值[%d]不正确", nId);
		        continue;
		    }
		    if ((len = tpMsgCtxGet(pCtx->preBuf.ptr, nId-1, reinstcode_100, 100)) < 0) {
		        LOG_ERR("tpMsgCtxGet(%d) error", nId-1);
		        continue;
		    }
		    LOG_MSG("报文[%03d]域.长度[%04d]=[%s]",nId, ((MSGCTX *)(pCtx->preBuf.ptr))->flds[nId-1].length, reinstcode_100);

		    memset(typ_procode_concode, 0x00, sizeof(typ_procode_concode));
	        sprintf(typ_procode_concode,"%4s%6s%2s", type_1, procode_3, condcode_25);
			LOG_MSG("报文交易类型=[%s]", typ_procode_concode);
/*
消费     	 020000000000
消费冲正 	 042000000000
消费撤销 	 020020000000
消费撤销冲正 042020000000
联机退货     022020000000
*/
			if( strncmp(typ_procode_concode, "020000000000", 12) !=0 &&
				strncmp(typ_procode_concode, "042000000000", 12) !=0 &&
				strncmp(typ_procode_concode, "020020000000", 12) !=0 &&
				strncmp(typ_procode_concode, "042020000000", 12) !=0 &&
				strncmp(typ_procode_concode, "022020000000", 12) !=0   ) {
				LOG_ERR("报文交易类型=[%s]不符合要求", typ_procode_concode);
				continue;
			}
	        tpPreBufFree(&tpCtx.preBuf);
			/********************************************* add  by Yuan.y 2015年11月19日18:08:23 END  */

            if ((ret = mq_post(mqh, NULL, m_des_q, 0, NULL, (char *)&tpMsg, msgLen, 0)) != 0) {
                LOG_ERR("mq_post() error: retcode=%d", ret);
                LOG_ERR("写队列[%d]出错",m_des_q);
            }
            LOG_MSG("转发报文[%d]==>[%d]...[ok]", q,m_des_q);
            continue;
        }
        else if (m_des_q == q) {
            /* 初始化报文头 */
            if(m_tphead == 1)
            {
                if (tpNewHead(m_tpTask.bindQ, &tpMsg.head) != 0) {
                    LOG_ERR("初始化报文头出错");
                    continue;
                }
                LOG_MSG("初始化报文头...[ok]");
            }
            /* 不初始化报文头 */
            else
            {
                tpMsg.head.orgQ = m_src_q;
                tpMsg.head.desQ = m_tpTask.bindQ;
                tpMsg.head.msgType = MSGAPP;
            }

            if (hasLogLevel(DEBUG)) tpHeadDebug(&tpMsg.head);

            tpMsg.head.bodyLen = msgLen - sizeof(TPHEAD);
            if ((ret = mq_post(mqh, NULL, m_src_q, 0, NULL, (char *)&tpMsg, msgLen, 0)) != 0) {
                LOG_ERR("mq_post() error: retcode=%d", ret);
                LOG_ERR("写队列[%d]出错",m_src_q);
            }
            LOG_MSG("转发报文[%d]==>[%d]...[ok]", q, m_src_q);
            continue;
        }
        else {
            LOG_ERR("报文来源队列[%d]非法 ",q);
            continue;
        }
    }
}

