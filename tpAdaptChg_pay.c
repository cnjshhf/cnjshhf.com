/**************************************************************
 * ����ת����������������Դ�����������Ӧ����һ��ͨѶ��·ʱ
 *************************************************************/
#include "queue.h"
#include "tpbus.h"

TPTASK m_tpTask;


int  m_src_q;                  /*��Դ����*/
int  m_des_q;                  /*Ŀ�Ķ���*/         /*add by dwxiong 2014-11-2 14:43:20*/
int  m_nMaxThds;               /*��󲢷���*/
int  m_tphead;                 /*�Ƿ��ʼ������ͷ*/ /*add by dwxiong 2015-9-18 15:11:24*/

int   tpParamInit();
void *tpProcessThread(void *arg);

/**************************************************************
 * ������
 *************************************************************/
int main(int argc, char *argv[])
{
    int  i,ret;

    /*��������в���*/
    if (argc != 2) {
        printf("Usage: %s <����ID>\n", argv[0]);
        return -1;
    }

    /*��ʼ������*/
    if (tpTaskInit(atoi(argv[1]), "tpAdaptChg", &m_tpTask) != 0) {
        printf("��ʼ�����̳���\n");
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

    /*��ʼ�������̳߳�*/
    for (i=0; i<m_nMaxThds; i++) {
        if (tpThreadCreate(tpProcessThread, NULL) != 0) {
            LOG_ERR("���������߳�ʧ��: %s", strerror(errno));
            return -1;
        }
    }

    while (1) {
        sleep(100);
    }
}

/***************************************************************
 * ��ʼ�����ò���
 **************************************************************/
int tpParamInit()
{
    if (tpGetParamInt(m_tpTask.params, "src_q", &m_src_q) != 0) m_src_q = Q_SVR;
    LOG_MSG("����[src_q]=[%d]", m_src_q);

    if (tpGetParamInt(m_tpTask.params, "des_q", &m_des_q) != 0) m_des_q = Q_SVR;
    LOG_MSG("����[des_q]=[%d]", m_des_q);

    if (tpGetParamInt(m_tpTask.params, "tphead", &m_tphead) != 0) m_tphead = 1;
    LOG_MSG("����[tphead]=[%d]", m_tphead);

    if (tpGetParamInt(m_tpTask.params, "nMaxThds", &m_nMaxThds) != 0) m_nMaxThds = 5;
    LOG_MSG("����[nMaxThds]=[%d]", m_nMaxThds);

    return 0;
}

/***************************************************************
 * �����߳�
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

    /*��������������*/
    tpCtxSetTSD(&tpCtx);


    /*�󶨶���*/
    if ((ret = mq_bind(m_tpTask.bindQ, &mqh)) != 0) {
        LOG_ERR("mq_bind() error: retcode=%d", ret);
        LOG_ERR("�󶨶���[%d]����", m_tpTask.bindQ);
        tpThreadExit();
        return NULL;
    }

    while (1) {
        msgLen = sizeof(TPMSG);
        memset(&tpMsg, 0x00, sizeof(tpMsg));
        LOG_MSG("������");
        /*�����������*/
        if ((ret = mq_pend(mqh, NULL, &q, NULL, NULL, (char *)&tpMsg, &msgLen, 0)) != 0) {
            LOG_ERR("mq_pend() error: retcode=%d", ret);
            LOG_ERR("������[%d]����", m_tpTask.bindQ);
            mq_detach(mqh);
            tpThreadExit();
            return NULL;
        }
		LOG_MSG("�õ�����");



        LOG_MSG("�յ����Զ���[%d]�ı���", q);
        if (hasLogLevel(DEBUG)) tpHeadDebug(&tpMsg.head);
        if (hasLogLevel(DEBUG)) LOG_HEX(tpMsg.body, tpMsg.head.bodyLen);

        /* if (m_src_q == q) { */           /*mod by dwxiong 2014-11-8 13:42:34*/
        if (m_src_q == q || Q_REV == q ) {

			/********************************************* add  by Yuan.y 2015��11��19��18:08:23 START*/
			memcpy(&tpCtx.tpMsg, &tpMsg, sizeof(TPMSG));

			tpCtx.msgCtx = NULL;
	        tpPreBufInit(&tpCtx.preBuf, NULL);

	        LOG_MSG("���ױ�����Դ[%d]",  tpCtx.tpMsg.head.orgQ);

	        LOG_MSG("������[%d]��Դ[%d]",  m_tpTask.bindQ, q);
			/*��ȡ�˵�����*/
	        /*if (!(ptpPort = tpPortShmGet(300 , NULL))) {
	        */
	        if (!(ptpPort = tpPortShmGet(tpCtx.tpMsg.head.orgQ , NULL))) {
	            LOG_ERR("��ȡ�˵�[%d]���ó���", ptpHead->orgQ);
	            continue;
	        }

	        /*����Ԥ���*/
	        if (tpPreUnpack(&tpCtx, &ptpPort->fmtDefs) != 0) {
	            LOG_ERR("����Ԥ�������");
	            continue;
	        }
	        LOG_MSG("����Ԥ���...[ok]");

	    	memset(result, 0x00, sizeof(result));

		    pCtx = tpCtxGetTSD();
		    if (!pCtx || !pCtx->preBuf.ptr) {
		        LOG_ERR("�����Ĳ���ȷ");
		        continue;
		    }

			LOG_MSG("��ֵ�洢��[%s]",((MSGCTX *)(pCtx->preBuf.ptr))->buf);
			LOG_MSG("��ֵ�洢��[%d]",((MSGCTX *)(pCtx->preBuf.ptr))->flds[24].offset);

			for(i=0;i<MAXFLDNUM;i++){
				if( 0 != ((MSGCTX *)(pCtx->preBuf.ptr))->flds[i].length )
				LOG_MSG("��ID[%03d]����[%20s] ��ֵƫ����[%03d]  ��ֵ����[%03d] ��ֵ�ռ��С[%03d] ��һͬ��ֵ��[%03d]",
				i+1,
				((MSGCTX *)(pCtx->preBuf.ptr))->flds[i].name   ,
				((MSGCTX *)(pCtx->preBuf.ptr))->flds[i].offset ,
				((MSGCTX *)(pCtx->preBuf.ptr))->flds[i].length ,
				((MSGCTX *)(pCtx->preBuf.ptr))->flds[i].size   ,
				((MSGCTX *)(pCtx->preBuf.ptr))->flds[i].next
				);
			}

			/*
			char   name[MAXKEYLEN+1];   ����
	        int    offset;              ��ֵƫ����
	        int    length;              ��ֵ����
	        int    size;                ��ֵ�ռ��С
	        short  next;                ��һͬ��ֵ��
			*/
		    nId = 1;
		    memset(type_1, 0x00, sizeof(type_1));
		    if (nId <= 0 || nId > NMAXBITS) {
		        LOG_ERR("��IDֵ[%d]����ȷ", nId);
		        continue;
		    }
		    if ((len = tpMsgCtxGet(pCtx->preBuf.ptr, nId-1, type_1, 100)) < 0) {
		        LOG_ERR("tpMsgCtxGet(%d) error", nId-1);
		        continue;
		    }
		    LOG_MSG("����[%03d]��.����[%04d]=[%s]",nId, ((MSGCTX *)(pCtx->preBuf.ptr))->flds[nId-1].length, type_1);

		    nId = 2;
		    memset(acctno_2, 0x00, sizeof(acctno_2));
		    if (nId <= 0 || nId > NMAXBITS) {
		        LOG_ERR("��IDֵ[%d]����ȷ", nId);
		        continue;
		    }
		    if ((len = tpMsgCtxGet(pCtx->preBuf.ptr, nId-1, acctno_2, 100)) < 0) {
		        LOG_ERR("tpMsgCtxGet(%d) error", nId-1);
		        continue;
		    }
		    LOG_MSG("����[%03d]��.����[%04d]=[%s]",nId, ((MSGCTX *)(pCtx->preBuf.ptr))->flds[nId-1].length, acctno_2);

		    nId = 3;
		    memset(procode_3, 0x00, sizeof(procode_3));
		    if (nId <= 0 || nId > NMAXBITS) {
		        LOG_ERR("��IDֵ[%d]����ȷ", nId);
		        continue;
		    }
		    if ((len = tpMsgCtxGet(pCtx->preBuf.ptr, nId-1, procode_3, 100)) < 0) {
		        LOG_ERR("tpMsgCtxGet(%d) error", nId-1);
		        continue;
		    }
		    LOG_MSG("����[%03d]��.����[%04d]=[%s]",nId, ((MSGCTX *)(pCtx->preBuf.ptr))->flds[nId-1].length, procode_3);

		    nId = 25;
		    memset(condcode_25, 0x00, sizeof(condcode_25));
		    if (nId <= 0 || nId > NMAXBITS) {
		        LOG_ERR("��IDֵ[%d]����ȷ", nId);
		        continue;
		    }
		    if ((len = tpMsgCtxGet(pCtx->preBuf.ptr, nId-1, condcode_25, 100)) < 0) {
		        LOG_ERR("tpMsgCtxGet(%d) error", nId-1);
		        continue;
		    }
		    LOG_MSG("����[%03d]��.����[%04d]=[%s]",nId, ((MSGCTX *)(pCtx->preBuf.ptr))->flds[nId-1].length, condcode_25);

		    nId = 32;
		    memset(ainstcode_32, 0x00, sizeof(ainstcode_32));
		    if (nId <= 0 || nId > NMAXBITS) {
		        LOG_ERR("��IDֵ[%d]����ȷ", nId);
		        continue;
		    }
		    if ((len = tpMsgCtxGet(pCtx->preBuf.ptr, nId-1, ainstcode_32, 100)) < 0) {
		        LOG_ERR("tpMsgCtxGet(%d) error", nId-1);
		        continue;
		    }
		    LOG_MSG("����[%03d]��.����[%04d]=[%s]",nId, ((MSGCTX *)(pCtx->preBuf.ptr))->flds[nId-1].length, ainstcode_32);

		    nId = 33;
		    memset(finstcode_33, 0x00, sizeof(finstcode_33));
		    if (nId <= 0 || nId > NMAXBITS) {
		        LOG_ERR("��IDֵ[%d]����ȷ", nId);
		        continue;
		    }
		    if ((len = tpMsgCtxGet(pCtx->preBuf.ptr, nId-1, finstcode_33, 100)) < 0) {
		        LOG_ERR("tpMsgCtxGet(%d) error", nId-1);
		        continue;
		    }
		    LOG_MSG("����[%03d]��.����[%04d]=[%s]",nId, ((MSGCTX *)(pCtx->preBuf.ptr))->flds[nId-1].length, finstcode_33);

		    nId = 37;
		    memset(retrino_37, 0x00, sizeof(retrino_37));
		    if (nId <= 0 || nId > NMAXBITS) {
		        LOG_ERR("��IDֵ[%d]����ȷ", nId);
		        continue;
		    }
		    if ((len = tpMsgCtxGet(pCtx->preBuf.ptr, nId-1, retrino_37, 100)) < 0) {
		        LOG_ERR("tpMsgCtxGet(%d) error", nId-1);
		        continue;
		    }
		    LOG_MSG("����[%03d]��.����[%04d]=[%s]",nId, ((MSGCTX *)(pCtx->preBuf.ptr))->flds[nId-1].length, retrino_37);

		    nId = 41;
		    memset(accptermid_41, 0x00, sizeof(accptermid_41));
		    if (nId <= 0 || nId > NMAXBITS) {
		        LOG_ERR("��IDֵ[%d]����ȷ", nId);
		        continue;
		    }
		    if ((len = tpMsgCtxGet(pCtx->preBuf.ptr, nId-1, accptermid_41, 100)) < 0) {
		        LOG_ERR("tpMsgCtxGet(%d) error", nId-1);
		        continue;
		    }
		    LOG_MSG("����[%03d]��.����[%04d]=[%s]",nId, ((MSGCTX *)(pCtx->preBuf.ptr))->flds[nId-1].length, accptermid_41);

		    nId = 42;
		    memset(cardcode_42, 0x00, sizeof(cardcode_42));
		    if (nId <= 0 || nId > NMAXBITS) {
		        LOG_ERR("��IDֵ[%d]����ȷ", nId);
		        continue;
		    }
		    if ((len = tpMsgCtxGet(pCtx->preBuf.ptr, nId-1, cardcode_42, 100)) < 0) {
		        LOG_ERR("tpMsgCtxGet(%d) error", nId-1);
		        continue;
		    }
		    LOG_MSG("����[%03d]��.����[%04d]=[%s]",nId, ((MSGCTX *)(pCtx->preBuf.ptr))->flds[nId-1].length, cardcode_42);

		    nId = 100;
		    memset(reinstcode_100, 0x00, sizeof(reinstcode_100));
		    if (nId <= 0 || nId > NMAXBITS) {
		        LOG_ERR("��IDֵ[%d]����ȷ", nId);
		        continue;
		    }
		    if ((len = tpMsgCtxGet(pCtx->preBuf.ptr, nId-1, reinstcode_100, 100)) < 0) {
		        LOG_ERR("tpMsgCtxGet(%d) error", nId-1);
		        continue;
		    }
		    LOG_MSG("����[%03d]��.����[%04d]=[%s]",nId, ((MSGCTX *)(pCtx->preBuf.ptr))->flds[nId-1].length, reinstcode_100);

		    memset(typ_procode_concode, 0x00, sizeof(typ_procode_concode));
	        sprintf(typ_procode_concode,"%4s%6s%2s", type_1, procode_3, condcode_25);
			LOG_MSG("���Ľ�������=[%s]", typ_procode_concode);
/*
����     	 020000000000
���ѳ��� 	 042000000000
���ѳ��� 	 020020000000
���ѳ������� 042020000000
�����˻�     022020000000
*/
			if( strncmp(typ_procode_concode, "020000000000", 12) !=0 &&
				strncmp(typ_procode_concode, "042000000000", 12) !=0 &&
				strncmp(typ_procode_concode, "020020000000", 12) !=0 &&
				strncmp(typ_procode_concode, "042020000000", 12) !=0 &&
				strncmp(typ_procode_concode, "022020000000", 12) !=0   ) {
				LOG_ERR("���Ľ�������=[%s]������Ҫ��", typ_procode_concode);
				continue;
			}
	        tpPreBufFree(&tpCtx.preBuf);
			/********************************************* add  by Yuan.y 2015��11��19��18:08:23 END  */

            if ((ret = mq_post(mqh, NULL, m_des_q, 0, NULL, (char *)&tpMsg, msgLen, 0)) != 0) {
                LOG_ERR("mq_post() error: retcode=%d", ret);
                LOG_ERR("д����[%d]����",m_des_q);
            }
            LOG_MSG("ת������[%d]==>[%d]...[ok]", q,m_des_q);
            continue;
        }
        else if (m_des_q == q) {
            /* ��ʼ������ͷ */
            if(m_tphead == 1)
            {
                if (tpNewHead(m_tpTask.bindQ, &tpMsg.head) != 0) {
                    LOG_ERR("��ʼ������ͷ����");
                    continue;
                }
                LOG_MSG("��ʼ������ͷ...[ok]");
            }
            /* ����ʼ������ͷ */
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
                LOG_ERR("д����[%d]����",m_src_q);
            }
            LOG_MSG("ת������[%d]==>[%d]...[ok]", q, m_src_q);
            continue;
        }
        else {
            LOG_ERR("������Դ����[%d]�Ƿ� ",q);
            continue;
        }
    }
}

