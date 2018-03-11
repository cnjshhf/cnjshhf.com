/***************************************************************
 * TPBUS�������
 **************************************************************/
const char _tpbus_c_Id[] = {"v1.0.0.0 2016/6/3"};

#include "tpLoad.c"

int tpSync(int argc, char *argv[]);
int tpInit(int argc, char *argv[]);
int tpReload(int argc, char *argv[]);
int tpStart(int argc, char *argv[]);
int tpStatus(int argc, char *argv[]);
int tpShow(int argc, char *argv[]);
int tpStop(int argc, char *argv[]);
int tpShutdown(int argc, char *argv[]);

int tpShmInit();
int tpShmReload();
int tpShmFree();
int tpShmExist();

/***************************************************************
 * ������
 *1��ȡ��������tpbus_home
 **************************************************************/
int main(int argc, char *argv[])
{
    if (argc < 2) {
        printf("�÷�: tpbus sync|init|reload|start|status|show|stop|shutdown\n");
        return -1;
    }

    if (getenv("TPBUS_HOME") == NULL) {
        printf("δ���û�������[TPBUS_HOME]\n");
        return -1;
    }

    if (strcmp("sync", argv[1]) == 0) {
        return tpSync(argc, argv);
    } else if (strcmp("init", argv[1]) == 0) {
        return tpInit(argc, argv);
    } else if (strcmp("reload", argv[1]) == 0) {
        return tpReload(argc, argv);
    } else if (strcmp("start", argv[1]) == 0) {
        return tpStart(argc, argv);
    } else if (strcmp("status", argv[1]) == 0) {
        return tpStatus(argc, argv);
    } else if (strcmp("show", argv[1]) == 0) {
        return tpShow(argc, argv);
    } else if (strcmp("stop", argv[1]) == 0) {
        return tpStop(argc, argv);
    } else if (strcmp("shutdown", argv[1]) == 0) {
        return tpShutdown(argc, argv);
    } else {
        printf("�÷�: tpbus sync|init|reload|start|status|show|stop|shutdown\n");
        return -1;
    }
    return 0;
}

/***************************************************************
 * ͬ��������Ϣ
 *1 ȡ��������tpbus_webhost
 *2 ��д���궨��ת��
 *3 
 *���԰����� demo01.c,�ļ��򿪲���ʵ��
 *demo02.c��ͨ���ļ���ʵ��ͬ��������^@
 **************************************************************/
int tpSync(int argc, char *argv[])
{
    int fd,port=0;
    char *host,lockFile[256];

    tpSetLogFile("tpSync.log");

    if (!(host = getenv("TPBUS_WEBHOST"))) {
        LOG_ERR("δ���û�������[TPBUS_WEBHOST]");
        return -1;
    }
    if (getenv("TPBUS_WEBPORT")) port = atoi(getenv("TPBUS_WEBPORT"));
    if (!port) port = 7086;

    snprintf(lockFile, sizeof(lockFile), "%s/etc/sync/sync_lock", getenv("TPBUS_HOME"));
    if ((fd = open(lockFile, O_RDWR|O_EXCL|O_CREAT|O_TRUNC)) == -1) {
        if (errno != EEXIST) {
            LOG_ERR("open() error: %s", strerror(errno));
            LOG_ERR("����ͬ���������ļ�[%s]����", lockFile);
            printf("ִ������ʧ��\n");
            return -1;
        }
        if ((fd = open(lockFile, O_RDWR)) == -1) {
            LOG_ERR("open() error: %s", strerror(errno));
            LOG_ERR("��ͬ���������ļ�[%s]����", lockFile);
            printf("ִ������ʧ��\n");
            return -1;
        }
    }
    if (write_lock(fd, 0, 0) == -1) {
        printf("ͬ���������ڱ�������ִ��\n");
        close(fd);
        return -1;
    }

    if (tpConfSync(host, port) != 0) {
        release_lock(fd, 0, 0);
        printf("ִ������ʧ��\n");
        close(fd);
        return -1;
    }
    release_lock(fd, 0, 0);
    printf("ִ������ɹ�\n");
    close(fd);
    return 0;
}

/***************************************************************
 * ��ʼ�������ڴ�
 **************************************************************/
int tpInit(int argc, char *argv[])
{
    tpSetLogFile("tpInit.log");

    if (tpShmExist()) {
        printf("�����ڴ��Ѿ���ʼ��\n");
        return -1;
    }
    if (tpShmInit() != 0) {
        printf("ִ������ʧ��\n");
        return -1;
    }
    printf("ִ������ɹ�\n");
    return 0;
}

/***************************************************************
 * ����װ�ع����ڴ�
 **************************************************************/
int tpReload(int argc, char *argv[])
{
    tpSetLogFile("tpReload.log");

    if (!tpShmExist()) {
        printf("�����ڴ���δ��ʼ��\n");
        return -1;
    }
    if (tpShmTouch() != 0) {
        LOG_ERR("���ӹ����ڴ����");
        printf("ִ������ʧ��\n");
        return -1;
    }
    if (gShm.pShmHead->nActive > 0) {
        printf("ƽ̨�Դ��ڻ״̬,�Ƿ�ǿ��ִ��(y/n)?");
        switch (getchar()) {
            case 'y':
            case 'Y':
                break;
            default:
                return -1;
        }                
    }
    if (tpShmReload() != 0) {
        printf("ִ������ʧ��\n");
        return -1;
    }
    printf("ִ������ɹ�\n");
    return 0;
}

/***************************************************************
 * ����ƽ̨������
 **************************************************************/
int tpStart(int argc, char *argv[])
{
    int   ret,len;
    int   taskId=0;
    char  buf[256],clazz[MSGCLZLEN+1];
    pid_t pid;
    time_t t;
    MQHDL *mqh;
    TPMSG tpMsg;

    tpSetLogFile("tpStart.log");

    if (argc > 3) {
        printf("�÷�: tpbus start [����ID]\n");
        return -1;
    }
    if (argc == 3) {
        if ((taskId = atoi(argv[2])) <= 0) {
            printf("��Ч������ID");
            return -1;
        }
    }

    if (!tpShmExist()) {
        if (taskId) {
            printf("ƽ̨��δ����\n");
            return -1;
        } else {
            if (tpShmInit() != 0) {
                LOG_ERR("��ʼ�������ڴ����");
                printf("ִ������ʧ��\n");
                return -1;
            }
            LOG_MSG("��ʼ�������ڴ�ok");
        }
    }
        
    if (tpShmTouch() != 0) {
        LOG_ERR("���ӹ����ڴ����");
        printf("ִ������ʧ��\n");
        return -1;
    }

    if (taskId) {
         if (tpGetPid("tpTaskMng") < 0) {
            printf("ƽ̨��δ����\n");
            return -1;
        }
    } else {
        if (tpGetPid("tpTaskMng") > 0) {
            printf("ƽ̨�Ѿ�����\n");
            return 0;
        }
        signal(SIGCLD, SIG_IGN);

        if ((pid = fork()) == -1) {
            LOG_ERR("fork() error: %s", strerror(errno));
            LOG_ERR("�������������ʧ��");
            printf("ִ������ʧ��\n");
            return -1;
        } else if (pid == 0) {
            snprintf(buf, sizeof(buf), "%s/bin/tpTaskMng", getenv("TPBUS_HOME"));
            execl(buf, "tpTaskMng", (char *)0);
            return 0;
        } else {
            sleep(1);
            if (kill(pid, 0) != 0) {
                LOG_ERR("�������������ʧ��");
                printf("ִ������ʧ��\n");
                return -1;
            }
        }
    }

    /*��ʼ������*/
    if ((ret = mq_init()) != 0) {
        LOG_ERR("mq_init() error: retcode=%d", ret);
        printf("ִ������ʧ��\n");
        return -1;
    }
        
    /*����ʱ����*/
    if ((ret = mq_bind(Q_TMP, &mqh)) != 0) {
        LOG_ERR("mq_bind(%d) error: retcode=%d", Q_TMP, ret);
        printf("ִ������ʧ��\n");
        return -1;
    }

    /*׼�������*/
    memset(&tpMsg.head, 0, sizeof(TPHEAD));
    tpMsg.head.msgType = MSGSYS_START;
    if (!taskId) {
        tpMsg.head.bodyLen = 0;
    } else {
        snprintf(tpMsg.body, sizeof(tpMsg.body), "%d", taskId);
        tpMsg.head.bodyLen = strlen(tpMsg.body);
    }

    /*���������*/
    time(&t);
    snprintf(clazz, sizeof(clazz), "%d%ld", getpid(), t);
    len = tpMsg.head.bodyLen + sizeof(TPHEAD);
    if ((ret = mq_post(mqh, NULL, Q_TSK, 0, clazz, (char *)&tpMsg, len, 0)) != 0) {
        LOG_ERR("mq_post(%d) error: retcode=%d", Q_TSK, ret);
        printf("ִ������ʧ��\n");
        return -1;
    }
        
    /*��������ִ�н��*/
    while (1) {
        len = sizeof(TPMSG);
        if ((ret = mq_pend(mqh, NULL, NULL, NULL, clazz, (char *)&tpMsg, &len, 0)) != 0) {
            LOG_ERR("mq_pend(%d) error: retcode=%d", Q_TMP, ret);
            printf("ִ������ʧ��\n");
            return -1;
        }
        if (tpMsg.head.bodyLen == 0) break;
        tpMsg.body[tpMsg.head.bodyLen] = 0;
        printf("%s\n", tpMsg.body);
    }
    printf("ִ������ɹ�\n");
    return 0;
}

