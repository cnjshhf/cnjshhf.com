/***************************************************************
 * ҵ�����������
 **************************************************************/
#include "tpbus.h"

/***************************************************************
 * ������
 **************************************************************/
int main(int argc, char *argv[])
{
    int    q,msgLen,ret;
    char   logPrefix[21];
    MQHDL  *mqh = NULL;
    DBHDL  *dbh = NULL;
    TPCTX  tpCtx;
    TPTRAN tpTran,*ptpTran;
    TPHEAD *ptpHead = &tpCtx.tpMsg.head;

    /*��������в���*/
    if (argc != 2) {
        printf("Usage: %s <����ID>\n", argv[0]);
        return -1;
    }

    /*��ʼ������*/
    if (tpTaskInit(argv[1], "tpBizProc", NULL) != 0) { 
        LOG_ERR("��ʼ�����̳���");
        return -1;
    }

    /*��ʼ������*/
    if ((ret = mq_init()) != 0) {
        LOG_ERR("mq_init() error: retcode=%d", ret);
        LOG_ERR("��ʼ�����г���");
        return -1;
    }

    /*�󶨶���*/
    if ((ret = mq_bind(Q_BIZ, &mqh)) != 0) {
        LOG_ERR("mq_bind() error: retcode=%d", ret);
        LOG_ERR("�󶨶���[%d]����", Q_BIZ);
        return -1;
    }

    /*��������������*/
    tpCtxSetTSD(&tpCtx);

    while (1) {
        msgLen = sizeof(TPHEAD);
        if ((ret = mq_pend(mqh, NULL, &q, NULL, NULL, (char *)ptpHead, &msgLen, 0)) != 0) {
            LOG_ERR("mq_pend() error: retcode=%d", ret);
            LOG_ERR("������[%d]����", Q_BIZ);
            break;
        }
        tpSetLogPrefix(ptpHead->tpId, logPrefix);
        LOG_MSG("�յ����Զ���[%d]�ı���", q);
        if (hasLogLevel(DEBUG)) tpHeadDebug(ptpHead);

        switch (ptpHead->msgType) {
            case MSGAPP:
                /*����������*/
                ptpTran = &(gShm.pShmTran[ptpHead->shmIndex-1].tpTran);
                break;
            case MSGREV:
                /*��ȡ������*/
                if (tpTranCtxLoad(ptpHead->tpId, &tpTran) != 0) {
                    LOG_ERR("��ȡ������[tpId=%d]����", ptpHead->tpId);
                    break;
                }
                ptpTran = &tpTran;
            default:
                LOG_ERR("��������[%d]����ȷ", ptpHead->msgType);
                continue;
        }
        tpCtx.msgCtx = &ptpTran->msgCtx;

        /*ҵ�����߼�(��ʼ)*/

          /*��ȡ�����ĺ���*/
          int tpMsgCtxHashGet(MSGCTX *msgCtx, char *key, char *buf, int size);

          /*д�������ĺ���*/
          int tpMsgCtxHashPut(MSGCTX *msgCtx, char *key, char *buf, int len);

          /*ִ�нű�����*/
          int tpFunExec(char *express, char *result, int size);        

        /*ҵ�����߼�(����)*/

        if (ptpHead->msgType == MSGAPP) { /*��������*/
            /*д���ض���*/
            if ((ret = mq_post(mqh, NULL, Q_SVR, 0, NULL, (char *)ptpHead, sizeof(TPHEAD), 0)) != 0) {
                LOG_ERR("mq_post() error: retcode=%d", ret);
                LOG_ERR("д���ض���[%d]����", Q_SVR);
            }
            LOG_MSG("д���ض���[%d]...[ok]", Q_SVR);
        } else { /*��������*/
            /*����������*/
            if (tpTranCtxSave(ptpHead->tpId, ptpTran) != 0) {
                LOG_ERR("���������ĳ���");
                continue;
            }
 
            /*д�������*/
            if ((ret = mq_post(mqh, NULL, Q_REV, 0, NULL, (char *)ptpHead, sizeof(TPHEAD), 0)) != 0) {
                LOG_ERR("mq_post() error: retcode=%d", ret);
                LOG_ERR("д�������[%d]����", Q_REV);
            }
            LOG_MSG("д�������[%d]...[ok]", Q_REV);
        }
    }
    return 0;
}
