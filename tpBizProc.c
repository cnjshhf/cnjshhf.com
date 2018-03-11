/***************************************************************
 * 业务处理组件程序
 **************************************************************/
#include "tpbus.h"

/***************************************************************
 * 主函数
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

    /*检查命令行参数*/
    if (argc != 2) {
        printf("Usage: %s <任务ID>\n", argv[0]);
        return -1;
    }

    /*初始化进程*/
    if (tpTaskInit(argv[1], "tpBizProc", NULL) != 0) { 
        LOG_ERR("初始化进程出错");
        return -1;
    }

    /*初始化队列*/
    if ((ret = mq_init()) != 0) {
        LOG_ERR("mq_init() error: retcode=%d", ret);
        LOG_ERR("初始化队列出错");
        return -1;
    }

    /*绑定队列*/
    if ((ret = mq_bind(Q_BIZ, &mqh)) != 0) {
        LOG_ERR("mq_bind() error: retcode=%d", ret);
        LOG_ERR("绑定队列[%d]出错", Q_BIZ);
        return -1;
    }

    /*设置上下文引用*/
    tpCtxSetTSD(&tpCtx);

    while (1) {
        msgLen = sizeof(TPHEAD);
        if ((ret = mq_pend(mqh, NULL, &q, NULL, NULL, (char *)ptpHead, &msgLen, 0)) != 0) {
            LOG_ERR("mq_pend() error: retcode=%d", ret);
            LOG_ERR("读队列[%d]出错", Q_BIZ);
            break;
        }
        tpSetLogPrefix(ptpHead->tpId, logPrefix);
        LOG_MSG("收到来自队列[%d]的报文", q);
        if (hasLogLevel(DEBUG)) tpHeadDebug(ptpHead);

        switch (ptpHead->msgType) {
            case MSGAPP:
                /*引用上下文*/
                ptpTran = &(gShm.pShmTran[ptpHead->shmIndex-1].tpTran);
                break;
            case MSGREV:
                /*提取上下文*/
                if (tpTranCtxLoad(ptpHead->tpId, &tpTran) != 0) {
                    LOG_ERR("提取上下文[tpId=%d]出错", ptpHead->tpId);
                    break;
                }
                ptpTran = &tpTran;
            default:
                LOG_ERR("报文类型[%d]不正确", ptpHead->msgType);
                continue;
        }
        tpCtx.msgCtx = &ptpTran->msgCtx;

        /*业务处理逻辑(开始)*/

          /*读取上下文函数*/
          int tpMsgCtxHashGet(MSGCTX *msgCtx, char *key, char *buf, int size);

          /*写入上下文函数*/
          int tpMsgCtxHashPut(MSGCTX *msgCtx, char *key, char *buf, int len);

          /*执行脚本函数*/
          int tpFunExec(char *express, char *result, int size);        

        /*业务处理逻辑(结束)*/

        if (ptpHead->msgType == MSGAPP) { /*正常报文*/
            /*写主控队列*/
            if ((ret = mq_post(mqh, NULL, Q_SVR, 0, NULL, (char *)ptpHead, sizeof(TPHEAD), 0)) != 0) {
                LOG_ERR("mq_post() error: retcode=%d", ret);
                LOG_ERR("写主控队列[%d]出错", Q_SVR);
            }
            LOG_MSG("写主控队列[%d]...[ok]", Q_SVR);
        } else { /*冲正报文*/
            /*保存上下文*/
            if (tpTranCtxSave(ptpHead->tpId, ptpTran) != 0) {
                LOG_ERR("保存上下文出错");
                continue;
            }
 
            /*写事务队列*/
            if ((ret = mq_post(mqh, NULL, Q_REV, 0, NULL, (char *)ptpHead, sizeof(TPHEAD), 0)) != 0) {
                LOG_ERR("mq_post() error: retcode=%d", ret);
                LOG_ERR("写事务队列[%d]出错", Q_REV);
            }
            LOG_MSG("写事务队列[%d]...[ok]", Q_REV);
        }
    }
    return 0;
}