/***************************************************************
 * tpTaskStatus
 **************************************************************/
void tpTaskStatus()
{
    int  i,j,k;
    char szTime[21];
    char szStat[21];
    char *p,szFile[256];

    printf("%-6.6s  %-20.20s  %-8.8s  %-15.15s  %-8.8s  %s  %s\n",
           "����ID", "��������", "�󶨶���", "ִ�г���", "���̺�", "��ͣʱ��", "״̬");
    printf("---------------------------------------------------------------------------------------\n");
    for (i=0; i<gShm.pShmList->tpTask.nItems; i++) {
        if (!gShm.ptpTask[i].taskId) continue;
        if (gShm.ptpTask[i].disabled) continue;

        p = gShm.ptpTask[i].pathFile;
        for (k=0; p[k] != '\0' && p[k] != ' '; k++) {}
        for (j=--k; j>=0 && p[j] != '/'; j--) {}
        memcpy(szFile, p+j+1, k-j);
        szFile[k-j] = 0; 

        if (!gShm.ptpTask[i].startTime && !gShm.ptpTask[i].stopTime) {
            strcpy(szStat, "δ����");
            strcpy(szTime, "--:--:--");
        } else {
            if (gShm.ptpTask[i].pid > 0) {
                strcpy(szStat, "������");
                getTimeString(gShm.ptpTask[i].startTime, "hh:mm:ss", szTime);
            } else {
                strcpy(szStat, "��ֹͣ");
                getTimeString(gShm.ptpTask[i].stopTime, "hh:mm:ss", szTime);
            }
        }

        printf("%-6d  %-20.20s  %8d  %-15.15s  %8d  %s  %s\n",
                gShm.ptpTask[i].taskId,
                gShm.ptpTask[i].taskName,  
                gShm.ptpTask[i].bindQ,
                szFile,             
                gShm.ptpTask[i].pid,  
                szTime,
                szStat);
    }
    printf("---------------------------------------------------------------------------------------\n");
}

/***************************************************************
 * tpServStatus
 **************************************************************/
void tpServStatus()
{
    int i,h;

    printf("%-10.10s  %-25.25s  %-10.10s  %-10.10s  %-10.10s\n",
           "�������", "��������", "���������", "����������", "���ڳ�����");
    printf("-------------------------------------------------------------------------\n");

    for (h=1; h<=HASH_SIZE; h++) {
        i = h;
        while (i>0 && gShm.ptpServ[i-1].servCode[0]) {
            printf("%-10.10s  %-25.25s  %10d  %10ld  %10ld\n",  
                   gShm.ptpServ[i-1].servCode,
                   gShm.ptpServ[i-1].servName,
                   gShm.ptpServ[i-1].nMaxReq,
                   gShm.ptpServ[i-1].nActive,
                   gShm.ptpServ[i-1].nReving);

            i = gShm.ptpServ[i-1].next;
        }
    }

    printf("-------------------------------------------------------------------------\n");
}

/*XJJ*/
void tpShmStatus()
{
    int i,h;
  /* 
    i = gShm.pShmLink->tpTran.head;
    while (i) {
        if (gShm.pShmTran[i-1].tag != 1) {
            i = gShm.pShmTran[i-1].next;
            continue;
        }

            printf("%2d %-10.10d  %10d  %15.15s  %15.15s %2d %15ld\n",
                   gShm.pShmTran[i-1].tag,
                   gShm.pShmTran[i-1].tpTran.tpLog.tpId,
                   gShm.pShmTran[i-1].tpTran.tpLog.headId,
                   gShm.pShmTran[i-1].tpTran.tpLog.servCode,
                   gShm.pShmTran[i-1].tpTran.tpLog.procCode,
                   gShm.pShmTran[i-1].tpTran.tpLog.status,
                   gShm.pShmTran[i-1].tpTran.tpLog.overTime);

  

    }
*/
  
    for (h=1; h<=HASH_SIZE; h++) {
        i = h;
        while (i>0 && gShm.pShmTran[i-1].tag > 0) {
            printf("%2d %-10.10d  %10d  %15.15s  %15.15s %2d\n",
                   gShm.pShmTran[i-1].tag,
                   gShm.pShmTran[i-1].tpTran.tpLog.tpId,
                   gShm.pShmTran[i-1].tpTran.tpLog.headId,
                   gShm.pShmTran[i-1].tpTran.tpLog.servCode,
                   gShm.pShmTran[i-1].tpTran.tpLog.procCode,
                   gShm.pShmTran[i-1].tpTran.tpLog.status);

            i = gShm.pShmTran[i-1].next;
        }
    }
}
void tpHeadStatus()
{
    int  i,j,k;
    char szTime[21];
    char szStat[21];
    char *p,szFile[256];
    char buftime[32];
    struct  tm *tm;

    printf("   ID %10s %10s %10s %-48s %14s\n",
           "ǰ��ID", "����ID", "����ID", "����ͷ��ֵ", "��ʱʱ��");
    printf("-------------------------------------------------------------------------------------------------------\n");

    i = gShm.pShmLink->tpHead.head;

    while (i) {
        tm = localtime(&gShm.pShmTphd[i-1].overTime);
        sprintf(buftime,"%04d%02d%02d%02d%02d%02d",tm->tm_year+1900,tm->tm_mon+1,tm->tm_mday,tm->tm_hour,tm->tm_min,tm->tm_sec);
        printf("%5d %10d %10d %10d %-48.48s %14s\n",i,gShm.pShmTphd[i-1].prev,gShm.pShmTphd[i-1].next,gShm.pShmTphd[i-1].desQ,gShm.pShmTphd[i-1].key,buftime);
        i = gShm.pShmTphd[i-1].next;
    }

    printf("-------------------------------------------------------------------------------------------------------\n");
}

/***************************************************************
 * �鿴ƽ̨״̬
 **************************************************************/
int tpStatus(int argc, char *argv[])
{
    tpSetLogFile("tpStop.log");

    if (argc != 3) {
        printf("�÷�: tpbus status ѡ��\n");
        printf("ѡ��: -t    �鿴����״̬\n");
        printf("      -s    �鿴����״̬\n");
        printf("      -m    �鿴�ڴ�״̬\n");
        printf("      -h    �鿴����ͷ�洢��Ϣ\n");
        return -1;
    }

    if (!tpShmExist()) {
        printf("ƽ̨��δ����\n");
        return -1;
    }

    if (tpGetPid("tpTaskMng") < 0) {
        printf("ƽ̨��δ����\n");
        return -1;
    }

    if (tpShmTouch() != 0) {
        LOG_ERR("���ӹ����ڴ����");
        printf("ִ������ʧ��\n");
        return -1;
    }

    if (strcmp(argv[2], "-t") == 0) {
        tpTaskStatus();
    } else if (strcmp(argv[2], "-s") == 0) {
        tpServStatus();
    } else if (strcmp(argv[2], "-m") == 0) {
        tpShmStatus();
    } else if (strcmp(argv[2], "-h") == 0) {
        tpHeadStatus();
    } else {
        printf("�÷�: tpbus status ѡ��\n");
        printf("ѡ��: -t    �鿴����״̬\n");
        printf("      -s    �鿴����״̬\n");
        printf("      -m    �鿴�ڴ�״̬\n");
        printf("      -h    �鿴����ͷ�洢��Ϣ\n");
        return -1;
    }
    return 0;
}

/***************************************************************
 * tpLogShow
 **************************************************************/
void tpLogShow()
{
    char sql[256];
    char tranCode[MAXKEYLEN+1];
    char servCode[MAXKEYLEN+1];
    char szBeginTime[21];
    char szStat[11];
    DBHDL *dbh = NULL;
    sqlite3_stmt *pStmt;

    if (tpSQLiteOpen(&dbh) != 0) {
       LOG_ERR("�����ݿ����");
       return;
    }
  
    printf("%-9.9s  %-4.4s  %-15.15s  %-15.15s  %-8s %-8s  %s\n",
           "tpId", "orgQ", "tranCode", "servCode", "Date", "Time", "status");
    printf("----------------------------------------------------------------------------\n");

    snprintf(sql, sizeof(sql), "select * from tp_log order by tp_id, begin_time");

    if (sqlitePrepare(dbh, sql, &pStmt) != 0) {
        LOG_ERR("ִ��SQL���[%s]����", sql);
        return;
    }

    while (SQLITE_ROW == sqliteFetch(pStmt)) {
        switch (sqliteGetInt(pStmt, 5)) {
            case TP_OK:
                strncpy(szStat, "TP_OK", sizeof(szStat)-1);
                break;
            case TP_RUNING:
                strncpy(szStat, "TP_RUNING", sizeof(szStat)-1);
                break;
            case TP_TIMEOUT:
                strncpy(szStat, "TP_TIMEOUT", sizeof(szStat)-1);
                break;
            case TP_REVING:
                strncpy(szStat, "TP_REVING", sizeof(szStat)-1);
                break;
            case TP_REVFAIL:
                strncpy(szStat, "TP_REVFAIL", sizeof(szStat)-1);
                break;
            case TP_BUSY:
                strncpy(szStat, "TP_BUSY", sizeof(szStat)-1);
                break;
            case TP_ERROR:
                strncpy(szStat, "TP_ERROR", sizeof(szStat)-1);
                break;
            default:
                strncpy(szStat, "???", sizeof(szStat)-1);
                break;
        }

        printf("%-9d  %-4d  %-15.15s  %-15.15s  %-17.17s  %s\n",
               sqliteGetInt(pStmt, 0),
               sqliteGetInt(pStmt, 1),
               sqliteGetString(pStmt, 2, tranCode, sizeof(tranCode)),
               sqliteGetString(pStmt, 3, servCode, sizeof(servCode)),
               getTimeString(sqliteGetLong(pStmt, 4), "yy/MM/dd hh:mm:ss", szBeginTime),
               szStat);
    }
    sqliteRelease(pStmt);
    tpSQLiteClose(dbh);

    printf("----------------------------------------------------------------------------\n");
}

/***************************************************************
 * tpSafShow
 **************************************************************/
void tpSafShow()
{
    char sql[256];
    char servCode[MAXKEYLEN+1];
    char procCode[MAXKEYLEN+1];
    char szBeginTime[21];
    DBHDL  *dbh = NULL;
    sqlite3_stmt *pStmt;

    if (tpSQLiteOpen(&dbh) != 0) {
       LOG_ERR("�����ݿ����");
       return;
    }
  
    printf("%-9.9s  %-9.9s  %-4.4s  %-15.15s  %-15.15s  %-6.6s  %s\n",
           "safId", "tpId", "desQ", "servCode", "procCode", "safNum", "overTime");
    printf("------------------------------------------------------------------------------\n");

    snprintf(sql, sizeof(sql), "select * from tp_saf order by saf_id");

    if (sqlitePrepare(dbh, sql, &pStmt) != 0) {
        LOG_ERR("ִ��SQL���[%s]����", sql);
        return;
    }

    while (SQLITE_ROW == sqliteFetch(pStmt)) {
        printf("%-9d  %-9d  %-4d  %-15.15s  %-15.15s  %-6d  %s\n",
               sqliteGetInt(pStmt, 0),
               sqliteGetInt(pStmt, 1),
               sqliteGetInt(pStmt, 2),
               sqliteGetString(pStmt, 3, servCode, sizeof(servCode)),
               sqliteGetString(pStmt, 4, procCode, sizeof(procCode)),
               sqliteGetInt(pStmt, 5),
               getTimeString(sqliteGetLong(pStmt, 6), "hh:mm:ss", szBeginTime));
    }
    sqliteRelease(pStmt);
    tpSQLiteClose(dbh);

    printf("------------------------------------------------------------------------------\n");
}

/***************************************************************
 * �鿴��ˮ��¼
 **************************************************************/
int tpShow(int argc, char *argv[])
{
    tpSetLogFile("tpShow.log");

    if (argc != 3) {
        printf("�÷�: tpbus show ѡ��\n");
        printf("ѡ��: log    �鿴�쳣��ˮ\n");
        printf("      saf    �鿴SAF��¼\n");
        return -1;
    }

    if (strcmp(argv[2], "log") == 0) {
        tpLogShow();
    } else if (strcmp(argv[2], "saf") == 0) {
        tpSafShow();
    } else {
        printf("�÷�: tpbus show ѡ��\n");
        printf("ѡ��: log    �鿴�쳣��ˮ\n");
        printf("      saf    �鿴SAF��¼\n");
        return -1;
    }
    return 0;
}

/***************************************************************
 * ֹͣƽ̨������
 **************************************************************/
int tpStop(int argc, char *argv[])
{
    int  ret,len;
    int  taskId=0;
    char clazz[MSGCLZLEN+1];
    time_t t;
    MQHDL *mqh;
    TPMSG tpMsg;
    
    tpSetLogFile("tpStop.log");

    if (argc > 3) {
        printf("�÷�: tpbus stop [����ID]\n");
        return -1;
    }
    if (argc == 3) {
        if ((taskId = atoi(argv[2])) <= 0) {
            printf("��Ч������ID");
            return -1;
        }
    }
    
    if (!tpShmExist()) {
        printf("ƽ̨��δ����\n");
        return -1;
    }

    if (tpGetPid("tpTaskMng") < 0) {
        printf("ƽ̨��δ����\n");
        return -1;
    }

    if (tpShmTouch() != 0) {
        LOG_ERR("���ӹ����ڴ����");
        printf("ִ������ʧ��\n");
        return -1;
    }

    /*��ʼ������*/
    if ((ret = mq_init()) != 0) {
        LOG_ERR("mq_init() error: retcode=%d", ret);
        printf("ִ������ʧ��\n");
        return -1;
    }

    /*����ʱ����*/
    if ((ret = mq_bind(Q_TMP, &mqh)) != 0) {
        LOG_ERR("mq_bind(%d) error: retcode=%d", Q_TMP, ret);
        printf("ִ������ʧ��\n");
        return -1;
    }
        
    /*׼�������*/
    memset(&tpMsg.head, 0, sizeof(TPHEAD));
    tpMsg.head.msgType = MSGSYS_STOP;
    if (!taskId) {
        tpMsg.head.bodyLen = 0;
    } else {
        snprintf(tpMsg.body, sizeof(tpMsg.body), "%d", taskId);
        tpMsg.head.bodyLen = strlen(tpMsg.body);
    }

    /*���������*/
    time(&t);
    snprintf(clazz, sizeof(clazz), "%d%ld", getpid(), t);
    len = tpMsg.head.bodyLen + sizeof(TPHEAD);
    if ((ret = mq_post(mqh, NULL, Q_TSK, 0, clazz, (char *)&tpMsg, len, 0)) != 0) {
        LOG_ERR("mq_post(%d) error: retcode=%d", Q_TSK, ret);
        printf("ִ������ʧ��\n");
        return -1;
    }
        
    /*��������ִ�н��*/
    while (1) {
        len = sizeof(TPMSG);
        if ((ret = mq_pend(mqh, NULL, NULL, NULL, clazz, (char *)&tpMsg, &len, 0)) != 0) {
            LOG_ERR("mq_pend(%d) error: retcode=%d", Q_TMP, ret);
            printf("ִ������ʧ��\n");
            return -1;
        }
        if (tpMsg.head.bodyLen == 0) break;
        tpMsg.body[tpMsg.head.bodyLen] = 0;
        printf("%s\n", tpMsg.body);
    }
    printf("ִ������ɹ�\n");
    return 0;
}

/***************************************************************
 * ж�ع����ڴ�
 **************************************************************/
int tpShutdown(int argc, char *argv[])
{
    int i,pid;

    tpSetLogFile("tpShutdown.log");

    if (!tpShmExist()) {
        printf("�����ڴ���δ��ʼ��\n");
        return -1;
    }
    if (tpShmTouch() != 0) {
        /*
        LOG_ERR("���ӹ����ڴ����");
        printf("ִ������ʧ��\n");
        return -1;
		*/
        tpShmFree();
        printf("ִ������ɹ�\n");
        return 0;
    }
    if ((pid = tpGetPid("tpTaskMng")) > 0) {       
        if (gShm.pShmHead->nActive > 0) {
            printf("ƽ̨�Դ��ڻ״̬,�Ƿ�ǿ��ִ��(y/n)?");
            switch (getchar()) {
                case 'y':
                case 'Y':
                    break;
                default:
                    return -1;
            }                
        }
        kill(pid, SIGTERM);
        for (i=0; i<gShm.pShmList->tpTask.nItems; i++) {
            if (gShm.ptpTask[i].pid > 0) {
                kill(gShm.ptpTask[i].pid, SIGKILL);
                gShm.ptpTask[i].pid = 0;
                time(&gShm.ptpTask[i].stopTime);
            }
        }
        sleep(1);
    }
    if (tpShmFree() != 0) {
        printf("ִ������ʧ��\n");        
        return -1;
    }
    printf("ִ������ɹ�\n");
    return 0;
}

/***************************************************************
 * ��ʼ��ƽ̨���ݿ�
 **************************************************************/
int tpInitDB(DBHDL *dbh)
{
    char *sql;

    /*TPLOG*/
    if (!sqliteObjectExist(dbh, "table", "tp_log")) {
        sql = "CREATE TABLE tp_log ("
              "tp_id INTEGER,"
              "org_q INTEGER,"
              "tran_code VARCHAR(20),"
              "serv_code VARCHAR(20),"
              "begin_time INTEGER,"
              "status CHAR(1));";
        if (sqliteExecute(dbh, sql) != 0) {
            LOG_ERR("Failed to create table tp_log");
            return -1;
        }
    }

    if (!sqliteObjectExist(dbh, "index", "idx_tp_log")) {
        sql = "CREATE UNIQUE INDEX idx_tp_log ON tp_log(tp_id,begin_time)";
        if (sqliteExecute(dbh, sql) != 0) {
            LOG_ERR("Failed to create index idx_tp_log");
            return -1;
        }
    }
    /*End TPLOG*/

    /*TPSAF*/
    if (!sqliteObjectExist(dbh, "table", "tp_saf")) {
        sql = "CREATE TABLE tp_saf ("
              "saf_id INTEGER,"
              "tp_id INTEGER,"
              "des_q INTEGER,"
              "serv_code VARCHAR(20),"
              "proc_code VARCHAR(20),"
              "saf_num INTEGER,"
              "over_time INTEGER);";
        if (sqliteExecute(dbh, sql) != 0) {
            LOG_ERR("Failed to create table tp_saf");
            return -1;
        }
    }

    if (!sqliteObjectExist(dbh, "index", "idx_tp_saf")) {
        sql = "CREATE UNIQUE INDEX idx_tp_saf ON tp_saf(saf_id)";
        if (sqliteExecute(dbh, sql) != 0) {
            LOG_ERR("Failed to create index idx_tp_saf");
            return -1;
        }
    }
    /*End TPSAF*/

    return 0;
}

/***************************************************************
 * ��ʼ��ƽ̨��ˮ��
 **************************************************************/
int tpInitID(DBHDL *dbh, int *result)
{
    sqlite3_stmt *pStmt;
    char *sql = "select max(tp_id) from tp_log";

    if (sqlitePrepare(dbh, sql, &pStmt) != 0) return -1;
    if (sqliteFetch(pStmt) != SQLITE_ROW) {
        sqliteRelease(pStmt);
        return -1;
    }
    *result = sqliteGetInt(pStmt, 0);
    sqliteRelease(pStmt);
    return 0;
}

/***************************************************************
 * ��ʼ��SAF��ˮ��
 **************************************************************/
int tpInitSAF(DBHDL *dbh, int *result)
{
    sqlite3_stmt *pStmt;
    char *sql = "select max(saf_id) from tp_saf";

    if (sqlitePrepare(dbh, sql, &pStmt) != 0) return -1;
    if (sqliteFetch(pStmt) != SQLITE_ROW) {
        sqliteRelease(pStmt);
        return -1;
    }
    *result = sqliteGetInt(pStmt, 0);
    sqliteRelease(pStmt);
    return 0;
}

/***************************************************************
 * ��鹲���ڴ��Ƿ��Ѵ���
 **************************************************************/
int tpShmExist()
{
    int shmKey;
  
    shmKey = (getenv("TPBUS_SHM_KEY") == NULL)? SHM_KEY:atoi(getenv("TPBUS_SHM_KEY"));
    return (shmget((key_t)shmKey, 0, IPC_EXCL|0600) == -1)? 0:1;
}

/***************************************************************
 * initShmEntry
 **************************************************************/
int initShmEntry(SHM_ENTRY *pShmEntry)
{
    memset(pShmEntry, 0, sizeof(SHM_ENTRY));   
    if (!(pShmEntry->pShmList = (SHM_LIST *)malloc(sizeof(SHM_LIST)))) {
        LOG_ERR("malloc() error: %s", strerror(errno));
        return -1;
    }
    memset(pShmEntry->pShmList, 0, sizeof(SHM_LIST));
    return 0;
}

/***************************************************************
 * freeShmEntry
 **************************************************************/
void freeShmEntry(SHM_ENTRY *pShmEntry)
{
    if (pShmEntry->pShmList)       free(pShmEntry->pShmList);
    if (pShmEntry->ptpConf)        free(pShmEntry->ptpConf);
    if (pShmEntry->ptpTask)        free(pShmEntry->ptpTask);
    if (pShmEntry->ptpPort)        free(pShmEntry->ptpPort);
    if (pShmEntry->ptpServ)        free(pShmEntry->ptpServ);
    if (pShmEntry->ptpProc)        free(pShmEntry->ptpProc);
    if (pShmEntry->ptpMapp)        free(pShmEntry->ptpMapp);
    if (pShmEntry->ptpMoni)        free(pShmEntry->ptpMoni);
    if (pShmEntry->pBitmap)        free(pShmEntry->pBitmap);
    if (pShmEntry->pRoutGrp)       free(pShmEntry->pRoutGrp);
    if (pShmEntry->pRoutStmt)      free(pShmEntry->pRoutStmt);
    if (pShmEntry->pConvGrp)       free(pShmEntry->pConvGrp);
    if (pShmEntry->pConvStmt)      free(pShmEntry->pConvStmt);
    /*memset(pShmEntry, 0, sizeof(SHM_ENTRY));*/
}

/***************************************************************
 * ��ʼ�������ڴ�
 **************************************************************/
int tpShmInit()
{
    int  i,shmKey,shmId,semId;
    long len,size,bizSize,offset;
    unsigned short arrVal[SEM_NUM];
    FILE *fp;
    char *pShm,tmpBuf[256];;
    void *pBizArea=NULL;
    DBHDL *dbh;
    SHM_ENTRY shmEntry;

    shmKey = (getenv("TPBUS_SHM_KEY")==NULL)? SHM_KEY : atoi(getenv("TPBUS_SHM_KEY"));

    size = sizeof(SHM_HEAD);
    if ((shmId = shmget((key_t)shmKey, (size_t)size, IPC_CREAT|IPC_EXCL|0600)) == -1) {
        if (errno == EEXIST) {
          LOG_WRN("�����ڴ��Ѿ���ʼ��");
        } else {
          LOG_ERR("shmget() error: %s", strerror(errno));
          LOG_ERR("���������ڴ����");
        }
        return -1;
    }

    if (initShmEntry(&shmEntry) != 0) {
        shmctl(shmId, IPC_RMID, 0);
        return -1;
    }

    if ((shmEntry.pShmHead = (SHM_HEAD *)shmat(shmId, NULL, SHM_RND)) == (SHM_HEAD *)-1) {
        LOG_ERR("shmat() error: %s", strerror(errno));
        LOG_ERR("���ӹ����ڴ����");
        shmctl(shmId, IPC_RMID, 0);
        return -1;
    }
    memset(shmEntry.pShmHead, 0, sizeof(SHM_HEAD));
    shmEntry.pShmHead->semId    = -1;
    shmEntry.pShmHead->shmId    = shmId;
    shmEntry.pShmHead->shmCfgId = -1;
    shmEntry.pShmHead->shmCtxId = -1;
    shmEntry.pShmHead->online   = 0;

    if (getenv("TPBUS_HOST")) shmEntry.pShmHead->hostId = atoi(getenv("TPBUS_HOST"));
    if (getenv("TPBUS_DISPATCH")) shmEntry.pShmHead->dispatch = atoi(getenv("TPBUS_DISPATCH"));

    /*�����ݿ�*/
    if (tpSQLiteOpen(&dbh) != 0) {
       LOG_ERR("�����ݿ����");
       goto ERR_RET;
    }

    /*��ʼ�����ݿ��*/
    if (tpInitDB(dbh) != 0) {
        LOG_ERR("��ʼ��ƽ̨���ݿ����");
        tpSQLiteClose(dbh);
        goto ERR_RET;
    }

    /*��ʼ��ƽ̨��ˮ��*/
    /*if (tpInitID(dbh, &shmEntry.pShmHead->tpId) != 0) {
        LOG_ERR("��ʼ��ƽ̨��ˮ�ų���");
        tpSQLiteClose(dbh);
        goto ERR_RET;
    }*/
    shmEntry.pShmHead->tpId = 0;
    snprintf(tmpBuf, sizeof(tmpBuf), "%s/rtm/tpId.dump", getenv("TPBUS_HOME"));
    if ((fp = fopen(tmpBuf, "r")) != NULL) {
        if (fgets(tmpBuf, sizeof(tmpBuf), fp) != NULL) {
            sscanf(tmpBuf, "%d", &shmEntry.pShmHead->tpId);
            shmEntry.pShmHead->tpId += 1000;
        }
        fclose(fp);
    }
    LOG_MSG("��ʼ��ƽ̨��ˮ��OK[MaxID=%d]", shmEntry.pShmHead->tpId); 
     
    /*��ʼ��SAF��ˮ��*/
    if (tpInitSAF(dbh, &shmEntry.pShmHead->safId) != 0) {
        LOG_ERR("��ʼ��SAF��ˮ�ų���");
        tpSQLiteClose(dbh);
        goto ERR_RET;
    }
    LOG_MSG("��ʼ��SAF��ˮ��OK[MaxID=%d]", shmEntry.pShmHead->safId);
    tpSQLiteClose(dbh);

    /*��ʼװ��������Ϣ*/

    if (tpConfLoad(&shmEntry) != 0) {
        LOG_ERR("װ��ƽ̨�������ó���");
        goto ERR_RET;
    }
    LOG_MSG("װ��ƽ̨��������...[ok]");

    if (tpTaskLoad(&shmEntry) != 0) {
        LOG_ERR("װ���������ó���");
        goto ERR_RET;
    }
    LOG_MSG("װ����������...[ok]");
  
    if (tpPortLoad(&shmEntry) != 0) {
        LOG_ERR("װ��Ӧ�ö˵����ó���");
        goto ERR_RET;
    }
    LOG_MSG("װ��Ӧ�ö˵�����...[ok]");

    if (tpServLoad(&shmEntry) != 0) {
        LOG_ERR("װ�ط���ע����Ϣ����");
        goto ERR_RET;
    }
    LOG_MSG("װ�ط���ע����Ϣ...[ok]");

    if (tpProcLoad(&shmEntry) != 0) {
        LOG_ERR("װ��Ԫ���������");
        goto ERR_RET;
    }
    LOG_MSG("װ��Ԫ������...[ok]");

    if (tpMappLoad(&shmEntry) != 0) {
        LOG_ERR("װ�ط���ӳ�����ó���");
        goto ERR_RET;
    }
    LOG_MSG("װ�ط���ӳ������...[ok]");

    if (bitmapLoad(&shmEntry) != 0) {
        LOG_ERR("װ��ISO8583λͼ���ó���");
        goto ERR_RET;
    }
    LOG_MSG("װ��ISO8583λͼ����...[ok]");

    if (tpRoutLoad(&shmEntry) != 0) {
        LOG_ERR("װ��·�����ó���");
        goto ERR_RET;
    }
    LOG_MSG("װ��·������...[ok]");
  
    if (tpConvLoad(&shmEntry) != 0) {
        LOG_ERR("װ�ظ�ʽת�����ó���");
        goto ERR_RET;
    }
    LOG_MSG("װ�ظ�ʽת������...[ok]");

    if ((bizSize = tpBizProcLoad(&pBizArea)) < 0) {
        LOG_ERR("װ��ҵ��������ó���");
        goto ERR_RET;
    }
    if (bizSize > 0) LOG_MSG("װ��ҵ���������...[ok]");

    /*����Ѱַƫ����*/
    size = sizeof(SHM_LIST);

    /*TPCONF*/
    size += sizeof(TPCONF);

    /*TPTASK*/
    shmEntry.pShmList->tpTask.offset = size;
    size += sizeof(TPTASK) * shmEntry.pShmList->tpTask.nItems;

    /*TPPORT*/
    shmEntry.pShmList->tpPort.offset = size;
    size += sizeof(TPPORT) * shmEntry.pShmList->tpPort.nItems;

    /*TPSERV*/
    shmEntry.pShmList->tpServ.offset = size;
    size += sizeof(TPSERV) * shmEntry.pShmList->tpServ.nItems;

    /*TPPROC*/
    shmEntry.pShmList->tpProc.offset = size;
    size += sizeof(TPPROC) * shmEntry.pShmList->tpProc.nItems;

    /*TPMAPP*/
    shmEntry.pShmList->tpMapp.offset = size;
    size += sizeof(TPMAPP) * shmEntry.pShmList->tpMapp.nItems;

    /*TPMONI*/
    shmEntry.pShmList->tpMoni.offset = size;
    size += sizeof(TPMONI) * shmEntry.pShmList->tpMoni.nItems;

    /*BITMAP*/
    shmEntry.pShmList->bitmap.offset = size;
    size += sizeof(BITMAP) * shmEntry.pShmList->bitmap.nItems;

    /*ROUTGRP*/      
    shmEntry.pShmList->routGrp.offset = size;
    size += sizeof(ROUTGRP) * shmEntry.pShmList->routGrp.nItems;

    /*ROUTSTMT*/
    shmEntry.pShmList->routStmt.offset = size;
    size += sizeof(ROUTSTMT) * shmEntry.pShmList->routStmt.nItems;

    /*CONVGRP*/
    shmEntry.pShmList->convGrp.offset = size;
    size += sizeof(CONVGRP) * shmEntry.pShmList->convGrp.nItems;

    /*CONVSTMT*/
    shmEntry.pShmList->convStmt.offset = size;
    size += sizeof(CONVSTMT) * shmEntry.pShmList->convStmt.nItems;

    /*BIZPROC*/
    size += bizSize;

    if (tpIndexPreSet(&shmEntry) != 0) {
        LOG_ERR("���㹲���ڴ���������");
        goto ERR_RET;
    }

    if ((shmId = shmget(IPC_PRIVATE, (size_t)size, IPC_CREAT|IPC_EXCL|0600)) == -1) {
        LOG_ERR("shmget() error: %s", strerror(errno));
        LOG_ERR("���������ڴ����");
        goto ERR_RET;
    }
    shmEntry.pShmHead->shmCfgId = shmId;
    LOG_MSG("�������ռ��С=[%ld �ֽ�]", size);

    if ((pShm = (char *)shmat(shmId, NULL, SHM_RND)) == (char *)-1) {
        LOG_ERR("shmat() error: %s", strerror(errno));
        LOG_ERR("���ӹ����ڴ����");
        goto ERR_RET;
    }
    offset = 0;

    /*����������Ϣ*/

    /*����Ŀ¼*/
    memcpy(pShm+offset, shmEntry.pShmList, sizeof(SHM_LIST));
    offset += sizeof(SHM_LIST);

    /*TPCONF*/
    memcpy(pShm+offset, shmEntry.ptpConf, sizeof(TPCONF)); 
    offset += sizeof(TPCONF);

    /*TPTASK*/
    len = sizeof(TPTASK)*shmEntry.pShmList->tpTask.nItems;
    memcpy(pShm+offset, shmEntry.ptpTask, len);
    offset += len;

    /*TPPORT*/
    len = sizeof(TPPORT)*shmEntry.pShmList->tpPort.nItems;
    memcpy(pShm+offset, shmEntry.ptpPort, len);
    offset += len;

    /*TPSERV*/
    len = sizeof(TPSERV)*shmEntry.pShmList->tpServ.nItems;
    memcpy(pShm+offset, shmEntry.ptpServ, len);
    offset += len;

    /*TPPROC*/
    len = sizeof(TPPROC)*shmEntry.pShmList->tpProc.nItems;
    memcpy(pShm+offset, shmEntry.ptpProc, len);
    offset += len;

    /*TPMAPP*/
    len = sizeof(TPMAPP)*shmEntry.pShmList->tpMapp.nItems;
    memcpy(pShm+offset, shmEntry.ptpMapp, len);
    offset += len;

    /*TPMONI*/
    len = sizeof(TPMONI)*shmEntry.pShmList->tpMoni.nItems;
    memcpy(pShm+offset, shmEntry.ptpMoni, len);
    offset += len;

    /*BITMAP*/
    len = sizeof(BITMAP)*shmEntry.pShmList->bitmap.nItems;
    memcpy(pShm+offset, shmEntry.pBitmap, len);
    offset += len;

    /*ROUTGRP*/
    len = sizeof(ROUTGRP)*shmEntry.pShmList->routGrp.nItems;
    memcpy(pShm+offset, shmEntry.pRoutGrp, len);
    offset += len;

    /*ROUTSTMT*/
    len = sizeof(ROUTSTMT)*shmEntry.pShmList->routStmt.nItems;
    memcpy(pShm+offset, shmEntry.pRoutStmt, len);
    offset += len;

    /*CONVGRP*/
    len = sizeof(CONVGRP)*shmEntry.pShmList->convGrp.nItems;
    memcpy(pShm+offset, shmEntry.pConvGrp, len);
    offset += len;

    /*CONVSTMT*/
    len = sizeof(CONVSTMT)*shmEntry.pShmList->convStmt.nItems;
    memcpy(pShm+offset, shmEntry.pConvStmt, len);
    offset += len;

    /*BIZPROC*/
    if (bizSize > 0) memcpy(pShm+offset, pBizArea, bizSize);

    /*������Ϣװ�����*/

    /*������+�첽����ͷ�ռ�*/
    size = sizeof(SHM_LINK);
    size += sizeof(SHM_TRAN)*shmEntry.ptpConf->nMaxTrans;
	size += sizeof(SHM_TPHD)*shmEntry.ptpConf->nMaxTrans;
    if ((shmId = shmget(IPC_PRIVATE, (size_t)size, IPC_CREAT|IPC_EXCL|0600)) == -1) {
        LOG_ERR("shmget() error: %s", strerror(errno));
        LOG_ERR("���������ڴ����");
        goto ERR_RET;
    }
    shmEntry.pShmHead->shmCtxId = shmId;
    LOG_MSG("������+�첽����ͷ�ռ��С=[%ld �ֽ�]", size);
      
    if ((shmEntry.pShmLink = (SHM_LINK *)shmat(shmId, NULL, SHM_RND)) == (SHM_LINK *)-1) {
        LOG_ERR("shmat() error: %s", strerror(errno));
        LOG_ERR("���ӹ����ڴ����");
        goto ERR_RET;
    }
    shmEntry.pShmLink->tpTran.head   = 0;
    shmEntry.pShmLink->tpTran.tail   = 0;
    shmEntry.pShmLink->tpTran.free   = 1;
    shmEntry.pShmLink->tpTran.offset = sizeof(SHM_LINK);

    shmEntry.pShmLink->tpHead.head   = 0;
    shmEntry.pShmLink->tpHead.tail   = 0;
    shmEntry.pShmLink->tpHead.free   = 1;
    shmEntry.pShmLink->tpHead.offset = sizeof(SHM_LINK) + sizeof(SHM_TRAN)*shmEntry.ptpConf->nMaxTrans;

    /*�ź���*/
    if ((semId = semget(IPC_PRIVATE, SEM_NUM, IPC_CREAT|IPC_EXCL|0600)) == -1) {
        LOG_ERR("semget() error: %s", strerror(errno));
        LOG_ERR("�����ź�������");
        goto ERR_RET;
    }
    shmEntry.pShmHead->semId = semId;

    for (i=0; i<SEM_NUM; i++) arrVal[i] = 1;
    if (semctl(semId, SEM_NUM, SETALL, arrVal) == -1) {
        LOG_ERR("semctl() error: %s", strerror(errno));
        LOG_ERR("��ʼ���ź�������");
        goto ERR_RET;
    }

    shmEntry.pShmHead->online = 1;
    freeShmEntry(&shmEntry);
    return 0;
  
ERR_RET: 
    freeShmEntry(&shmEntry);
    if (pBizArea) free(pBizArea);
    if (shmEntry.pShmHead->shmId    != -1) shmctl(shmEntry.pShmHead->shmId,    IPC_RMID, 0);
    if (shmEntry.pShmHead->shmCfgId != -1) shmctl(shmEntry.pShmHead->shmCfgId, IPC_RMID, 0);
    if (shmEntry.pShmHead->shmCtxId != -1) shmctl(shmEntry.pShmHead->shmCtxId, IPC_RMID, 0);
    if (shmEntry.pShmHead->semId    != -1) semctl(shmEntry.pShmHead->semId, 0, IPC_RMID, 0);
    return -1;
}

/***************************************************************
 * ˢ�¹����ڴ�
 **************************************************************/
int tpShmReload()
{
    int  i,j,shmKey;
    int  shmId,shmOldId,shmNewId=-1;
    long len,size,bizSize,offset;
    char *pShm,*pShmOld;
    void *pBizArea=NULL;
    SHM_ENTRY shmEntry;
    SHM_LIST *pShmList,*pShmListOld;
    TPTASK *ptpTask,*ptpTaskOld;
    TPSERV *ptpServ,*ptpServOld;

    shmKey = (getenv("TPBUS_SHM_KEY")==NULL)? SHM_KEY : atoi(getenv("TPBUS_SHM_KEY"));

    if ((shmId = shmget((key_t)shmKey, 0, IPC_EXCL|0600)) == -1) {
        LOG_ERR("shmget() error: %s", strerror(errno));
        return -1;
    }
    if (initShmEntry(&shmEntry) != 0) return -1;
    
    if ((shmEntry.pShmHead = (SHM_HEAD *)shmat(shmId, NULL, SHM_RND)) == (SHM_HEAD *)-1) {
        LOG_ERR("shmat() error: %s", strerror(errno));
        return -1;
    }

    shmEntry.pShmHead->online = 0;
    shmOldId = shmEntry.pShmHead->shmCfgId;

    /*��ʼװ��������Ϣ*/

    if (tpConfLoad(&shmEntry) != 0) {
        LOG_ERR("װ��ƽ̨�������ó���");
        goto ERR_RET;
    }
    LOG_MSG("װ��ƽ̨��������...[ok]");

    if (tpTaskLoad(&shmEntry) != 0) {
        LOG_ERR("װ���������ó���");
        goto ERR_RET;
    }
    LOG_MSG("װ����������...[ok]");
  
    if (tpPortLoad(&shmEntry) != 0) {
        LOG_ERR("װ��Ӧ�ö˵����ó���");
        goto ERR_RET;
    }
    LOG_MSG("װ��Ӧ�ö˵�����...[ok]");

    if (tpServLoad(&shmEntry) != 0) {
        LOG_ERR("װ�ط���ע����Ϣ����");
        goto ERR_RET;
    }
    LOG_MSG("װ�ط���ע����Ϣ...[ok]");

    if (tpProcLoad(&shmEntry) != 0) {
        LOG_ERR("װ��Ԫ���������");
        goto ERR_RET;
    }
    LOG_MSG("װ��Ԫ������...[ok]");

    if (tpMappLoad(&shmEntry) != 0) {
        LOG_ERR("װ�ط���ӳ�����ó���");
        goto ERR_RET;
    }
    LOG_MSG("װ�ط���ӳ������...[ok]");

    if (bitmapLoad(&shmEntry) != 0) {
        LOG_ERR("װ��ISO8583λͼ���ó���");
        goto ERR_RET;
    }
    LOG_MSG("װ��ISO8583λͼ����...[ok]");

    if (tpRoutLoad(&shmEntry) != 0) {
        LOG_ERR("װ��·�����ó���");
        goto ERR_RET;
    }
    LOG_MSG("װ��·������...[ok]");
  
    if (tpConvLoad(&shmEntry) != 0) {
        LOG_ERR("װ�ظ�ʽת�����ó���");
        goto ERR_RET;
    }
    LOG_MSG("װ�ظ�ʽת������...[ok]");

    if ((bizSize = tpBizProcLoad(&pBizArea)) < 0) {
        LOG_ERR("װ��ҵ��������ó���");
        goto ERR_RET;
    }
    if (bizSize > 0) LOG_MSG("װ��ҵ���������...[ok]");

    /*����Ѱַƫ����*/
    size = sizeof(SHM_LIST);

    /*TPCONF*/
    size += sizeof(TPCONF);

    /*TPTASK*/
    shmEntry.pShmList->tpTask.offset = size;
    size += sizeof(TPTASK) * shmEntry.pShmList->tpTask.nItems;

    /*TPPORT*/
    shmEntry.pShmList->tpPort.offset = size;
    size += sizeof(TPPORT) * shmEntry.pShmList->tpPort.nItems;

    /*TPSERV*/
    shmEntry.pShmList->tpServ.offset = size;
    size += sizeof(TPSERV) * shmEntry.pShmList->tpServ.nItems;

    /*TPPROC*/
    shmEntry.pShmList->tpProc.offset = size;
    size += sizeof(TPPROC) * shmEntry.pShmList->tpProc.nItems;

    /*TPMAPP*/
    shmEntry.pShmList->tpMapp.offset = size;
    size += sizeof(TPMAPP) * shmEntry.pShmList->tpMapp.nItems;

    /*TPMONI*/
    shmEntry.pShmList->tpMoni.offset = size;
    size += sizeof(TPMONI) * shmEntry.pShmList->tpMoni.nItems;

    /*BITMAP*/
    shmEntry.pShmList->bitmap.offset = size;
    size += sizeof(BITMAP) * shmEntry.pShmList->bitmap.nItems;

    /*ROUTGRP*/      
    shmEntry.pShmList->routGrp.offset = size;
    size += sizeof(ROUTGRP) * shmEntry.pShmList->routGrp.nItems;

    /*ROUTSTMT*/
    shmEntry.pShmList->routStmt.offset = size;
    size += sizeof(ROUTSTMT) * shmEntry.pShmList->routStmt.nItems;

    /*CONVGRP*/
    shmEntry.pShmList->convGrp.offset = size;
    size += sizeof(CONVGRP) * shmEntry.pShmList->convGrp.nItems;

    /*CONVSTMT*/
    shmEntry.pShmList->convStmt.offset = size;
    size += sizeof(CONVSTMT) * shmEntry.pShmList->convStmt.nItems;

    /*BIZPROC*/
    size += bizSize;

    if (tpIndexPreSet(&shmEntry) != 0) {
        LOG_ERR("���㹲���ڴ���������");
        goto ERR_RET;
    }

    if ((shmNewId = shmget(IPC_PRIVATE, (size_t)size, IPC_CREAT|IPC_EXCL|0600)) == -1) {
        LOG_ERR("shmget() error: %s", strerror(errno));
        LOG_ERR("���������ڴ����");
        goto ERR_RET;
    }
    if ((pShm = (char *)shmat(shmNewId, NULL, SHM_RND)) == (char *)-1) {
        LOG_ERR("shmat() error: %s", strerror(errno));
        LOG_ERR("���ӹ����ڴ����");
        goto ERR_RET;
    }
    offset = 0;

    /*����������Ϣ*/

    /*����Ŀ¼*/
    memcpy(pShm+offset, shmEntry.pShmList, sizeof(SHM_LIST));
    offset += sizeof(SHM_LIST);

    /*TPCONF*/
    memcpy(pShm+offset, shmEntry.ptpConf, sizeof(TPCONF)); 
    offset += sizeof(TPCONF);

    /*TPTASK*/
    len = sizeof(TPTASK)*shmEntry.pShmList->tpTask.nItems;
    memcpy(pShm+offset, shmEntry.ptpTask, len);
    offset += len;

    /*TPPORT*/
    len = sizeof(TPPORT)*shmEntry.pShmList->tpPort.nItems;
    memcpy(pShm+offset, shmEntry.ptpPort, len);
    offset += len;

    /*TPSERV*/
    len = sizeof(TPSERV)*shmEntry.pShmList->tpServ.nItems;
    memcpy(pShm+offset, shmEntry.ptpServ, len);
    offset += len;

    /*TPPROC*/
    len = sizeof(TPPROC)*shmEntry.pShmList->tpProc.nItems;
    memcpy(pShm+offset, shmEntry.ptpProc, len);
    offset += len;

    /*TPMAPP*/
    len = sizeof(TPMAPP)*shmEntry.pShmList->tpMapp.nItems;
    memcpy(pShm+offset, shmEntry.ptpMapp, len);
    offset += len;

    /*TPMONI*/
    len = sizeof(TPMONI)*shmEntry.pShmList->tpMoni.nItems;
    memcpy(pShm+offset, shmEntry.ptpMoni, len);
    offset += len;

    /*BITMAP*/
    len = sizeof(BITMAP)*shmEntry.pShmList->bitmap.nItems;
    memcpy(pShm+offset, shmEntry.pBitmap, len);
    offset += len;

    /*ROUTGRP*/
    len = sizeof(ROUTGRP)*shmEntry.pShmList->routGrp.nItems;
    memcpy(pShm+offset, shmEntry.pRoutGrp, len);
    offset += len;

    /*ROUTSTMT*/
    len = sizeof(ROUTSTMT)*shmEntry.pShmList->routStmt.nItems;
    memcpy(pShm+offset, shmEntry.pRoutStmt, len);
    offset += len;

    /*CONVGRP*/
    len = sizeof(CONVGRP)*shmEntry.pShmList->convGrp.nItems;
    memcpy(pShm+offset, shmEntry.pConvGrp, len);
    offset += len;

    /*CONVSTMT*/
    len = sizeof(CONVSTMT)*shmEntry.pShmList->convStmt.nItems;
    memcpy(pShm+offset, shmEntry.pConvStmt, len);
    offset += len;

    /*BIZPROC*/
    if (bizSize > 0) memcpy(pShm+offset, pBizArea, bizSize);

    /*������Ϣװ�����*/

    if ((pShmOld = (char *)shmat(shmOldId, NULL, SHM_RND)) == (char *)-1) {
        LOG_ERR("shmat() error: %s", strerror(errno));
        goto ERR_RET;
    }
    pShmList = (SHM_LIST *)pShm;
    pShmListOld = (SHM_LIST *)pShmOld;

    /*ͬ������״̬*/
    ptpTask = (TPTASK *)(pShm + pShmList->tpTask.offset);
    ptpTaskOld = (TPTASK *)(pShmOld + pShmListOld->tpTask.offset);
    for (i=0; i<pShmList->tpTask.nItems; i++) {
        for (j=0; j<pShmListOld->tpTask.nItems; j++) {
            if (ptpTask[i].taskId != ptpTaskOld[j].taskId) continue;
            ptpTask[i].pid = ptpTaskOld[j].pid;
            ptpTask[i].startTime = ptpTaskOld[j].startTime;
            ptpTask[i].stopTime = ptpTaskOld[j].stopTime;
            ptpTask[i].restedNum = ptpTaskOld[j].restedNum;
        }
    }
    LOG_MSG("ͬ������״̬...[ok]");

    /*ͬ������״̬*/
    ptpServ = (TPSERV *)(pShm + pShmList->tpServ.offset);
    ptpServOld = (TPSERV *)(pShmOld + pShmListOld->tpServ.offset);
    for (i=0; i<pShmList->tpServ.nItems; i++) {
        if (!ptpServ[i].servCode[0]) continue;
        for (j=0; j<pShmListOld->tpServ.nItems; j++) {
            if (!ptpServOld[j].servCode[0]) continue;
            if (strcmp(ptpServ[i].servCode, ptpServOld[j].servCode) != 0) continue;
            ptpServ[i].nActive = ptpServOld[j].nActive;
            ptpServ[i].nReving = ptpServOld[j].nReving;
        }
    }
    LOG_MSG("ͬ������״̬...[ok]");

    shmEntry.pShmHead->shmCfgId = shmNewId;
    shmctl(shmOldId, IPC_RMID, 0);
    shmEntry.pShmHead->online = 1;
    return 0;

ERR_RET:
    freeShmEntry(&shmEntry);
    if (pBizArea) free(pBizArea);
    if (shmNewId != -1) shmctl(shmNewId, IPC_RMID, 0);
    shmEntry.pShmHead->online = 1;
    return -1;
}

/***************************************************************
 * ж�ع����ڴ�
 **************************************************************/
int tpShmFree()
{
    int ret = 0;
    int shmKey,shmId;
    FILE *fp;
    char tmpBuf[256];
    SHM_HEAD *pShmHead;
    
    shmKey = (getenv("TPBUS_SHM_KEY"))? atoi(getenv("TPBUS_SHM_KEY")) : SHM_KEY;

    if ((shmId = shmget((key_t)shmKey, 0, IPC_EXCL|0600)) == -1) {
        LOG_ERR("shmget() error: %s", strerror(errno));
        return -1;
    }
    if ((pShmHead = (SHM_HEAD *)shmat(shmId, NULL, SHM_RND)) == (SHM_HEAD *)-1) {
        LOG_ERR("shmat() error: %s", strerror(errno));
        return -1;
    }
    pShmHead->online = 0; /*�л�Ϊ����״̬*/

    snprintf(tmpBuf, sizeof(tmpBuf), "%s/rtm/tpId.dump", getenv("TPBUS_HOME"));
    if ((fp = fopen(tmpBuf, "w")) != NULL) {
        fprintf(fp, "%d\n", pShmHead->tpId);
        fclose(fp);
    }

    /*�ź���*/
    if (pShmHead->semId != -1) {
        if (semctl(pShmHead->semId, 0, IPC_RMID, 0) == -1) {
            LOG_ERR("semctl() error: %s", strerror(errno));
            ret = -1;
        }
    }
    
    /*�����ڴ���������*/
    if (pShmHead->shmCfgId != -1) {
        if (shmctl(pShmHead->shmCfgId, IPC_RMID, 0) == -1) {
            LOG_ERR("shmctl() error: %s", strerror(errno));
            ret = -1;
        }
    }
    
    /*�����ڴ�����������*/
    if (pShmHead->shmCtxId != -1) {
        if (shmctl(pShmHead->shmCtxId, IPC_RMID, 0) == -1) {
            LOG_ERR("shmctl() error: %s", strerror(errno));
            ret = -1;
        }
    }
    
    /*�����ڴ�ͷ��Ϣ����*/
    if (shmctl(shmId, IPC_RMID, 0) == -1) {
        LOG_ERR("shmctl() error: %s", strerror(errno));
        ret = -1;
    }
    return ret;
}

