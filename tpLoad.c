/***************************************************************
 * ������Ϣװ�غ���
 **************************************************************/
#include <dlfcn.h>
#include "tpbus.h"

#define NMAXTAGS  100  

#define KW_NONE       DO_NONE
#define KW_JUMP       DO_JUMP
#define KW_IF         KW_NONE+71
#define KW_ELSEIF     KW_NONE+72
#define KW_ELSE       KW_NONE+73
#define KW_ENDTHEN    KW_NONE+74
#define KW_ENDIF      KW_NONE+75
#define KW_WHILE      KW_NONE+76
#define KW_CONTINUE   KW_NONE+77
#define KW_EXIT       KW_NONE+78
#define KW_ENDWHILE   KW_NONE+79
#define KW_SWITCH     KW_NONE+80
#define KW_CASE       KW_NONE+81
#define KW_BREAK      KW_NONE+82
#define KW_DEFAULT    KW_NONE+83
#define KW_ENDSWITCH  KW_NONE+84  
#define KW_VAR        DO_VAR
#define KW_ALLOC      DO_ALLOC
#define KW_SET        DO_SET
#define KW_EXEC       DO_EXEC
#define KW_CALL       DO_CALL
#define KW_SEND       DO_SEND
#define KW_CONV       DO_CONV
#define KW_REVERSE    DO_REVERSE
#define KW_RETURN     DO_RETURN

static int    m_tpFunSize = 0;
static TPFUN *m_tpFunList = NULL;
static char  *m_macroDefs = NULL;

static char S_ENDIF[] = "ENDIF";

typedef struct {
    char name[MAXKEYLEN+1];
    int  index;
} JUMP_TAG;

typedef struct stk_switch_st {
    char   express[MAXEXPLEN+1];
    struct stk_switch_st *next;
} STK_SWITCH;

typedef struct stk_item_st {
    int    index;
    char   action;
    struct stk_item_st *next;
} STK_ITEM;

/***************************************************************
 * �ļ�������
 **************************************************************/
static int lock_file(int fd, int cmd, int type, int offset, int whence, int len)
{
    struct flock lock;

    lock.l_type   = type;
    lock.l_start  = offset;
    lock.l_whence = whence;
    lock.l_len    = len;
    
    switch (cmd) {
        case F_SETLK:
            if (fcntl(fd, cmd, &lock) == -1) {
                LOG_ERR("fcntl() error: %s", strerror(errno));
                return -1;
            }
            break;
        case F_SETLKW:
            while (1) {
                if (fcntl(fd, cmd, &lock) != -1) break;
                if (errno != EINTR) {
                    LOG_ERR("fcntl() error: %s", strerror(errno));
                    return -1;
                }
            }
            break;
        default:
            LOG_ERR("fcntl(): Invalid argument");
            return -1;
    }
    return 0;
}

#define read_lock(fd, offset, len) \
        lock_file(fd, F_SETLK, F_RDLCK, offset, SEEK_SET, len)

#define read_lockw(fd, offset, len) \
        lock_file(fd, F_SETLKW, F_RDLCK, offset, SEEK_SET, len)

#define write_lock(fd, offset, len) \
        lock_file(fd, F_SETLK, F_WRLCK, offset, SEEK_SET, len)

#define write_lockw(fd, offset, len) \
        lock_file(fd, F_SETLKW, F_WRLCK, offset, SEEK_SET, len)

#define release_lock(fd, offset, len) \
        lock_file(fd, F_SETLK, F_UNLCK, offset, SEEK_SET, len)

/***************************************************************
 * ͬ����������
 **************************************************************/
int tpConfSync(const char *host, int port)
{
    FILE *fp=NULL;
    int len,flag=0,sockfd=-1;
    char cfg[256],etc[256];
    char buf[1024],tmpFile[256];
    time_t t;

    snprintf(etc, sizeof(etc), "%s/etc", getenv("TPBUS_HOME"));

    /*�������÷�����*/
    if ((sockfd = tcp_connect(NULL, host, port, 0)) == -1) {
        LOG_ERR("�������÷�����[%s,%d]ʧ��", host, port);
        goto ERR_RET;
    }
    LOG_MSG("�������÷�����[%s,%d]...[ok]", host, port);

    /*����ͬ��������*/
    snprintf(buf, sizeof(buf), "%d|SYNC", (!getenv("TPBUS_HOST"))? 0:atoi(getenv("TPBUS_HOST")));
    if (tcp_send(sockfd, buf, strlen(buf), 4, 0) == -1) {
        LOG_ERR("tcp_send() error: %s", strerror(errno));
        LOG_ERR("����ͬ���������");
        goto ERR_RET;
    }
    LOG_MSG("����ͬ��������...[ok]");

    /*�����������嵥*/
    if ((len = tcp_recv(sockfd, cfg, 0, 4, 0)) == -1) {
        LOG_ERR("tcp_recv() error: %s", strerror(errno));
        LOG_ERR("�����������嵥����");	
        goto ERR_RET;
    }
    if (len<=0) {
        printf("�������������ݷ���\n");
        close(sockfd);
        return 0;		
    }
    cfg[len] = 0;
    LOG_MSG("�����������嵥...[ok]");
    LOG_MSG("�������嵥����: [%s]", cfg);

    /*�������ݽ�����ʱ�ļ�*/
    snprintf(tmpFile, sizeof(tmpFile), "%s/sync/tmp/tpbus.new.zip", etc);
    if (!(fp = fopen(tmpFile, "w"))) {
        LOG_ERR("fopen(%s) error: %s", tmpFile, strerror(errno));
        LOG_ERR("�������ݽ�����ʱ�ļ�����");
        goto ERR_RET;
    }
    LOG_MSG("�������ݽ�����ʱ�ļ�...[ok]");

    LOG_MSG("��ʼ�������������ݰ�...");
    while (1) {
        if ((len = tcp_recv(sockfd, buf, 0, 4, 0)) == -1) {
            LOG_ERR("tcp_recv() error: %s", strerror(errno));
            LOG_ERR("�������������ݰ�����");
            goto ERR_RET;
        }
        LOG_MSG("�յ����ݰ�[len=%d]", len);
        if (fwrite(buf, 1, len, fp) != len) {
            LOG_ERR("fwrite() error: д�����ݲ�����");
            LOG_ERR("д��ʱ�ļ�����");
            goto ERR_RET;
        }
        if (len<1024) break;
    }
    LOG_MSG("���������ݰ��������");
    fclose(fp);
    close(sockfd);

    /*���ݵ�ǰ��������*/
    snprintf(buf, sizeof(buf), "cd %s\nzip -r -q %s/sync/bak/tpbus.old.zip %s\ncd", etc, etc,
                               "conf.xml macro.xml TPTASK TPPORT TPSERV TPPROC TPMAPP TPROUT TPCONV BITMAP");
    if (system(buf) != 0) {
        LOG_ERR("���ݵ�ǰ�������ݳ���");
        goto ERR_RET;
    }
    LOG_MSG("���ݵ�ǰ��������...[ok]");   

    /*�����嵥ɾ����ǰ��������*/
    if (strstr(cfg, "TPTASK")) {
        snprintf(buf, sizeof(buf), "rm -rf %s/TPTASK", etc);
        system(buf);
    }
    if (strstr(cfg, "TPPORT")) {
        snprintf(buf, sizeof(buf), "rm -rf %s/TPPORT", etc);
        system(buf);
    }
    if (strstr(cfg, "TPSERV")) {
        snprintf(buf, sizeof(buf), "rm -rf %s/TPSERV", etc);
        system(buf);
    }
    if (strstr(cfg, "TPPROC")) {
        snprintf(buf, sizeof(buf), "rm -rf %s/TPPROC", etc);
        system(buf);
    }
    if (strstr(cfg, "TPMAPP")) {
        snprintf(buf, sizeof(buf), "rm -rf %s/TPMAPP", etc);
        system(buf);
    }
    if (strstr(cfg, "TPROUT")) {
        snprintf(buf, sizeof(buf), "rm -rf %s/TPROUT", etc);
        system(buf);
    }
    if (strstr(cfg, "TPCONV")) {
        snprintf(buf, sizeof(buf), "rm -rf %s/TPCONV", etc);
        system(buf);
    }
    if (strstr(cfg, "BITMAP")) {
        snprintf(buf, sizeof(buf), "rm -rf %s/BITMAP", etc);
        system(buf);
    }

    snprintf(buf, sizeof(buf), "unzip -o -q -d %s %s/sync/tmp/tpbus.new.zip", etc, etc);
    if (system(buf) != 0) {
        LOG_ERR("�����ϵ��������ݳ���");
        flag = 1; goto ERR_RET;
    }
    LOG_MSG("�����ϵ���������...[ok]");

    time(&t);
    snprintf(buf, sizeof(buf), "mv %s/sync/bak/tpbus.old.zip %s/sync/bak/tpbus.zip.%ld", etc, etc, t);
    system(buf);
    return 0;

ERR_RET:
    if (flag) { /*�ָ���ǰ��������*/
        snprintf(buf, sizeof(buf), "unzip -o -q -d %s %s/sync/bak/tpbus.old.zip", etc, etc);
        system(buf);
    } else {
        if (fp) fclose(fp);
        if (sockfd != -1) close(sockfd);
    }
    snprintf(buf, sizeof(buf), "rm -f %s/sync/bak/tpbus.old.zip", etc);
    system(buf);
    return -1;
}

/***************************************************************
 * getLogLevel
 **************************************************************/
static int getLogLevel(char *buf)
{
    strTrim(buf);

    if (strcmp("ERROR", buf) == 0) return ERROR;
    if (strcmp("WARN", buf)  == 0) return WARN;
    if (strcmp("INFO", buf)  == 0) return INFO;
    if (strcmp("DEBUG", buf) == 0) return DEBUG;
    return -1;
}

/***************************************************************
 * getFmtType
 **************************************************************/
static int getFmtType(char *buf)
{
    strTrim(buf);

    if (strcmp("NVL", buf)     == 0) return MSG_NVL;
    if (strcmp("CSV", buf)     == 0) return MSG_CSV;
    if (strcmp("TLV", buf)     == 0) return MSG_TLV;
    if (strcmp("FML", buf)     == 0) return MSG_FML;
    if (strcmp("XML", buf)     == 0) return MSG_XML;
    if (strcmp("ISO8583", buf) == 0) return MSG_ISO8583;
    if (strcmp("CUSTOM", buf)  == 0) return MSG_CUSTOM;
    return -1;
}

/***************************************************************
 * getFldType
 **************************************************************/
static int getFldType(char *buf)
{
    strTrim(buf);

    if (strcmp("fa", buf)   == 0) return 1;
    if (strcmp("fn", buf)   == 0) return 2;
    if (strcmp("fs", buf)   == 0) return 3;
    if (strcmp("fan", buf)  == 0) return 4;
    if (strcmp("fas", buf)  == 0) return 5;
    if (strcmp("fns", buf)  == 0) return 6;
    if (strcmp("fans", buf) == 0) return 7;
    if (strcmp("fx", buf)   == 0) return 8;
    if (strcmp("fb", buf)   == 0) return 9;

    if (strcmp("va", buf)   == 0) return 10;
    if (strcmp("vn", buf)   == 0) return 11;
    if (strcmp("vs", buf)   == 0) return 12;
    if (strcmp("van", buf)  == 0) return 13;
    if (strcmp("vas", buf)  == 0) return 14;
    if (strcmp("vns", buf)  == 0) return 15;
    if (strcmp("vans", buf) == 0) return 16;
    if (strcmp("vz", buf)   == 0) return 17;

    return -1;
}

/***************************************************************
 * getLenType
 **************************************************************/
static int getLenType(char *buf)
{
    strTrim(buf);

    if (strcmp("ASC", buf) == 0) return ASCLEN;
    if (strcmp("BCD", buf) == 0) return BCDLEN;
    if (strcmp("HEX", buf) == 0) return HEXLEN;
    return -1;
}

/***************************************************************
 * getFillType
 **************************************************************/
static int getFillType(char *buf)
{
    strTrim(buf);

    if (strcmp("LF_ZERO",  buf) == 0) return 1;
    if (strcmp("RF_ZERO",  buf) == 0) return 2;
    if (strcmp("LF_SPACE", buf) == 0) return 3;
    if (strcmp("RF_SPACE", buf) == 0) return 4;

    return -1;
}

/***************************************************************
 * getMacro
 **************************************************************/
static int getMacro(char *name, char *result)
{
    int i,n,len,size=0;
    char pathFile[256],nodePath[256];
    char key[MAXKEYLEN+1],buf[MAXEXPLEN+1];
    XMLDOC *doc;

    if (!m_macroDefs) {
        snprintf(pathFile, sizeof(pathFile), "%s/etc/macro.xml", getenv("TPBUS_HOME"));
        if (!(doc = loadXMLFile(pathFile))) {
            LOG_ERR("loadXMLFile() error");
            return -1;
        }

        if ((n = getXMLNodeCount(doc, "list/macro")) < 0) {
            LOG_ERR("getXMLNodeCount() error");
            goto ERR_RET;
        }

        for (i=0; i<n; i++) {
            /*name*/
            snprintf(nodePath, sizeof(nodePath), "list/macro[%d]/name", i+1);
            if (getXMLText(doc, nodePath, key, sizeof(key)) != 0) {
                LOG_ERR("getXMLText() error");
                goto ERR_RET;
            }
            strTrim(key);
            if (!key[0]) continue;

            len = strlen(key)+1;
            if (!m_macroDefs) {
                if (!(m_macroDefs = (char *)malloc(size+len))) {
                    LOG_ERR("malloc() error: %s", strerror(errno));
                    goto ERR_RET;
                }
            } else {
                if (!(m_macroDefs = (char *)realloc(m_macroDefs, size+len))) {
                    LOG_ERR("realloc() error: %s", strerror(errno));
                    goto ERR_RET;
                }
            }
            memcpy(m_macroDefs+size, key, len);
            size += len;

            /*value*/
            snprintf(nodePath, sizeof(nodePath), "list/macro[%d]/value", i+1);
            if (getXMLText(doc, nodePath, buf, sizeof(buf)) != 0) {
                LOG_ERR("getXMLText() error");
                goto ERR_RET;
            }
            strTrim(buf);
            if (!buf[0]) {
                LOG_ERR("�궨��[%s]��ֵ����Ϊ��", key);
                goto ERR_RET;
            }

            len = strlen(buf);
            if (!(m_macroDefs = (char *)realloc(m_macroDefs, size+len+2))) {
                LOG_ERR("realloc() error: %s", strerror(errno));
                goto ERR_RET;
            }
            m_macroDefs[size]   = len / 255;
            m_macroDefs[size+1] = len % 255;
            size += 2;

            memcpy(m_macroDefs+size, buf, len);
            size += len;
        }
        freeXML(doc);
        if (!(m_macroDefs = (char *)realloc(m_macroDefs, size+1))) {
            LOG_ERR("realloc() error: %s", strerror(errno));
            goto ERR_RET;
        }
        m_macroDefs[size++] = 0;
    }
    return tpGetParamString(m_macroDefs, name, result);

ERR_RET:
    freeXML(doc);
    if (m_macroDefs) free(m_macroDefs);
    return -1;
}

/***************************************************************
 * getFunIndex
 **************************************************************/
static int getFunIndex(char *key)
{
    int i;

    if (!m_tpFunList) {
        if ((m_tpFunSize = tpFunGetList(&m_tpFunList)) <= 0) {
            LOG_ERR("�޷���ýű�������Ϣ");
            return -1;
        }
    }
    for (i=0; i<m_tpFunSize; i++) {
        if (strcmp(key, m_tpFunList[i].fn) == 0) break;
    }
    if (i == m_tpFunSize) return -1;
    return i;
}

/***************************************************************
 * preScanExpress
 **************************************************************/
static void preScanExpress(char *result, char *express, char *error)
{
    int i,j,len,flag;
    char *p,*ch;
    char key[MAXNAMLEN+1];
    char keyVal[MAXEXPLEN+1];
    char tmpBuf[MAXEXPLEN+1];

    error[0] = 0;

    /*��1��ɨ��: �滻�ض��ַ�(@��$)*/
    len = 0;
    flag = 0;
    ch = express;
    while (*ch) {
        switch (*ch) {
        case '@':
            p = ++ch;
            while (1) {
                switch (*ch) {
                case '@':
                case '$':
                case '(':
                    strcpy(error, "�﷨����");
                    return;
                case ',':
                case '+':
                case ')':
                    if ((j = memTrimCopy(key, p, ch-p)) <= 0) {
                        strcpy(error, "�﷨����");
                        return;
                    }
                    key[j] = 0;
                    len -= ch-p;
                    if (getMacro(key, keyVal) != 0) {
                        sprintf(error, "�궨��[%s]������", key);
                        return;
                    }
                    strcpy(tmpBuf+len, keyVal);
                    len += strlen(keyVal);
                    tmpBuf[len++] = *(ch++);
                    flag = 1;
                    break;
                default:
                    tmpBuf[len++] = *(ch++);
                    break;
                }
                if (flag) {
                    flag = 0;
                    break;
                }
                if (!(*ch)) {
                    if ((j = memTrimCopy(key, p, ch-p)) <= 0) {
                        strcpy(error, "�﷨����");
                        return;
                    }
                    key[j] = 0;
                    len -= ch-p;
                    if (getMacro(key, keyVal) != 0) {
                        sprintf(error, "�궨��[%s]������", key);
                        return;
                    }
                    strcpy(tmpBuf+len, keyVal);
                    len += strlen(keyVal);
                    break;
                }
            }       
            break;
        case '$':
            ch++;
            memcpy(tmpBuf+len, "CTX(", 4);
            len += 4;
            while (1) {
                switch (*ch) {
                case '@':
                case '$':
                case '(':
                    strcpy(error, "�﷨����");
                    return;
                case ',':
                case '+':
                case ')':
                    tmpBuf[len++] = ')';
                    tmpBuf[len++] = *(ch++);
                    flag = 1;
                    break;
                default:
                    tmpBuf[len++] = *(ch++);
                    break;
                }
                if (flag) {
                    flag = 0;
                    break;
                }
                if (!(*ch)) {
                    tmpBuf[len++] = ')';
                    break;
                }
            }
            break;
        default :
            tmpBuf[len++] = *(ch++);
            break;
        }        
    }
    tmpBuf[len] = 0;

    /*��2��ɨ��: ����ű���������ֵ*/
    len = 0;
    flag = 0;
    ch = tmpBuf;
    while (*ch) {
        switch (*ch) {
        case '(':
            flag++;
            for (i=len; i>0; i--) {
                if (result[i-1] == ')') {
                    strcpy(error, "�﷨����");
                    return;
                }
                if (result[i-1] == '(' || result[i-1] == ',') break;
            }
            if ((j = memTrimCopy(key, result+i, len-i)) <= 0) {
                strcpy(error, "�﷨����");
                return;
            }
            key[j] = 0;
            if ((j = getFunIndex(key)) < 0) {
                sprintf(error, "�ű�����[%s]������", key);
                return;
            }
            sprintf(result+i, "%d", j);
            len = i+strlen(result+i);
            result[len++] = *(ch++);
            break;
        case ')':
            flag--;
            result[len++] = *(ch++);
            break;
        default :
            result[len++] = *(ch++);
            break;
        }        
    }
    if (flag) {
        strcpy(error, "��������ƥ��");
        return;
    }
    result[len] = 0;
}

/***************************************************************
 * tpConfLoad
 **************************************************************/
int tpConfLoad(SHM_ENTRY *pShmEntry)
{
    int i,j,n;
    XMLDOC *doc;
    char pathFile[256],nodePath[256],buf[1024];

    snprintf(pathFile, sizeof(pathFile), "%s/etc/conf.xml", getenv("TPBUS_HOME"));
    if ((doc = loadXMLFile(pathFile)) == NULL) {
        LOG_ERR("loadXMLFile() error");
        return -1;
    }
    if (!(pShmEntry->ptpConf = (TPCONF *)malloc(sizeof(TPCONF)))) {
        LOG_ERR("malloc() error: %s", strerror(errno));
        goto ERR_RET;
    }
    memset(pShmEntry->ptpConf, 0, sizeof(TPCONF));

    /*nMaxTrans*/
    if (getXMLText(doc, "nMaxTrans", buf, sizeof(buf)) != 0) {
        LOG_ERR("getXMLText() error");
        goto ERR_RET;
    }
    pShmEntry->ptpConf->nMaxTrans = atoi(buf);

    /*nMaxProcs*/
    if (getXMLText(doc, "nMaxProcs", buf, sizeof(buf)) != 0) {
        LOG_ERR("getXMLText() error");
        goto ERR_RET;
    }
    pShmEntry->ptpConf->nMaxProcs = atoi(buf);

    /*nRevProcs*/
    if (getXMLText(doc, "nRevProcs", buf, sizeof(buf)) != 0) {
        LOG_ERR("getXMLText() error");
        goto ERR_RET;
    }
    pShmEntry->ptpConf->nRevProcs = atoi(buf);

    /*nMonProcs*/
    if (getXMLText(doc, "nMonProcs", buf, sizeof(buf)) != 0) {
        LOG_ERR("getXMLText() error");
        goto ERR_RET;
    }
    pShmEntry->ptpConf->nMonProcs = atoi(buf);

    /*ntsMonLog*/
    if (getXMLText(doc, "ntsMonLog", buf, sizeof(buf)) != 0) {
        LOG_ERR("getXMLText() error");
        goto ERR_RET;
    }
    pShmEntry->ptpConf->ntsMonLog = atoi(buf);

    /*ntsMonSaf*/
    if (getXMLText(doc, "ntsMonSaf", buf, sizeof(buf)) != 0) {
        LOG_ERR("getXMLText() error");
        goto ERR_RET;
    }
    pShmEntry->ptpConf->ntsMonSaf = atoi(buf);

    /*ntsSysMon*/
    if (getXMLText(doc, "ntsSysMon", buf, sizeof(buf)) != 0) {
        LOG_ERR("getXMLText() error");
        goto ERR_RET;
    }
    pShmEntry->ptpConf->ntsSysMon = atoi(buf);

    /*sysMonPort*/
    if (getXMLText(doc, "sysMonPort", buf, sizeof(buf)) != 0) {
        LOG_ERR("getXMLText() error");
        goto ERR_RET;
    }
    pShmEntry->ptpConf->sysMonPort = atoi(buf);

    /*webMonPort*/
    if (getXMLText(doc, "webMonPort", buf, sizeof(buf)) != 0) {
        LOG_ERR("getXMLText() error");
        goto ERR_RET;
    }
    pShmEntry->ptpConf->webMonPort = atoi(buf);

    /*hisLogFlag*/
    if (getXMLText(doc, "hisLogFlag", buf, sizeof(buf)) != 0) {
        LOG_ERR("getXMLText() error");
        goto ERR_RET;
    }
    pShmEntry->ptpConf->hisLogFlag = atoi(buf);

    /*cluster*/
    if ((n = getXMLNodeCount(doc, "cluster")) <= 0) { /*�޼�Ⱥ����*/
        freeXML(doc);
        return 0;
    }

    /*dispatch*/
    if (getXMLText(doc, "cluster/dispatch", buf, sizeof(buf)) != 0) {
        LOG_ERR("getXMLText() error");
        goto ERR_RET;
    }
    strTrim(buf);
    if (!buf[0]) {
        LOG_ERR("��Ⱥ�ַ����й�����δ����");
        goto ERR_RET;
    }
    strncpy(pShmEntry->ptpConf->dispatch, strTrim(buf), MAXQMNLEN);

    /*hosts*/
    if ((n = getXMLNodeCount(doc, "cluster/hosts/host")) < 0) {
        LOG_ERR("getXMLNodeCount() error");
        goto ERR_RET;
    }
    if (n>NMAXHOSTS) {
        LOG_ERR("��Ⱥ�������������������[%d]", NMAXHOSTS);
        goto ERR_RET;
    }
    for (i=0; i<n; i++) {
        snprintf(nodePath, sizeof(nodePath), "cluster/hosts/host[%d]", i+1);

        /*id*/
        if (getXMLProp(doc, nodePath, "id", buf, sizeof(buf)) != 0) {
            LOG_ERR("getXMLProp() error");
            goto ERR_RET;
        }
        j = atoi(buf);
        if (j<1 || j>NMAXHOSTS) {
            LOG_ERR("��Ⱥ������[id]ֵ����ȷ");
            goto ERR_RET;
        }
        if (pShmEntry->ptpConf->hosts[j-1].id) {
            LOG_ERR("��Ⱥ����[%d]�ظ�����", j);
            goto ERR_RET;
        }
        pShmEntry->ptpConf->hosts[j-1].id = j; 

        /*qm*/
        if (getXMLProp(doc, nodePath, "qm", buf, sizeof(buf)) != 0) {
            LOG_ERR("getXMLProp() error");
            goto ERR_RET;
        }
        strTrim(buf);
        if (!buf[0]) {
            LOG_ERR("��Ⱥ����[#d]��[qm]ֵ����ȷ", j);
            goto ERR_RET;
        }
        strncpy(pShmEntry->ptpConf->hosts[j-1].qm, buf, MAXQMNLEN);
    }

    freeXML(doc);
    return 0;

ERR_RET:
    freeXML(doc);
    return -1;
}

/***************************************************************
 * tpTaskLoad
 **************************************************************/
int tpTaskLoad(SHM_ENTRY *pShmEntry)
{
    FILE *pp;
    XMLDOC *doc;
    int i,j,n,flag,nItems=0;
    int len,tmpLen,maxLen;
    char pathFile[256],nodePath[256],buf[1024*4],key[30],keyVal[100];
    TPTASK tmpTask;

    snprintf(pathFile, sizeof(pathFile), "ls %s/etc/TPTASK/*.xml", getenv("TPBUS_HOME"));
    if ((pp = popen(pathFile, "r")) == NULL) {
        LOG_ERR("popen() error: %s", strerror(errno));
        return -1;
    }

    while (fgets(pathFile, sizeof(pathFile), pp) != NULL) {
        if ((doc = loadXMLFile(strTrim(pathFile))) == NULL) {
            LOG_ERR("loadXMLFile() error");
            pclose(pp);
            return -1;
        }

        /*disabled*/
        if (getXMLText(doc, "disabled", buf, sizeof(buf)) != 0) {
            LOG_ERR("getXMLText() error");
            goto ERR_RET;
        }
        if ((flag = atoi(buf))) {
            freeXML(doc);
            continue;
        }
        nItems++;

        if (!pShmEntry->ptpTask) {
            if (!(pShmEntry->ptpTask = (TPTASK *)malloc(nItems*sizeof(TPTASK)))) {
                LOG_ERR("malloc() error: %s", strerror(errno));
                goto ERR_RET;
            }
        } else {
            if (!(pShmEntry->ptpTask = (TPTASK *)realloc(pShmEntry->ptpTask, nItems*sizeof(TPTASK)))) {
                LOG_ERR("realloc() error: %s", strerror(errno));
                goto ERR_RET;
            }
        }
        memset(&pShmEntry->ptpTask[nItems-1], 0, sizeof(TPTASK));
        pShmEntry->ptpTask[nItems-1].disabled = flag;

        /*taskId*/
        if (getXMLText(doc, "taskId", buf, sizeof(buf)) != 0) {
            LOG_ERR("getXMLText() error");
            goto ERR_RET;
        }
        pShmEntry->ptpTask[nItems-1].taskId = atoi(buf);
        if (pShmEntry->ptpTask[nItems-1].taskId <= 0) {
            LOG_ERR("[%s]: [taskId]��ֵ����ȷ", pathFile);
            goto ERR_RET;
        }

        /*taskName*/
        if (getXMLText(doc, "taskName", buf, sizeof(buf)) != 0) {
            LOG_ERR("getXMLText() error");
            goto ERR_RET;
        }
        strncpy(pShmEntry->ptpTask[nItems-1].taskName, strTrim(buf), MAXNAMLEN);

        /*bindQ*/
        if (getXMLText(doc, "bindQ", buf, sizeof(buf)) != 0) {
            LOG_ERR("getXMLText() error");
            goto ERR_RET;
        }
        pShmEntry->ptpTask[nItems-1].bindQ = atoi(buf);

        /*pathFile*/
        if (getXMLText(doc, "pathFile", buf, sizeof(buf)) != 0) {
            LOG_ERR("getXMLText() error");
            goto ERR_RET;
        }
        strncpy(pShmEntry->ptpTask[nItems-1].pathFile, buf, sizeof(pShmEntry->ptpTask[0].pathFile)-1);
        if (!pShmEntry->ptpTask[nItems-1].pathFile[0]) {
            LOG_ERR("[%s]: [pathFile]��ֵ����ȷ");
            goto ERR_RET;
        }

        /*params*/
        if ((n = getXMLNodeCount(doc, "params/param")) < 0) {
            LOG_ERR("getXMLNodeCount() error");
            goto ERR_RET;
        }
        len = 0;
        maxLen = sizeof(pShmEntry->ptpTask[0].params);
        for (i=0; i<n; i++) {
            /*name*/
            snprintf(nodePath, sizeof(nodePath), "params/param[%d]/name", i+1);
            if (getXMLText(doc, nodePath, buf, sizeof(buf)) != 0) {
                LOG_ERR("getXMLText() error");
                goto ERR_RET;
            }
            strTrim(buf);
            if (!buf[0]) continue;
            tmpLen = strlen(buf)+1;
            if (maxLen < len+tmpLen) {
                LOG_ERR("[%s]: [param]̫�����ֵ̫��", pathFile);
                goto ERR_RET;
            }
            memcpy(pShmEntry->ptpTask[nItems-1].params+len, buf, tmpLen);
            len += tmpLen;

            /*value*/
            snprintf(nodePath, sizeof(nodePath), "params/param[%d]/value", i+1);
            if (getXMLText(doc, nodePath, buf, sizeof(buf)) != 0) {
                LOG_ERR("getXMLText() error");
                goto ERR_RET;
            }
            strTrim(buf);
            tmpLen = strlen(buf);
            if(buf[0]=='@') /*���滻*/
		{
               memcpy(key,buf+1,tmpLen-1);
               key[tmpLen-1]=0;
                    if (getMacro(key, keyVal) != 0) {
                        LOG_ERR("�궨��[%s]������", key);
                        goto ERR_RET;
                    }
               strcpy(buf,keyVal);
		}
            tmpLen = strlen(buf);
            if (maxLen < len+tmpLen+2) {
                LOG_ERR("[%s]: [param]̫�����ֵ̫��", pathFile);
                goto ERR_RET;
            }
            pShmEntry->ptpTask[nItems-1].params[len]   = tmpLen / 255;
            pShmEntry->ptpTask[nItems-1].params[len+1] = tmpLen % 255;
            len += 2;
            memcpy(pShmEntry->ptpTask[nItems-1].params+len, buf, tmpLen);
            len += tmpLen;
        }

        /*maxRestNum*/
        if (getXMLText(doc, "maxRestNum", buf, sizeof(buf)) != 0) {
            LOG_ERR("getXMLText() error");
            goto ERR_RET;
        }
        pShmEntry->ptpTask[nItems-1].maxRestNum = atoi(buf);

        /*logLevel*/
        if (getXMLText(doc, "logLevel", buf, sizeof(buf)) != 0) {
            LOG_ERR("getXMLText() error");
            goto ERR_RET;
        }
        pShmEntry->ptpTask[nItems-1].logLevel = getLogLevel(buf);
        if (pShmEntry->ptpTask[nItems-1].logLevel < 0) {
            LOG_ERR("[%s]: [logLevel]��ֵ����ȷ", pathFile);
            goto ERR_RET;
        }
        freeXML(doc);
    }
    pclose(pp);
    pShmEntry->pShmList->tpTask.nItems = nItems;
    for (i=0; i<nItems-1; i++) {
        for (j=0; j<nItems-i-1; j++) {
            if (pShmEntry->ptpTask[j].taskId > pShmEntry->ptpTask[j+1].taskId) {
                memcpy(&tmpTask, &pShmEntry->ptpTask[j], sizeof(TPTASK));
                memcpy(&pShmEntry->ptpTask[j], &pShmEntry->ptpTask[j+1], sizeof(TPTASK));
                memcpy(&pShmEntry->ptpTask[j+1], &tmpTask, sizeof(TPTASK));
            }
        }
    }
    return 0;

ERR_RET:
    freeXML(doc);
    pclose(pp);
    return -1;
}

/***************************************************************
 * tpPortLoad
 **************************************************************/
int tpPortLoad(SHM_ENTRY *pShmEntry)
{
    FILE *pp;
    XMLDOC *doc;
    int q,nItems=0;
    char pathFile[256],buf[1024],error[256];
    char tmpBuf[MAXEXPLEN+1];

    snprintf(pathFile, sizeof(pathFile), "ls %s/etc/TPPORT/*.xml", getenv("TPBUS_HOME"));
    if ((pp = popen(pathFile, "r")) == NULL) {
        LOG_ERR("popen() error: %s", strerror(errno));
        return -1;
    }

    nItems = NMAXBINDQ;
    if (!(pShmEntry->ptpPort = (TPPORT *)malloc(nItems*sizeof(TPPORT)))) {
        LOG_ERR("malloc() error: %s", strerror(errno));
        pclose(pp);
        return -1;
    }
    memset(pShmEntry->ptpPort, 0, nItems*sizeof(TPPORT));

    while (fgets(pathFile, sizeof(pathFile), pp) != NULL) {
        if ((doc = loadXMLFile(strTrim(pathFile))) == NULL) {
            LOG_ERR("loadXMLFile() error");
            pclose(pp);
            return -1;
        }

        /*bindQ*/
        if (getXMLText(doc, "bindQ", buf, sizeof(buf)) != 0) {
            LOG_ERR("getXMLText() error");
            goto ERR_RET;
        }
        q = atoi(buf);
        if (q != Q_BIZ) {
            if (q < NMINBINDQ || q > NMAXBINDQ) {
                LOG_ERR("[%s]: [bindQ]��ֵ����ȷ", pathFile);
                goto ERR_RET;
            }
        }
        pShmEntry->ptpPort[q-1].bindQ = q;

        /*portName*/
        if (getXMLText(doc, "portName", buf, sizeof(buf)) != 0) {
            LOG_ERR("getXMLText() error");
            goto ERR_RET;
        }
        strncpy(pShmEntry->ptpPort[q-1].portName, buf, MAXNAMLEN);

        /*portType*/
        if (getXMLText(doc, "portType", buf, sizeof(buf)) != 0) {
            LOG_ERR("getXMLText() error");
            goto ERR_RET;
        }
        pShmEntry->ptpPort[q-1].portType = atoi(buf);
        if (q == Q_BIZ) {
            if (pShmEntry->ptpPort[q-1].portType != 2) {
                LOG_ERR("[%s]: [portType]��ֵ����ȷ", pathFile);
                goto ERR_RET;
            }
        } else {
            if (pShmEntry->ptpPort[q-1].portType < 0 || pShmEntry->ptpPort[q-1].portType > 3) {
                LOG_ERR("[%s]: [portType]��ֵ����ȷ", pathFile);
                goto ERR_RET;
            }
        }

        if (pShmEntry->ptpPort[q-1].portType != 2) {
            /*fmtType*/
            if (getXMLText(doc, "msgFmt/fmtType", buf, sizeof(buf)) != 0) {
                LOG_ERR("getXMLText() error");
                goto ERR_RET;
            }
            pShmEntry->ptpPort[q-1].fmtDefs.fmtType = getFmtType(buf);
            if (pShmEntry->ptpPort[q-1].fmtDefs.fmtType < 0) {
                LOG_ERR("[%s]: [fmtType]��ֵ����ȷ", pathFile);
                goto ERR_RET;
            }

            /*fmtProp*/
            switch (pShmEntry->ptpPort[q-1].fmtDefs.fmtType) {
                case MSG_CSV:
                    /*fsTag*/
                    if (getXMLText(doc, "msgFmt/fmtProp/fsTag", buf, sizeof(buf)) != 0) {
                        LOG_ERR("getXMLText() error");
                        goto ERR_RET;
                    }
                    preScanExpress(pShmEntry->ptpPort[q-1].fmtDefs.fmtProp.csv.fsTag, buf, error);
                    if (error[0]) {
                        LOG_ERR("[%s]: [fsTag]%s", pathFile, error);
                        goto ERR_RET;
                    }
                    pShmEntry->ptpPort[q-1].fmtDefs.fmtProp.csv.fsLen = tpFunExec(pShmEntry->ptpPort[q-1].fmtDefs.fmtProp.csv.fsTag, tmpBuf, sizeof(tmpBuf));
                    if (pShmEntry->ptpPort[q-1].fmtDefs.fmtProp.csv.fsLen < 0) {
                        LOG_ERR("����[fsTag]���ʽ����");
                        goto ERR_RET;
                    }
                    memcpy(pShmEntry->ptpPort[q-1].fmtDefs.fmtProp.csv.fsTag, tmpBuf, pShmEntry->ptpPort[q-1].fmtDefs.fmtProp.csv.fsLen);

                    /*rsTag*/
                    if (getXMLText(doc, "msgFmt/fmtProp/rsTag", buf, sizeof(buf)) != 0) {
                        LOG_ERR("getXMLText() error");
                        goto ERR_RET;
                    }
                    preScanExpress(pShmEntry->ptpPort[q-1].fmtDefs.fmtProp.csv.rsTag, buf, error);
                    if (error[0]) {
                        LOG_ERR("[%s]: [rsTag]%s", pathFile, error);
                        goto ERR_RET;
                    }
                    pShmEntry->ptpPort[q-1].fmtDefs.fmtProp.csv.rsLen = tpFunExec(pShmEntry->ptpPort[q-1].fmtDefs.fmtProp.csv.rsTag, tmpBuf, sizeof(tmpBuf));
                    if (pShmEntry->ptpPort[q-1].fmtDefs.fmtProp.csv.rsLen < 0) {
                        LOG_ERR("����[rsTag]���ʽ����");
                        goto ERR_RET;
                    }
                    memcpy(pShmEntry->ptpPort[q-1].fmtDefs.fmtProp.csv.rsTag, tmpBuf, pShmEntry->ptpPort[q-1].fmtDefs.fmtProp.csv.rsLen);

                    /*gsTag*/
                    if (getXMLText(doc, "msgFmt/fmtProp/gsTag", buf, sizeof(buf)) != 0) {
                        LOG_ERR("getXMLText() error");
                        goto ERR_RET;
                    }
                    preScanExpress(pShmEntry->ptpPort[q-1].fmtDefs.fmtProp.csv.gsTag, buf, error);
                    if (error[0]) {
                        LOG_ERR("[%s]: [gsTag]%s", pathFile, error);
                        goto ERR_RET;
                    }
                    pShmEntry->ptpPort[q-1].fmtDefs.fmtProp.csv.gsLen = tpFunExec(pShmEntry->ptpPort[q-1].fmtDefs.fmtProp.csv.gsTag, tmpBuf, sizeof(tmpBuf));
                    if (pShmEntry->ptpPort[q-1].fmtDefs.fmtProp.csv.gsLen < 0) {
                        LOG_ERR("����[gsTag]���ʽ����");
                        goto ERR_RET;
                    }
                    memcpy(pShmEntry->ptpPort[q-1].fmtDefs.fmtProp.csv.gsTag, tmpBuf, pShmEntry->ptpPort[q-1].fmtDefs.fmtProp.csv.gsLen);

                    break;
                case MSG_ISO8583:
                    /*bitmap*/
                    if (getXMLText(doc, "msgFmt/fmtProp/bitmap", buf, sizeof(buf)) != 0) {
                        LOG_ERR("getXMLText() error");
                        goto ERR_RET;
                    }
                    strcpy(pShmEntry->ptpPort[q-1].fmtDefs.fmtProp.iso8583.bitmap, buf);

                    /*macFun*/
                    if (getXMLText(doc, "msgFmt/fmtProp/macFun", buf, sizeof(buf)) != 0) {
                        LOG_ERR("getXMLText() error");
                        goto ERR_RET;
                    }
                    preScanExpress(pShmEntry->ptpPort[q-1].fmtDefs.fmtProp.iso8583.macFun, buf, error);
                    if (error[0]) {
                        LOG_ERR("[%s]: [macFun]%s", pathFile, error);
                        goto ERR_RET;
                    }
                    break;
                case MSG_NVL:
                case MSG_TLV:
                case MSG_FML:
                case MSG_XML:
                case MSG_CUSTOM:
                    break;
                default:
                    LOG_ERR("[%s]: [fmtType]��ֵ����ȷ", pathFile);
                    goto ERR_RET;
                    break;
            }

            /*unpkFun*/
            if (getXMLText(doc, "msgFmt/unpkFun", buf, sizeof(buf)) != 0) {
                LOG_ERR("getXMLText() error");
                goto ERR_RET;
            }
            preScanExpress(pShmEntry->ptpPort[q-1].fmtDefs.unpkFun, buf, error);
            if (error[0]) {
                LOG_ERR("[%s]: [unpkFun]%s", pathFile, error);
                goto ERR_RET;
            }

            /*packFun*/
            if (getXMLText(doc, "msgFmt/packFun", buf, sizeof(buf)) != 0) {
                LOG_ERR("getXMLText() error");
                goto ERR_RET;
            }
            preScanExpress(pShmEntry->ptpPort[q-1].fmtDefs.packFun, buf, error);
            if (error[0]) {
                LOG_ERR("[%s]: [packFun]%s", pathFile, error);
                goto ERR_RET;
            }

            /*freeFun*/
            if (getXMLText(doc, "msgFmt/freeFun", buf, sizeof(buf)) != 0) {
                LOG_ERR("getXMLText() error");
                goto ERR_RET;
            }
            preScanExpress(pShmEntry->ptpPort[q-1].fmtDefs.freeFun, buf, error);
            if (error[0]) {
                LOG_ERR("[%s]: [freeFun]%s", pathFile, error);
                goto ERR_RET;
            }

    }
        if (!pShmEntry->ptpPort[q-1].portType) {
            /*tranCodeKey*/
            if (getXMLText(doc, "tranCodeKey", buf, sizeof(buf)) != 0) {
                LOG_ERR("getXMLText() error");
                goto ERR_RET;
            }
            preScanExpress(pShmEntry->ptpPort[q-1].tranCodeKey, buf, error);
            if (error[0]) {
                LOG_ERR("[%s]: [tranCodeKey]%s", pathFile, error);
                goto ERR_RET;
            }

            /*convGrp*/
            if (getXMLText(doc, "convGrp", buf, sizeof(buf)) != 0) {
                LOG_ERR("getXMLText() error");
                goto ERR_RET;
            }
            pShmEntry->ptpPort[q-1].convGrp = atoi(buf);
        } else {
            /*respCodeKey*/
            if (getXMLText(doc, "respCodeKey", buf, sizeof(buf)) != 0) {
                LOG_ERR("getXMLText() error");
                goto ERR_RET;
            }
            preScanExpress(pShmEntry->ptpPort[q-1].respCodeKey, buf, error);
            if (error[0]) {
                LOG_ERR("[%s]: [respCodeKey]%s", pathFile, error);
                goto ERR_RET;
            }

            /*nMaxSaf*/
            if (getXMLText(doc, "nMaxSaf", buf, sizeof(buf)) != 0) {
                LOG_ERR("getXMLText() error");
                goto ERR_RET;
            }
            pShmEntry->ptpPort[q-1].nMaxSaf = atoi(buf);
        
            /*safDelay*/
            if (getXMLText(doc, "safDelay", buf, sizeof(buf)) != 0) {
                LOG_ERR("getXMLText() error");
                goto ERR_RET;
            }
            pShmEntry->ptpPort[q-1].safDelay = atoi(buf);
        
            /*multiRev*/
            if (getXMLText(doc, "multiRev", buf, sizeof(buf)) != 0) {
                LOG_ERR("getXMLText() error");
                goto ERR_RET;
            }
            pShmEntry->ptpPort[q-1].multiRev = atoi(buf);
        }

        if (pShmEntry->ptpPort[q-1].portType != 2) {
            /*headSaveKey*/
            if (getXMLText(doc, "headSaveKey", buf, sizeof(buf)) != 0) {
                LOG_ERR("getXMLText() error");
                goto ERR_RET;
            }
            preScanExpress(pShmEntry->ptpPort[q-1].headSaveKey, buf, error);
            if (error[0]) {
                LOG_ERR("[%s]: [headSaveKey]%s", pathFile, error);
                goto ERR_RET;
            }

            /*headLoadKey*/
            if (getXMLText(doc, "headLoadKey", buf, sizeof(buf)) != 0) {
                LOG_ERR("getXMLText() error");
                goto ERR_RET;
            }
            preScanExpress(pShmEntry->ptpPort[q-1].headLoadKey, buf, error);
            if (error[0]) {
                LOG_ERR("[%s]: [headLoadKey]%s", pathFile, error);
                goto ERR_RET;
            }

        /*timeout*/
        if (getXMLText(doc, "timeout", buf, sizeof(buf)) != 0) {
            LOG_ERR("getXMLText() error");
            goto ERR_RET;
        }
        pShmEntry->ptpPort[q-1].timeout = atoi(buf);
        if (pShmEntry->ptpPort[q-1].timeout <= 0) pShmEntry->ptpPort[q-1].timeout = 30; /*Default*/

        freeXML(doc);
      }
    }
    pShmEntry->pShmList->tpPort.nItems = nItems;
    pclose(pp);
    return 0;

ERR_RET:
    freeXML(doc);
    pclose(pp);
    return -1;
}

/***************************************************************
 * tpServLoad
 **************************************************************/
int tpServLoad(SHM_ENTRY *pShmEntry)
{
    FILE *pp;
    int  i,j,k,n,flag;
    char error[256];
    char servCode[MAXKEYLEN+1];
    char pathFile[256],nodePath[256],buf[1024];
    XMLDOC *doc;

    snprintf(pathFile, sizeof(pathFile), "ls %s/etc/TPSERV/*.xml", getenv("TPBUS_HOME"));
    if ((pp = popen(pathFile, "r")) == NULL) {
        LOG_ERR("popen() error: %s", strerror(errno));
        return -1;
    }

    while (fgets(pathFile, sizeof(pathFile), pp) != NULL) {
        if ((doc = loadXMLFile(strTrim(pathFile))) == NULL) {
            LOG_ERR("loadXMLFile() error");
            pclose(pp);
            return -1;
        }

        if ((n = getXMLNodeCount(doc, "tpServs/tpServ")) < 0) {
            LOG_ERR("getXMLNodeCount() error");
            goto ERR_RET;
        }  
    
        for (k=0; k<n; k++) {
            /*disabled*/
            snprintf(nodePath, sizeof(nodePath), "tpServs/tpServ[%d]/disabled", k+1);
            if (getXMLText(doc, nodePath, buf, sizeof(buf)) != 0) {
                LOG_ERR("getXMLText() error");
                goto ERR_RET;
            }
            if ((flag = atoi(buf))) continue;

            /*servCode*/
            snprintf(nodePath, sizeof(nodePath), "tpServs/tpServ[%d]/servCode", k+1);
            if (getXMLText(doc, nodePath, buf, sizeof(buf)) != 0) {
                LOG_ERR("getXMLText() error");
                goto ERR_RET;
            }
            strncpy(servCode, strTrim(buf), sizeof(servCode)-1);
            if (!servCode[0]) {
                LOG_ERR("[%s]: [servCode]��ֵ����ȷ", pathFile);
                goto ERR_RET;
            }

            i = j = hashValue(servCode, HASH_SIZE)+1;

            if (!pShmEntry->ptpServ) {
                pShmEntry->pShmList->tpServ.nItems = HASH_SIZE;
                pShmEntry->ptpServ = (TPSERV *)malloc(sizeof(TPSERV) * pShmEntry->pShmList->tpServ.nItems);
                if (!pShmEntry->ptpServ) {
                    LOG_ERR("malloc() error: %s", strerror(errno));
                    goto ERR_RET;
                }
                memset(pShmEntry->ptpServ, 0, (sizeof(TPSERV) * pShmEntry->pShmList->tpServ.nItems));
            } else {
                if (pShmEntry->ptpServ[i-1].servCode[0]) {
                    pShmEntry->pShmList->tpServ.nItems += 1;
                    pShmEntry->ptpServ = (TPSERV *)realloc(pShmEntry->ptpServ, (sizeof(TPSERV) * pShmEntry->pShmList->tpServ.nItems));
                    if (!pShmEntry->ptpServ) {
                        LOG_ERR("realloc() error: %s", strerror(errno));
                        goto ERR_RET;
                    }
                    i = pShmEntry->pShmList->tpServ.nItems;
                    memset(&pShmEntry->ptpServ[i-1], 0, sizeof(TPSERV));
                    while (j>0) {
                        if (strcmp(servCode, pShmEntry->ptpServ[j-1].servCode) == 0) {
                            LOG_ERR("����[%s]�ظ�����", servCode);
                            goto ERR_RET;
                        }
                        if (!pShmEntry->ptpServ[j-1].next) break;
                        j = pShmEntry->ptpServ[j-1].next;
                    }
                    pShmEntry->ptpServ[j-1].next = i;
                }
            }
            strncpy(pShmEntry->ptpServ[i-1].servCode, servCode, MAXKEYLEN);
            pShmEntry->ptpServ[i-1].disabled = flag;

            /*servName*/
            snprintf(nodePath, sizeof(nodePath), "tpServs/tpServ[%d]/servName", k+1);
            if (getXMLText(doc, nodePath, buf, sizeof(buf)) != 0) {
                LOG_ERR("getXMLText() error");
                goto ERR_RET;
            }
            strncpy(pShmEntry->ptpServ[i-1].servName, strTrim(buf), MAXNAMLEN);

            /*routGrp*/
            snprintf(nodePath, sizeof(nodePath), "tpServs/tpServ[%d]/routGrp", k+1);
            if (getXMLText(doc, nodePath, buf, sizeof(buf)) != 0) {
                LOG_ERR("getXMLText() error");
                goto ERR_RET;
            }
            pShmEntry->ptpServ[i-1].routGrp = atoi(buf);
            if (pShmEntry->ptpServ[i-1].routGrp <= 0) {
                LOG_ERR("[%s]: [routGrp]��ֵ����ȷ", pathFile);
                goto ERR_RET;
            }

            /*nMaxReq*/
            snprintf(nodePath, sizeof(nodePath), "tpServs/tpServ[%d]/nMaxReq", k+1);
            if (getXMLText(doc, nodePath, buf, sizeof(buf)) != 0) {
                LOG_ERR("getXMLText() error");
                goto ERR_RET;
            }
            pShmEntry->ptpServ[i-1].nMaxReq = atoi(buf);
            if (pShmEntry->ptpServ[i-1].nMaxReq <= 0) pShmEntry->ptpServ[i-1].nMaxReq = 100; /*Default*/

            /*monFlag*/
            snprintf(nodePath, sizeof(nodePath), "tpServs/tpServ[%d]/monFlag", k+1);
            if (getXMLNodeCount(doc, nodePath) <= 0) {
                pShmEntry->ptpServ[i-1].monFlag = 0;
                continue;
            }
            if (getXMLText(doc, nodePath, buf, sizeof(buf)) != 0) {
                LOG_ERR("getXMLText() error");
                goto ERR_RET;
            }
            pShmEntry->ptpServ[i-1].monFlag = atoi(buf);

            /*monData*/
            snprintf(nodePath, sizeof(nodePath), "tpServs/tpServ[%d]/monData", k+1);
            if (getXMLNodeCount(doc, nodePath) <= 0) {
                if (pShmEntry->ptpServ[i-1].monFlag) pShmEntry->ptpServ[i-1].monFlag = 0;
                continue;
            }

            /*monData/item*/
            snprintf(nodePath, sizeof(nodePath), "tpServs/tpServ[%d]/monData/item", k+1);
            pShmEntry->ptpServ[i-1].monItems = getXMLNodeCount(doc, nodePath);
            if (pShmEntry->ptpServ[i-1].monItems <= 0) {
                if (pShmEntry->ptpServ[i-1].monFlag == 2) pShmEntry->ptpServ[i-1].monFlag = 1;
                continue;
            }

            pShmEntry->ptpServ[i-1].monIndex = pShmEntry->pShmList->tpMoni.nItems;
            pShmEntry->pShmList->tpMoni.nItems += pShmEntry->ptpServ[i-1].monItems;
            if (!pShmEntry->ptpMoni) {
                pShmEntry->ptpMoni = (TPMONI *)malloc(sizeof(TPMONI) * pShmEntry->pShmList->tpMoni.nItems);
                if (!pShmEntry->ptpMoni) {
                    LOG_ERR("malloc() error: %s", strerror(errno));
                    goto ERR_RET;
                }
            } else {
                pShmEntry->ptpMoni = (TPMONI *)realloc(pShmEntry->ptpMoni, sizeof(TPMONI) * pShmEntry->pShmList->tpMoni.nItems);
                if (!pShmEntry->ptpMoni) {
                    LOG_ERR("realloc() error: %s", strerror(errno));
                    goto ERR_RET;
                }
            }
            memset(&pShmEntry->ptpMoni[pShmEntry->ptpServ[i-1].monIndex], 0, sizeof(TPMONI) * pShmEntry->ptpServ[i-1].monItems);  

            for (j=0; j<pShmEntry->ptpServ[i-1].monItems; j++) {
                snprintf(nodePath, sizeof(nodePath), "tpServs/tpServ[%d]/monData/item[%d]", k+1, j+1);

                /*item@id*/
                if (getXMLProp(doc, nodePath, "id", buf, sizeof(buf)) != 0) {
                    LOG_ERR("getXMLProp() error");
                    goto ERR_RET;
                }
                strTrim(buf);
                if (!buf[0]) {
                    LOG_ERR("[%s]: [monData/item@id]��ֵ����Ϊ��", pathFile);
                    goto ERR_RET;
                }
                strncpy(pShmEntry->ptpMoni[j+pShmEntry->ptpServ[i-1].monIndex].id, buf, MAXNAMLEN);

                /*item@express*/
                if (getXMLProp(doc, nodePath, "express", buf, sizeof(buf)) != 0) {
                    LOG_ERR("getXMLProp() error");
                    goto ERR_RET;
                }
                strTrim(buf);
                if (!buf[0]) {
                    LOG_ERR("[%s]: [monData/item@express]��ֵ����Ϊ��", pathFile);
                    goto ERR_RET;
                }
                preScanExpress(pShmEntry->ptpMoni[j+pShmEntry->ptpServ[i-1].monIndex].express, buf, error);
                if (error[0]) {
                    LOG_ERR("[%s]: %s", pathFile, error);
                    goto ERR_RET;
                }
            }
        }
        freeXML(doc);
    }
    pclose(pp);
    return 0;

ERR_RET:
    freeXML(doc);
    pclose(pp);
    return -1;
}

/***************************************************************
 * tpProcLoad
 **************************************************************/
int tpProcLoad(SHM_ENTRY *pShmEntry)
{
    FILE *pp;
    XMLDOC *doc;
    int i,j,k,n,q,nItems=0;
    char pathFile[256],nodePath[256],buf[1024];
    char procCode[MAXKEYLEN+1];

    snprintf(pathFile, sizeof(pathFile), "ls %s/etc/TPPROC/*.xml", getenv("TPBUS_HOME"));
    if ((pp = popen(pathFile, "r")) == NULL) {
        LOG_ERR("popen() error: %s", strerror(errno));
        return -1;
    }

    while (fgets(pathFile, sizeof(pathFile), pp) != NULL) {
        if ((doc = loadXMLFile(strTrim(pathFile))) == NULL) {
            LOG_ERR("loadXMLFile() error");
            pclose(pp);
            return -1;
        }

        /*desQ*/
        if (getXMLText(doc, "desQ", buf, sizeof(buf)) != 0) {
            LOG_ERR("getXMLText() error");
            goto ERR_RET;
        }
        q = atoi(buf);
        if (q < NMINBINDQ || q > NMAXBINDQ) {
            LOG_ERR("[%s]: [desQ]��ֵ����ȷ", pathFile);
            goto ERR_RET;
        }

        if ((n = getXMLNodeCount(doc, "tpProcs/tpProc")) < 0) {
            LOG_ERR("getXMLNodeCount() error");
            goto ERR_RET;
        }

        i = j = q;

        for (k=0; k<n; k++) {
            /*procCode*/
            snprintf(nodePath, sizeof(nodePath), "tpProcs/tpProc[%d]/procCode", k+1);
            if (getXMLText(doc, nodePath, buf, sizeof(buf)) != 0) {
                LOG_ERR("getXMLText() error");
                goto ERR_RET;
            }
            strncpy(procCode, strTrim(buf), MAXKEYLEN);
            if (!procCode[0]) {
                LOG_ERR("[%s]: [procCode]����Ϊ��", pathFile);
                goto ERR_RET;
            } 

            if (!pShmEntry->ptpProc) {
                nItems = NMAXBINDQ;
                if (!(pShmEntry->ptpProc = (TPPROC *)malloc(nItems*sizeof(TPPROC)))) {
                    LOG_ERR("malloc() error: %s", strerror(errno));
                    pclose(pp);
                    goto ERR_RET;
                }
                memset(pShmEntry->ptpProc, 0, nItems*sizeof(TPPROC));
            } else {
                if (pShmEntry->ptpProc[q-1].desQ) {
                    i = ++nItems;
                    if (!(pShmEntry->ptpProc = (TPPROC *)realloc(pShmEntry->ptpProc, nItems*sizeof(TPPROC)))) {
                        LOG_ERR("realloc() error: %s", strerror(errno));
                        goto ERR_RET;
                    }
                    memset(&pShmEntry->ptpProc[i-1], 0, sizeof(TPPROC));
                    while (j>0) {
                        if (strcmp(procCode, pShmEntry->ptpProc[j-1].procCode) == 0) {
                            LOG_ERR("Ԫ������[%s]�ظ�����", procCode);
                            goto ERR_RET;
                        }
                        if (!pShmEntry->ptpProc[j-1].next) break;
                        j = pShmEntry->ptpProc[j-1].next;			
                    }
                    pShmEntry->ptpProc[j-1].next = i;
                }
            }
            pShmEntry->ptpProc[i-1].desQ = q;
            strncpy(pShmEntry->ptpProc[i-1].procCode, procCode, MAXKEYLEN);

            /*procName*/
            snprintf(nodePath, sizeof(nodePath), "tpProcs/tpProc[%d]/procName", k+1);
            if (getXMLText(doc, nodePath, buf, sizeof(buf)) != 0) {
                LOG_ERR("getXMLText() error");
                goto ERR_RET;
            }
            strncpy(pShmEntry->ptpProc[i-1].procName, strTrim(buf), MAXKEYLEN);

            /*packGrp*/
            snprintf(nodePath, sizeof(nodePath), "tpProcs/tpProc[%d]/packGrp", k+1);
            if (getXMLText(doc, nodePath, buf, sizeof(buf)) != 0) {
                LOG_ERR("getXMLText() error");
                goto ERR_RET;
            }
            pShmEntry->ptpProc[i-1].packGrp = atoi(buf);
            if (pShmEntry->ptpProc[i-1].packGrp <= 0) {
                LOG_ERR("[%s]: [packGrp]��ֵ����ȷ", pathFile);
                goto ERR_RET;
            }

            /*unpkGrp*/
            snprintf(nodePath, sizeof(nodePath), "tpProcs/tpProc[%d]/unpkGrp", k+1);
            if (getXMLText(doc, nodePath, buf, sizeof(buf)) != 0) {
                LOG_ERR("getXMLText() error");
                goto ERR_RET;
            }
            pShmEntry->ptpProc[i-1].unpkGrp = atoi(buf);
            if (pShmEntry->ptpProc[i-1].unpkGrp <= 0) {
                LOG_ERR("[%s]: [unpkGrp]��ֵ����ȷ", pathFile);
                goto ERR_RET;
            }

            /*revPackGrp*/
            snprintf(nodePath, sizeof(nodePath), "tpProcs/tpProc[%d]/revPackGrp", k+1);
            if (getXMLText(doc, nodePath, buf, sizeof(buf)) != 0) {
                LOG_ERR("getXMLText() error");
                goto ERR_RET;
            }
            pShmEntry->ptpProc[i-1].revPackGrp = atoi(buf);

            /*revUnpkGrp*/
            snprintf(nodePath, sizeof(nodePath), "tpProcs/tpProc[%d]/revUnpkGrp", k+1);
            if (getXMLText(doc, nodePath, buf, sizeof(buf)) != 0) {
                LOG_ERR("getXMLText() error");
                goto ERR_RET;
            }
            pShmEntry->ptpProc[i-1].revUnpkGrp = atoi(buf);
        }       
        freeXML(doc);
    }
    pclose(pp);
    pShmEntry->pShmList->tpProc.nItems = nItems;
    return 0;

ERR_RET:
    freeXML(doc);
    pclose(pp);
    return -1;
}

/***************************************************************
 * tpMappLoad
 **************************************************************/
int tpMappLoad(SHM_ENTRY *pShmEntry)
{
    FILE *pp;
    XMLDOC *doc;
    int i,j,k,n,q,nItems=0;
    char pathFile[256],nodePath[256],buf[1024];
    char tranCode[MAXKEYLEN+1];

    snprintf(pathFile, sizeof(pathFile), "ls %s/etc/TPMAPP/*.xml", getenv("TPBUS_HOME"));
    if ((pp = popen(pathFile, "r")) == NULL) {
        LOG_ERR("popen() error: %s", strerror(errno));
        return -1;
    }

    while (fgets(pathFile, sizeof(pathFile), pp) != NULL) {
        if ((doc = loadXMLFile(strTrim(pathFile))) == NULL) {
            LOG_ERR("loadXMLFile() error");
            pclose(pp);
            return -1;
        }

        /*orgQ*/
        if (getXMLText(doc, "orgQ", buf, sizeof(buf)) != 0) {
            LOG_ERR("getXMLText() error");
            goto ERR_RET;
        }
        q = atoi(buf);
        if (q < NMINBINDQ || q > NMAXBINDQ) {
            LOG_ERR("[%s]: [orgQ]��ֵ����ȷ", pathFile);
            goto ERR_RET;
        }

        if ((n = getXMLNodeCount(doc, "tpMapps/tpMapp")) < 0) {
            LOG_ERR("getXMLNodeCount() error");
            goto ERR_RET;
        }

        i = j = q;

        for (k=0; k<n; k++) {
            /*tranCode*/
            snprintf(nodePath, sizeof(nodePath), "tpMapps/tpMapp[%d]/tranCode", k+1);
            if (getXMLText(doc, nodePath, buf, sizeof(buf)) != 0) {
                LOG_ERR("getXMLText() error");
                goto ERR_RET;
            }
            strncpy(tranCode, strTrim(buf), MAXKEYLEN);
            if (!tranCode[0]) {
                LOG_ERR("[%s]: [tranCode]����Ϊ��", pathFile);
                goto ERR_RET;
            } 

            if (!pShmEntry->ptpMapp) {
                nItems = NMAXBINDQ;
                if (!(pShmEntry->ptpMapp = (TPMAPP *)malloc(nItems*sizeof(TPMAPP)))) {
                    LOG_ERR("malloc() error: %s", strerror(errno));
                    goto ERR_RET;
                }
                memset(pShmEntry->ptpMapp, 0, nItems*sizeof(TPMAPP));
            } else {
                if (pShmEntry->ptpMapp[q-1].orgQ) {
                    i = ++nItems;
                    if (!(pShmEntry->ptpMapp = (TPMAPP *)realloc(pShmEntry->ptpMapp, nItems*sizeof(TPMAPP)))) {
                        LOG_ERR("realloc() error: %s", strerror(errno));
                        goto ERR_RET;
                    }
                    memset(&pShmEntry->ptpMapp[i-1], 0, sizeof(TPMAPP));
                    while (j>0) {
                        if (strcmp(tranCode, pShmEntry->ptpMapp[j-1].tranCode) == 0) {
                            LOG_ERR("�˵�[%d]������ؼ���[%s]�ظ�����", q, tranCode);
                            goto ERR_RET;
                        }
                        if (!pShmEntry->ptpMapp[j-1].next) break;
                        j = pShmEntry->ptpMapp[j-1].next;
                    }
                    pShmEntry->ptpMapp[j-1].next = i;
                }
            }
            pShmEntry->ptpMapp[i-1].orgQ = q;
            strncpy(pShmEntry->ptpMapp[i-1].tranCode, tranCode, MAXKEYLEN);

            /*servCode*/
            snprintf(nodePath, sizeof(nodePath), "tpMapps/tpMapp[%d]/servCode", k+1);
            if (getXMLText(doc, nodePath, buf, sizeof(buf)) != 0) {
                LOG_ERR("getXMLText() error");
                goto ERR_RET;
            }
            strncpy(pShmEntry->ptpMapp[i-1].servCode, strTrim(buf), MAXKEYLEN);
            if (!pShmEntry->ptpMapp[i-1].servCode[0]) {
                LOG_ERR("[%s]: [servCode]����Ϊ��", pathFile);
                goto ERR_RET;
            }

            /*isThrough*/
            snprintf(nodePath, sizeof(nodePath), "tpMapps/tpMapp[%d]/isThrough", k+1);
            if (getXMLText(doc, nodePath, buf, sizeof(buf)) != 0) {
                LOG_ERR("getXMLText() error");
                goto ERR_RET;
            }
            pShmEntry->ptpMapp[i-1].isThrough = atoi(buf);
            if (pShmEntry->ptpMapp[i-1].isThrough) continue;

            /*unpkGrp*/
            snprintf(nodePath, sizeof(nodePath), "tpMapps/tpMapp[%d]/unpkGrp", k+1);
            if (getXMLText(doc, nodePath, buf, sizeof(buf)) != 0) {
                LOG_ERR("getXMLText() error");
                goto ERR_RET;
            }
            pShmEntry->ptpMapp[i-1].unpkGrp = atoi(buf);
            if (pShmEntry->ptpMapp[i-1].unpkGrp <= 0) {
                LOG_ERR("[%s]: [unpkGrp]��ֵ����ȷ", pathFile);
                goto ERR_RET;
            }

            /*packGrp*/
            snprintf(nodePath, sizeof(nodePath), "tpMapps/tpMapp[%d]/packGrp", k+1);
            if (getXMLText(doc, nodePath, buf, sizeof(buf)) != 0) {
                LOG_ERR("getXMLText() error");
                goto ERR_RET;
            }
            pShmEntry->ptpMapp[i-1].packGrp = atoi(buf);
        }       
        freeXML(doc);
    }
    pclose(pp);
    pShmEntry->pShmList->tpMapp.nItems = nItems;
    return 0;

ERR_RET:
    freeXML(doc);
    pclose(pp);
    return -1;
}

/***************************************************************
 * bitmapLoad
 **************************************************************/
int bitmapLoad(SHM_ENTRY *pShmEntry)
{
    FILE *pp;
    XMLDOC *doc;
    int i,j,n,nItems=0;
    char pathFile[256],nodePath[256],buf[1024];

    snprintf(pathFile, sizeof(pathFile), "ls %s/etc/BITMAP/*.xml", getenv("TPBUS_HOME"));
    if ((pp = popen(pathFile, "r")) == NULL) {
        LOG_ERR("popen() error: %s", strerror(errno));
        return -1;
    }

    while (fgets(pathFile, sizeof(pathFile), pp) != NULL) {
        if ((doc = loadXMLFile(strTrim(pathFile))) == NULL) {
            LOG_ERR("loadXMLFile() error");
            pclose(pp);
            return -1;
        }
        nItems++;

        if (!pShmEntry->pBitmap) {
            if (!(pShmEntry->pBitmap = (BITMAP *)malloc(nItems*sizeof(BITMAP)))) {
                LOG_ERR("malloc() error: %s", strerror(errno));
                goto ERR_RET;
            }
        } else {
            if (!(pShmEntry->pBitmap = (BITMAP *)realloc(pShmEntry->pBitmap, nItems*sizeof(BITMAP)))) {
                LOG_ERR("realloc() error: %s", strerror(errno));
                goto ERR_RET;
            }
        }
        memset(&pShmEntry->pBitmap[nItems-1], 0, sizeof(BITMAP));

        /*id*/
        if (getXMLText(doc, "id", buf, sizeof(buf)) != 0) {
            LOG_ERR("getXMLText() error");
            goto ERR_RET;
        }
        strncpy(pShmEntry->pBitmap[nItems-1].id, strTrim(buf), MAXKEYLEN);

        /*name*/
        if (getXMLText(doc, "name", buf, sizeof(buf)) != 0) {
            LOG_ERR("getXMLText() error");
            goto ERR_RET;
        }
        strncpy(pShmEntry->pBitmap[nItems-1].name, strTrim(buf), MAXNAMLEN);

        /*nBits*/
        if (getXMLText(doc, "nBits", buf, sizeof(buf)) != 0) {
            LOG_ERR("getXMLText() error");
            goto ERR_RET;
        }
        pShmEntry->pBitmap[nItems-1].nBits = atoi(buf);
        if (pShmEntry->pBitmap[nItems-1].nBits <= 0) pShmEntry->pBitmap[nItems-1].nBits = 128; /*Default*/
        if (pShmEntry->pBitmap[nItems-1].nBits > NMAXBITS) {
            LOG_ERR("[%s]: λͼ���Ȳ��ܳ���[%d]", NMAXBITS);
            goto ERR_RET;
        }

        if ((n = getXMLNodeCount(doc, "items/item")) < 0) {
            LOG_ERR("getXMLNodeCount() error");
            goto ERR_RET;
        }
        if (n > pShmEntry->pBitmap[nItems-1].nBits) {
            LOG_ERR("[%s]: λͼ�����[%d]���ܳ���λͼ����[%d]", pathFile, n, pShmEntry->pBitmap[nItems-1].nBits);
            goto ERR_RET;
        }

        /*items*/
        for (i=0; i<n; i++) {
            /*fldId*/
            snprintf(nodePath, sizeof(nodePath), "items/item[%d]/fldId", i+1);
            if (getXMLText(doc, nodePath, buf, sizeof(buf)) != 0) {
                LOG_ERR("getXMLText() error");
                goto ERR_RET;
            }
            j = atoi(buf);
            if (j<=0 || j>pShmEntry->pBitmap[nItems-1].nBits) {
                LOG_ERR("[%s]: [fldId]��ֵ����ȷ", pathFile);
                goto ERR_RET;
            }
            pShmEntry->pBitmap[nItems-1].items[j-1].fldId = j;

            /*fldType*/
            snprintf(nodePath, sizeof(nodePath), "items/item[%d]/fldType", i+1);
            if (getXMLText(doc, nodePath, buf, sizeof(buf)) != 0) {
                LOG_ERR("getXMLText() error");
                goto ERR_RET;
            }
            pShmEntry->pBitmap[nItems-1].items[j-1].fldType = getFldType(buf);
            if (pShmEntry->pBitmap[nItems-1].items[j-1].fldType < 0) {
                LOG_ERR("[%s]: [fldType]��ֵ����ȷ", pathFile);
                goto ERR_RET;
            } 

            /*fldLen*/
            snprintf(nodePath, sizeof(nodePath), "items/item[%d]/fldLen", i+1);
            if (getXMLText(doc, nodePath, buf, sizeof(buf)) != 0) {
                LOG_ERR("getXMLText() error");
                goto ERR_RET;
            }
            pShmEntry->pBitmap[nItems-1].items[j-1].fldLen = atoi(buf);
            if (pShmEntry->pBitmap[nItems-1].items[j-1].fldLen <= 0) {
                LOG_ERR("[%s]: [fldLen]��ֵ����ȷ", pathFile);
                goto ERR_RET;
            }

            /*lenLen*/
            snprintf(nodePath, sizeof(nodePath), "items/item[%d]/lenLen", i+1);
            if (getXMLText(doc, nodePath, buf, sizeof(buf)) != 0) {
                LOG_ERR("getXMLText() error");
                goto ERR_RET;
            }
            pShmEntry->pBitmap[nItems-1].items[j-1].lenLen = atoi(buf);

            /*lenType*/
            snprintf(nodePath, sizeof(nodePath), "items/item[%d]/lenType", i+1);
            if (getXMLText(doc, nodePath, buf, sizeof(buf)) != 0) {
                LOG_ERR("getXMLText() error");
                goto ERR_RET;
            }
            pShmEntry->pBitmap[nItems-1].items[j-1].lenType = getLenType(buf);
            if (pShmEntry->pBitmap[nItems-1].items[j-1].lenType < 0) {
                LOG_ERR("[%s]: [lenType]��ֵ����ȷ", pathFile); 
                goto ERR_RET;
            }

            /*bcdFlag*/
            snprintf(nodePath, sizeof(nodePath), "items/item[%d]/bcdFlag", i+1);
            if (getXMLText(doc, nodePath, buf, sizeof(buf)) != 0) {
                LOG_ERR("getXMLText() error");
                goto ERR_RET;
            }
            pShmEntry->pBitmap[nItems-1].items[j-1].bcdFlag = atoi(buf);

            /*fillType*/
            snprintf(nodePath, sizeof(nodePath), "items/item[%d]/fillType", i+1);
            if (getXMLText(doc, nodePath, buf, sizeof(buf)) != 0) {
                LOG_ERR("getXMLText() error");
                goto ERR_RET;
            }
            pShmEntry->pBitmap[nItems-1].items[j-1].fillType = getFillType(buf);
            if (pShmEntry->pBitmap[nItems-1].items[j-1].fillType < 0) {
                LOG_ERR("[%s]: [fillType]��ֵ����ȷ", pathFile);
                goto ERR_RET;
            }
        }
        freeXML(doc);
    }
    pclose(pp);
    pShmEntry->pShmList->bitmap.nItems = nItems;
    return 0;

ERR_RET:
    freeXML(doc);
    pclose(pp);
    return -1;
}

/***************************************************************
 * mallocOneRoutStmt
 **************************************************************/
static ROUTSTMT *mallocOneRoutStmt(SHM_ENTRY *pShmEntry)
{
    pShmEntry->pShmList->routStmt.nItems += 1;
    if (!pShmEntry->pRoutStmt) {
        pShmEntry->pRoutStmt = (ROUTSTMT *)malloc(sizeof(ROUTSTMT));
        if (!pShmEntry->pRoutStmt) {
            LOG_ERR("malloc() error: %s", strerror(errno));
            return NULL;
        }
    } else {
        pShmEntry->pRoutStmt = (ROUTSTMT *)realloc(pShmEntry->pRoutStmt, (sizeof(ROUTSTMT) * pShmEntry->pShmList->routStmt.nItems));
        if (!pShmEntry->pRoutStmt) {
            LOG_ERR("realloc() error: %s", strerror(errno));
            return NULL;
        }
    }
    memset(&pShmEntry->pRoutStmt[pShmEntry->pShmList->routStmt.nItems-1], 0, sizeof(ROUTSTMT));
    return &pShmEntry->pRoutStmt[pShmEntry->pShmList->routStmt.nItems-1];
}

/***************************************************************
 * mallocOneConvStmt
 **************************************************************/
static CONVSTMT *mallocOneConvStmt(SHM_ENTRY *pShmEntry)
{
    pShmEntry->pShmList->convStmt.nItems += 1;
    if (!pShmEntry->pConvStmt) {
        pShmEntry->pConvStmt = (CONVSTMT *)malloc(sizeof(CONVSTMT));
        if (!pShmEntry->pConvStmt) {
            LOG_ERR("malloc() error: %s", strerror(errno));
            return NULL;
        }
    } else {
        pShmEntry->pConvStmt = (CONVSTMT *)realloc(pShmEntry->pConvStmt, (sizeof(CONVSTMT) * pShmEntry->pShmList->convStmt.nItems));
        if (!pShmEntry->pConvStmt) {
            LOG_ERR("realloc() error: %s", strerror(errno));
            return NULL;
        }
    }
    memset(&pShmEntry->pConvStmt[pShmEntry->pShmList->convStmt.nItems-1], 0, sizeof(CONVSTMT));
    return &pShmEntry->pConvStmt[pShmEntry->pShmList->convStmt.nItems-1];
}

/***************************************************************
 * preParseIF
 **************************************************************/
static int preParseIF(char *buf, char *ap[])
{
    int  i;
    char *p;
    
    for (i=0; i<3; i++) ap[i] = NULL;
    ap[0] = buf;
    if ((p = strstr(ap[0], "THEN")) != NULL) {
        *p = 0;
        if (*(strTrim(p+4))) {
            ap[1] = p+4;
            if (strstr(ap[1], "IF ") || strstr(ap[1], "ELSE ") || strstr(ap[1], "ELSEIF ")) return -1;
            if ((p = strstr(ap[1], " ENDIF")) != NULL) {
                *p = 0;
                ap[2] = p+1;
            } else {
                ap[2] = S_ENDIF;
            }
        }
    }
    return 0;
}

/***************************************************************
 * tpRoutStmtParse
 **************************************************************/
static int tpRoutStmtParse(int grpId, int line, char *buf, SHM_ENTRY *pShmEntry, JUMP_TAG jumpTags[])
{
    int  i;
    char *p,*p2,*ch,*ch2;
    char error[256];
    char tmpBuf[MAXEXPLEN+1];
    ROUTSTMT *pRoutStmt;
    STK_SWITCH *item;
    static STK_SWITCH *stack=NULL;

    p = buf;
    strTrim(p);

    /*����Ƿ������ǩ*/
    if (*p == '[' && (ch = strchr(p+1, ']'))) {
        *ch = 0;
        p += 1;
        strTrim(p); /*��ǩ��*/
        if (!(*p)) {
            LOG_ERR("·����[%d]��[%d]��: ȱ�ٱ�ǩ��", grpId, line);
            return -1;
        }
        /*�ݴ��ǩ����λ��*/
        for (i=0; i<NMAXTAGS; i++) {
            if (!jumpTags[i].name[0]) break;
            if (!strncmp(p, jumpTags[i].name, MAXKEYLEN)) {
                LOG_ERR("·����[%d]��[%d]��: ��ǩ���ظ�", grpId, line);
                return -1;
            }
        }
        if (i >= NMAXTAGS) {
            LOG_ERR("·����[%d]��[%d]��: ��ǩ������", grpId, line);
            return -1;
        }
        strncpy(jumpTags[i].name, p, MAXKEYLEN);
        jumpTags[i].index = pShmEntry->pShmList->routStmt.nItems;
        p = ch+1;
        strTrim(p);
        if (!(*p)) {
            if (!(pRoutStmt = mallocOneRoutStmt(pShmEntry))) return -1;
            pRoutStmt->action = KW_NONE;
            return 0;
        }
    }

    /*IF: {IF �������ʽ THEN [...] [ENDIF]}*/
    if (memcmp(p, "IF ", 3) == 0) {
        p += 3;
        strTrim(p);
        if (!(*p)) {
            LOG_ERR("·����[%d]��[%d]��: ȱ���������ʽ", grpId, line);
            return -1;
        }
        if (!(pRoutStmt = mallocOneRoutStmt(pShmEntry))) return -1;
        pRoutStmt->action = KW_IF;
        snprintf(tmpBuf, MAXEXPLEN+1, "NOT(%s)", p);
        preScanExpress(pRoutStmt->express, tmpBuf, error);
        if (error[0]) {
            LOG_ERR("·����[%d]��[%d]��: %s", grpId, line, error); 
            return -1;
        }
        return 0;
    }

    /*ELSEIF: {ELSEIF �������ʽ THEN [...] [ENDIF]}*/
    if (memcmp(p, "ELSEIF ", 7) == 0) {
        if (!(pRoutStmt = mallocOneRoutStmt(pShmEntry))) return -1;
        pRoutStmt->action = KW_ENDTHEN;
        p += 7;
        strTrim(p);
        if (!(*p)) {
            LOG_ERR("·����[%d]��[%d]��: ȱ���������ʽ", grpId, line);
            return -1;
        }
        if (!(pRoutStmt = mallocOneRoutStmt(pShmEntry))) return -1;
        pRoutStmt->action = KW_ELSEIF;
        snprintf(tmpBuf, MAXEXPLEN+1, "NOT(%s)", p);
        preScanExpress(pRoutStmt->express, tmpBuf, error);
        if (error[0]) {
            LOG_ERR("·����[%d]��[%d]��: %s", grpId, line, error);
            return -1;
        }
        return 0;
    }

    /*ELSE THEN [...] [ENDIF]*/
    if (memcmp(p, "ELSE", 4) == 0) {
        if (!(pRoutStmt = mallocOneRoutStmt(pShmEntry))) return -1;
        pRoutStmt->action = KW_ENDTHEN;
        if (!(pRoutStmt = mallocOneRoutStmt(pShmEntry))) return -1;
        pRoutStmt->action = KW_ELSE;
        return 0;
    }

    /*ENDIF*/
    if (memcmp(p, "ENDIF", 5) == 0) {
        if (!(pRoutStmt = mallocOneRoutStmt(pShmEntry))) return -1;
        pRoutStmt->action = KW_ENDIF;
        return 0;
    }
    
    /*WHILE: {WHILE �������ʽ}*/
    if (memcmp(p, "WHILE ", 6) == 0) {
        p += 6;
        strTrim(p);
        if (!(*p)) {
            LOG_ERR("·����[%d]��[%d]��: ȱ���������ʽ", grpId, line);
            return -1;
        }
        if (!(pRoutStmt = mallocOneRoutStmt(pShmEntry))) return -1;
        pRoutStmt->action = KW_WHILE;
        snprintf(tmpBuf, MAXEXPLEN+1, "NOT(%s)", p);
        preScanExpress(pRoutStmt->express, tmpBuf, error);
        if (error[0]) {
            LOG_ERR("·����[%d]��[%d]��: %s", grpId, line, error);
            return -1;
        }
        return 0;
    }

    /*CONTINUE*/
    if (memcmp(p, "CONTINUE", 8) == 0) {
        if (!(pRoutStmt = mallocOneRoutStmt(pShmEntry))) return -1;
        pRoutStmt->action = KW_CONTINUE;
        return 0;
    }

    /*EXIT*/
    if (memcmp(p, "EXIT", 4) == 0) {
        if (!(pRoutStmt = mallocOneRoutStmt(pShmEntry))) return -1;
        pRoutStmt->action = KW_EXIT;
        return 0;
    }

    /*ENDWHILE*/
    if (memcmp(p, "ENDWHILE", 8) == 0) {
        if (!(pRoutStmt = mallocOneRoutStmt(pShmEntry))) return -1;
        pRoutStmt->action = KW_ENDWHILE;
        return 0;        
    }

    /*SWITCH: {SWITCH �������ʽ}*/
    if (memcmp(p, "SWITCH ", 7) == 0) {
        p += 7;
        strTrim(p);
        if (!(*p)) {
            LOG_ERR("·����[%d]��[%d]��: ȱ���������ʽ", grpId, line);
            return -1;
        }
        if ((item = (STK_SWITCH *)malloc(sizeof(STK_SWITCH))) == NULL) {
            LOG_ERR("malloc() error: %s", strerror(errno));
            return -1;
        }
        strncpy(item->express, p, MAXEXPLEN);
        item->next = stack;
        stack = item;
        if (!(pRoutStmt = mallocOneRoutStmt(pShmEntry))) return -1;
        pRoutStmt->action = KW_SWITCH;
        return 0;
    }

    /*CASE: {CASE �������ʽ}*/
    if (memcmp(p, "CASE ", 5) == 0) {
        p += 5;
        strTrim(p);
        if (!(*p)) {
            LOG_ERR("·����[%d]��[%d]��: ȱ���������ʽ", grpId, line);
            return -1;
        }
        if (!(pRoutStmt = mallocOneRoutStmt(pShmEntry))) return -1;
        pRoutStmt->action = KW_CASE;
        if (!stack) {
            LOG_ERR("·����[%d]��[%d]��: �﷨����[>>>CASE]", grpId, line);
            return -1;
        }
        snprintf(tmpBuf, MAXEXPLEN+1, "NOT(SEQ(%s,%s))", p, stack->express);
        preScanExpress(pRoutStmt->express, tmpBuf, error);
        if (error[0]) {
            LOG_ERR("·����[%d]��[%d]��: %s", grpId, line, error);
            return -1;
        }
        return 0;
    }

    /*BREAK*/
    if (memcmp(p, "BREAK", 5) == 0) {
        if (!(pRoutStmt = mallocOneRoutStmt(pShmEntry))) return -1;
        pRoutStmt->action = KW_BREAK;
        return 0;
    }

    /*DEFAULT*/
    if (memcmp(p, "DEFAULT", 7) == 0) {
        if (!(pRoutStmt = mallocOneRoutStmt(pShmEntry))) return -1;
        pRoutStmt->action = KW_DEFAULT;
        return 0;
    }

    /*ENDSWITCH*/
    if (memcmp(p, "ENDSWITCH", 9) == 0) {
        item = stack;
        stack = stack->next;
        free(item);
        if (!(pRoutStmt = mallocOneRoutStmt(pShmEntry))) return -1;
        pRoutStmt->action = KW_ENDSWITCH;
        return 0;
    }
    
    /*JUMP: {JUMP ��ǩ��}*/
    if (memcmp(p, "JUMP ", 5) == 0) {
        p += 5;
        strTrim(p);
        if (!(*p)) {
            LOG_ERR("·����[%d]��[%d]��: ȱ�ٱ�ǩ��", grpId, line);
            return -1;
        }
        if (!(pRoutStmt = mallocOneRoutStmt(pShmEntry))) return -1;
        pRoutStmt->action = KW_JUMP;
        strncpy(pRoutStmt->express, p, MAXKEYLEN);
        return 0;
    }

    /*VAR: {VAR ��ʱ������ = ��ʱ����ֵ}*/
    if (memcmp(p, "VAR ", 4) == 0) {
        p += 4;
        if (!(ch = strchr(p, '='))) {
            LOG_ERR("·����[%d]��[%d]��: ȱ�ٹؼ���=", grpId, line);
            return -1;
        }
        *ch = 0;
        strTrim(p); /*��ʱ������*/
        if (!(*p)) {
            LOG_ERR("·����[%d]��[%d]��: ȱ����ʱ������", grpId, line);
            return -1;
        }
        if (!(pRoutStmt = mallocOneRoutStmt(pShmEntry))) return -1;
        pRoutStmt->action = KW_VAR;

        preScanExpress(pRoutStmt->fldName, p, error);
        if (error[0]) {
            LOG_ERR("·����[%d]��[%d]��: %s", grpId, line, error);
            return -1;
        }
        p = ch+1;
        strTrim(p); /*��ֵ���ʽ*/
        if (!(*p)) {
            LOG_ERR("·����[%d]��[%d]��: ȱ�ٸ�ֵ���ʽ", grpId, line);
            return -1;
        }

        /*VAR v = ALLOC(z) ==> EXEC ALLOC(v,z)*/
        if (memcmp(p, "ALLOC", 5) != 0) {
            preScanExpress(pRoutStmt->express, p, error);
            if (error[0]) {
                LOG_ERR("·����[%d]��[%d]��: %s", grpId, line, error);
                return -1;
            }
        } else {
            p += 5;
            strTrim(p);
            if (p[0] != '(' || p[strlen(p)-1] != ')') {
                LOG_ERR("·����[%d]��[%d]��: �﷨����", grpId, line, error);
                return -1;
            }
            p[strlen(p)-1] = 0;
            preScanExpress(pRoutStmt->express, p+1, error);
            if (error[0]) {
                LOG_ERR("·����[%d]��[%d]��: %s", grpId, line, error);
                return -1;
            }
            pRoutStmt->action = KW_ALLOC;
        }
        return 0;
    }

    /*SET: {SET �������ʽ = ��ֵ���ʽ}*/
    if (memcmp(p, "SET ", 4) == 0) {
        p += 4;
        if (!(ch = strchr(p, '='))) {
            LOG_ERR("·����[%d]��[%d]��: ȱ�ٹؼ���=", grpId, line);
            return -1;
        }
        *ch = 0;
        strTrim(p); /*�������ʽ*/
        if (!(*p)) {
            LOG_ERR("·����[%d]��[%d]��: ȱ���������ʽ", grpId, line);
            return -1;
        }
        if (!(pRoutStmt = mallocOneRoutStmt(pShmEntry))) return -1;
        pRoutStmt->action = KW_SET;

        if (!(ch2 = strchr(p, '['))) {
            preScanExpress(pRoutStmt->fldName, p, error);
            if (error[0]) {
                LOG_ERR("·����[%d]��[%d]��: %s", grpId, line, error);
                return -1;
            }
        } else {
            p2 = ch2+1;
            memcpy(pRoutStmt->fldName, p, p2-p);
            pRoutStmt->fldName[p2-p] = 0;
            if (!(ch2 = strchr(p2, ']'))) {
                LOG_ERR("·����[%d]��[%d]��: �������ʽ����ȷ", grpId, line);
                return -1;
            }
            memcpy(tmpBuf, p2, ch2-p2);
            tmpBuf[ch2-p2] = 0;
            preScanExpress(pRoutStmt->fldName+strlen(pRoutStmt->fldName), tmpBuf, error);
            if (error[0]) {
                LOG_ERR("·����[%d]��[%d]��: %s", grpId, line, error);
                return -1;
            }
            strcat(pRoutStmt->fldName, "]");
        }
        p = ch+1;
        strTrim(p); /*��ֵ���ʽ*/
        if (!(*p)) {
            LOG_ERR("·����[%d]��[%d]��: ȱ����ֵ���ʽ", grpId, line);
            return -1;
        }
        preScanExpress(pRoutStmt->express, p, error);
        if (error[0]) {
            LOG_ERR("·����[%d]��[%d]��: %s", grpId, line, error);
            return -1;
        }
        return 0;
    }

    /*CALL: {CALL Ŀ��˵� {�������|NONE} [REVERSE [FORCE] [NORESP]]}*/
    if (memcmp(p, "CALL ", 5) == 0) {
        p += 5;
        if (!(ch = strchr(p, '{'))) {
            LOG_ERR("·����[%d]��[%d]��: �﷨����", grpId, line);
            return -1;
        }
        *ch = 0;
        strTrim(p); /*Ŀ��˵�*/
        if (!(*p)) {
            LOG_ERR("·����[%d]��[%d]��: ȱ��Ŀ��˵�", grpId, line);
            return -1;
        }
        preScanExpress(tmpBuf, p, error); /*����֧�ֺ��滻*/
        if (error[0]) {
            LOG_ERR("·����[%d]��[%d]��: %s", grpId, line, error);
            return -1;
        }
        if (!(pRoutStmt = mallocOneRoutStmt(pShmEntry))) return -1;
        pRoutStmt->action = KW_CALL;
        pRoutStmt->desQ = atoi(tmpBuf);
        if (pRoutStmt->desQ <= 0 || pRoutStmt->desQ > NMAXBINDQ) {
            LOG_ERR("·����[%d]��[%d]��: Ŀ��˵㲻��ȷ", grpId, line);
            return -1;
        }
        p = ch+1;
        if (!(ch = strchr(p, '}'))) {
            LOG_ERR("·����[%d]��[%d]��: �﷨����", grpId, line);
            return -1;
        }
        *ch = 0;
        strTrim(p); /*�������*/
        if (!(*p)) {
            LOG_ERR("·����[%d]��[%d]��: ȱ�ٷ������", grpId, line);
            return -1;
        }
        strncpy(pRoutStmt->procCode, p, MAXKEYLEN);
        p = ch+1;
        if (!(ch = strstr(p, "REVERSE"))) return 0; /*��������*/
        pRoutStmt->revFlag = 1;
        p = ch+7;
        if (strstr(p, "FORCE") != NULL) {
            pRoutStmt->forceRev = 1; /*��ʱǿ�Ƴ���*/
        }
        if (strstr(p, "NORESP") != NULL) {
            pRoutStmt->noRespRev = 1; /*�޳�����Ӧ*/
        }
        return 0;
    }

    /*SEND: {SEND Ŀ��˵� USING ���ת����}*/
    /*SEND: {SEND �����˵� USING {�����ʶ��|NONE}}*/
    if (memcmp(p, "SEND ", 5) == 0) {
        p += 5;
        if (!(ch = strstr(p, "USING "))) {
            LOG_ERR("·����[%d]��[%d]��: ȱ�ٹؼ���USING", grpId, line);
            return -1;
        }
        *ch = 0;
        strTrim(p); /*Ŀ��˵�*/
        if (!(*p)) {
            LOG_ERR("·����[%d]��[%d]��: ȱ��Ŀ��˵�", grpId, line);
            return -1;
        }
        preScanExpress(tmpBuf, p, error); /*����֧�ֺ��滻*/
        if (error[0]) {
            LOG_ERR("·����[%d]��[%d]��: %s", grpId, line, error);
            return -1;
        } 
        if (!(pRoutStmt = mallocOneRoutStmt(pShmEntry))) return -1;
        pRoutStmt->action = KW_SEND;
        pRoutStmt->desQ = atoi(tmpBuf);
        if (pRoutStmt->desQ <= 0 || pRoutStmt->desQ > NMAXBINDQ) {
            LOG_ERR("·����[%d]��[%d]��: Ŀ��˵㲻��ȷ", grpId, line);
            return -1;
        }
        p = ch+6;
        strTrim(p); /*���ת����*/
        if (!(*p)) {
            LOG_ERR("·����[%d]��[%d]��: ȱ�ٴ��ת��������ʶ��", grpId, line);
            return -1;
        }
        if (p[0] == '{' && p[strlen(p)-1] == '}') {
            strncpy(tmpBuf, p+1, sizeof(tmpBuf)-1);
            tmpBuf[strlen(tmpBuf)-1] = 0;
            strTrim(tmpBuf);
            if (!tmpBuf[0]) {
                LOG_ERR("·����[%d]��[%d]��: ȱ�ٴ����ʶ��", grpId, line);
                return -1;
            }
            strncpy(pRoutStmt->procCode, tmpBuf, MAXKEYLEN);
        } else {
            if (atoi(p)<=0) {
                LOG_ERR("·����[%d]��[%d]��: ���ת���鲻��ȷ", grpId, line);
                return -1;
            }
            strncpy(pRoutStmt->procCode, p, MAXKEYLEN);
        }
        return 0;
    }

    /*EXEC: {EXEC ���ʽ}*/ 
    if (memcmp(p, "EXEC ", 5) == 0) {
        p += 5;
        strTrim(p);
        if (!(*p)) {
            LOG_ERR("·����[%d]��[%d]��: ȱ�ٱ��ʽ", grpId, line);
            return -1;
        }
        if (!(pRoutStmt = mallocOneRoutStmt(pShmEntry))) return -1;
        pRoutStmt->action = KW_EXEC;
        preScanExpress(pRoutStmt->express, p, error);
        if (error[0]) {
            LOG_ERR("·����[%d]��[%d]��: %s", grpId, line, error);
            return -1;
        }
        return 0;
    }

    /*REVERSE: {REVERSE}*/ 
    if (memcmp(p, "REVERSE", 7) == 0) {
    if (*(p+7)) {
            LOG_ERR("·����[%d]��[%d]��: �﷨����", grpId, line);
            return -1;
        }
        if (!(pRoutStmt = mallocOneRoutStmt(pShmEntry))) return -1;
        pRoutStmt->action = KW_REVERSE;
        return 0;
    }

    /*RETURN: {RETURN [������]}*/ 
    if (memcmp(p, "RETURN", 6) == 0) {
        p += 6;
        if (*p && *p != ' ') {
            LOG_ERR("·����[%d]��[%d]��: �﷨����", grpId, line);
            return -1;
        }
        if (!(pRoutStmt = mallocOneRoutStmt(pShmEntry))) return -1;
        pRoutStmt->action = KW_RETURN;
        strTrim(p);
        strncpy(pRoutStmt->express, p, MAXEXPLEN);
        return 0;
    }
    LOG_ERR("·����[%d]��[%d]��: �﷨����", grpId, line);
    return -1;
}

/***************************************************************
 * tpRoutGrpLoad
 **************************************************************/
static int tpRoutGrpLoad(SHM_ENTRY *pShmEntry, ROUTGRP *pRoutGrp, char *script)
{
    int i,j,k,grpId,line;
    char *p,*ch,*ap[3];
    ROUTSTMT *pRoutStmt;
    JUMP_TAG jumpTags[NMAXTAGS];
    STK_ITEM *item,*stack=NULL;
    STK_ITEM *item2,*stack2=NULL;

    grpId = pRoutGrp->grpId;
    pRoutGrp->nStart = pShmEntry->pShmList->routStmt.nItems;
    memset(jumpTags, 0, (NMAXTAGS * sizeof(JUMP_TAG)));

    for (i=0, line=0, p=script; script[i]!='\0'; i++) {
        if (script[i] == '\n') {
            script[i] = 0;
            if ((ch = strchr(p, '#'))) *ch = 0; /*����ע��*/
            strTrim(p);
            if (!p[0]) {
                p = &(script[i+1]);
                continue;
            }
            line++;
            if (preParseIF(p, ap) != 0) {
                LOG_ERR("·����[%d]��[%d]��: �﷨����", grpId, line);        
                return -1;
            }
            for (j=0; j<3 && ap[j]!= NULL; j++) {
                if (tpRoutStmtParse(grpId, line, ap[j], pShmEntry, jumpTags) != 0) { 
                    LOG_ERR("tpRoutStmtParse() error");
                    return -1;
                }
            }
            p = &script[i+1];
        }
    }
    if (*p) { /*���һ�нű���ѭ���п���δ������*/
        if ((ch = strchr(p, '#'))) *ch = 0;
        strTrim(p);
        if (*p) {
            line++;
            if (preParseIF(script, ap) != 0) {
                LOG_ERR("·����[%d]��[%d]��: �﷨����", grpId, line);        
                return -1;
            }
            for (j=0; j<3 && ap[j]!= NULL; j++) {
                if (tpRoutStmtParse(grpId, line, ap[j], pShmEntry, jumpTags) != 0) { 
                    LOG_ERR("tpRoutStmtParse() error");
                    return -1;
                }
            }
        }
    }
    pRoutGrp->nStmts = pShmEntry->pShmList->routStmt.nItems - pRoutGrp->nStart;

    for (i=pRoutGrp->nStart; i<pShmEntry->pShmList->routStmt.nItems; i++) {
        pRoutStmt = &pShmEntry->pRoutStmt[i];
        switch (pRoutStmt->action) {
            case KW_IF:
            case KW_ELSEIF:
            case KW_ELSE:
            case KW_ENDTHEN:
            case KW_WHILE:
            case KW_CONTINUE:
            case KW_EXIT:
            case KW_SWITCH:
            case KW_CASE:
            case KW_BREAK:
            case KW_DEFAULT:
                /*ѹջ*/
                if ((item = (STK_ITEM *)malloc(sizeof(STK_ITEM))) == NULL) {
                    LOG_ERR("malloc() error: %s", strerror(errno));
                    goto ERR_RET;
                }
                item->index = i;
                item->action = pRoutStmt->action;
                item->next = stack;
                stack = item;
                break;
            case KW_JUMP:
                for (j=0; j<NMAXTAGS; j++) {
                    if (!jumpTags[j].name[0] || !strcmp(jumpTags[j].name, pRoutStmt->express)) break;
                }
                if (j>=NMAXTAGS || !jumpTags[j].name[0]) {
                    LOG_ERR("·����[%d]��δ�����ǩ[%s]", grpId, pRoutStmt->express);
                    goto ERR_RET;
                }
                pRoutStmt->action = DO_JUMP;
                pRoutStmt->jump = jumpTags[j].index;
                break;        
            case KW_ENDIF:
                pRoutStmt->action = DO_NONE;
                j = i;
                /*������ƥ���IF��ELSEIF��ELSE*/
                while (stack) {
                    if (stack->action == (char)KW_ELSE) {
                        pRoutStmt = &pShmEntry->pRoutStmt[stack->index];
                        pRoutStmt->action = DO_NONE;
                        j = stack->index;
                        item = stack;
                        stack = stack->next;
                        free(item);
                    } else if (stack->action == (char)KW_ELSEIF) {
                        pRoutStmt = &pShmEntry->pRoutStmt[stack->index];
                        pRoutStmt->action = DO_JUMP;
                        pRoutStmt->jump = j; /*��ת����һ��ELSEIF��ELSE��ENDIF*/
                        j = stack->index;
                        item = stack;
                        stack = stack->next;
                        free(item);
                    } else if (stack->action == (char)KW_ENDTHEN) {
                        pRoutStmt = &pShmEntry->pRoutStmt[stack->index];
                        pRoutStmt->action = DO_JUMP;
                        pRoutStmt->jump = i; /*��ת��ENDIF*/
                        item = stack;
                        stack = stack->next;
                        free(item);
                    } else if (stack->action == (char)KW_IF) {
                        pRoutStmt = &pShmEntry->pRoutStmt[stack->index];
                        pRoutStmt->action = DO_JUMP;
                        pRoutStmt->jump = j; /*��ת��ELSEIF��ELSE��ENDIF*/
                        item = stack;
                        stack = stack->next;
                        free(item);
                        /*stack2��ջ->stackѹջ(���½�ջ)*/
                        item = stack2;
                        while (item) {
                            item2 = item->next;
                            item->next = stack;
                            stack = item;
                            item = item2;
                        }
                        stack2 = NULL;
                        break;
                    } else if (stack->action == (char)KW_EXIT ||
                               stack->action == (char)KW_CONTINUE) {
                        /*stack��ջ->stack2ѹջ(��ʱ��ջ)*/
                        item2 = stack;
                        stack = stack->next;
                        item2->next = stack2;
                        stack2 = item2;
                    } else {
                        LOG_ERR("·����[%d]: �﷨����[>>>ENDIF]", grpId);
                        goto ERR_RET;
                    }
                } 
                break;
            case KW_ENDWHILE:
                /*������ƥ���WHILE*/
                j = -1;
                item = stack;
                while (item) {
                    if (item->action == (char)KW_WHILE) {
                        j = item->index;
                        break;
                    }
                    item = item->next;
                }
                if (j<0) {
                    LOG_ERR("·����[%d]: �﷨����[>>>ENDWHILE]", grpId);
                    goto ERR_RET;
                }
                pRoutStmt->action = DO_JUMP;
                pRoutStmt->jump = j;
                /*������ƥ���CONTINUE��EXIT��WHILE*/
                while (stack) {
                    if (stack->action == (char)KW_CONTINUE) {
                        pRoutStmt = &pShmEntry->pRoutStmt[stack->index];
                        pRoutStmt->action = DO_JUMP;
                        pRoutStmt->jump = j; /*��ת��WHILE*/
                        item = stack;
                        stack = stack->next;
                        free(item);
                    } else if (stack->action == (char)KW_EXIT) {
                        pRoutStmt = &pShmEntry->pRoutStmt[stack->index];
                        pRoutStmt->action = DO_JUMP;
                        pRoutStmt->jump = i+1; /*��ת��ENDWHILE��һ��*/
                        item = stack;
                        stack = stack->next;
                        free(item);
                    } else if (stack->action == (char)KW_WHILE) {
                        pRoutStmt = &pShmEntry->pRoutStmt[stack->index];
                        pRoutStmt->action = DO_JUMP;
                        pRoutStmt->jump = i+1; /*��ת��ENDWHILE��һ��*/
                        item = stack;
                        stack = stack->next;
                        free(item);
                        break;
                    } else {
                        LOG_ERR("·����[%d]: �﷨����[>>>ENDWHILE]", grpId);
                        goto ERR_RET;
                    }
                } 
                break;
            case KW_ENDSWITCH:
                pRoutStmt->action = DO_NONE;
                j = i;
                /*������ƥ���SWITCH��CASE��BREAK��DEFAULT*/
                while (stack) {
                    if (stack->action == (char)KW_BREAK) {
                        pRoutStmt = &pShmEntry->pRoutStmt[stack->index];
                        pRoutStmt->action = DO_JUMP;
                        pRoutStmt->jump = j; /*��ת��ENDSWITCH*/
                        item = stack;
                        stack = stack->next;
                        free(item);
                    } else if (stack->action == (char)KW_DEFAULT) {
                        pRoutStmt = &pShmEntry->pRoutStmt[stack->index];
                        pRoutStmt->action = DO_NONE;
                        k = stack->index;
                        item = stack;
                        stack = stack->next;
                        free(item);
                    } else if (stack->action == (char)KW_CASE) {
                        pRoutStmt = &pShmEntry->pRoutStmt[stack->index];
                        pRoutStmt->action = DO_JUMP;
                        /*pRoutStmt->jump = k; ��ת��DEFAULT���һCASE*/
                        if (k < 0) {
                            pRoutStmt->jump = i;
                        } else {
                            pRoutStmt->jump = k;
                        }
                        k = stack->index;
                        item = stack;
                        stack = stack->next;
                        free(item);
                    } else if (stack->action == (char)KW_SWITCH) {
                        pRoutStmt = &pShmEntry->pRoutStmt[stack->index];
                        pRoutStmt->action = DO_NONE;
                        item = stack;
                        stack = stack->next;
                        free(item);
                        /*stack2��ջ->stackѹջ(���½�ջ)*/
                        item = stack2;
                        while (item) {
                            item2 = item->next;
                            item->next = stack;
                            stack = item;
                            item = item2;
                        }
                        stack2 = NULL;
                        break;
                    } else if (stack->action == (char)KW_EXIT ||
                               stack->action == (char)KW_CONTINUE) {
                        /*stack��ջ->stack2ѹջ(��ʱ��ջ)*/
                        item2 = stack;
                        stack = stack->next;
                        item2->next = stack2;
                        stack2 = item2;
                    } else {
                        LOG_ERR("·����[%d]: �﷨����[>>>ENDSWITCH]", grpId);
                        goto ERR_RET;
                    }
                } 
                break;
            default:
                break;
        }

    }
    if (stack) {
        LOG_ERR("·����[%d]: �﷨����", grpId);
        goto ERR_RET;
    }
    return 0;

ERR_RET:
    while (stack) {
        item = stack;
        stack = stack->next;
        free(item);
    }
    return -1;
}

/***************************************************************
 * tpRoutLoad
 **************************************************************/
int tpRoutLoad(SHM_ENTRY *pShmEntry)
{
    FILE *pp;
    int  i,j,grpId;
    char pathFile[256],buf[1024],script[2048*32];
    XMLDOC *doc;

    snprintf(pathFile, sizeof(pathFile), "ls %s/etc/TPROUT/*.xml", getenv("TPBUS_HOME"));
    if ((pp = popen(pathFile, "r")) == NULL) {
        LOG_ERR("popen() error: %s", strerror(errno));
        return -1;
    }

    while (fgets(pathFile, sizeof(pathFile), pp) != NULL) {
        if ((doc = loadXMLFile(strTrim(pathFile))) == NULL) {
            LOG_ERR("loadXMLFile() error");
            pclose(pp);
            return -1;
        }

        /*grpId*/
        if (getXMLText(doc, "grpId", buf, sizeof(buf)) != 0) {
            LOG_ERR("getXMLText() error");
            goto ERR_RET;
        }
        grpId = atoi(buf);
        i = (grpId % HASH_SIZE)+1;

        if (!pShmEntry->pRoutGrp) {
            pShmEntry->pShmList->routGrp.nItems = HASH_SIZE;
            pShmEntry->pRoutGrp = (ROUTGRP *)malloc(sizeof(ROUTGRP) * pShmEntry->pShmList->routGrp.nItems);
            if (!pShmEntry->pRoutGrp) {
                LOG_ERR("malloc() error: %s", strerror(errno));
                goto ERR_RET;
            }
            memset(pShmEntry->pRoutGrp, 0, (sizeof(ROUTGRP) * pShmEntry->pShmList->routGrp.nItems)); 
        } else {
            if (pShmEntry->pRoutGrp[i-1].grpId > 0) {
                pShmEntry->pShmList->routGrp.nItems += 1;
                pShmEntry->pRoutGrp = (ROUTGRP *)realloc(pShmEntry->pRoutGrp, (sizeof(ROUTGRP) * pShmEntry->pShmList->routGrp.nItems));
                if (!pShmEntry->pRoutGrp) {
                    LOG_ERR("realloc() error: %s", strerror(errno));
                    goto ERR_RET;
                }
                i = pShmEntry->pShmList->routGrp.nItems;
                memset(&pShmEntry->pRoutGrp[i-1], 0, sizeof(ROUTGRP));
                j = (grpId % HASH_SIZE)+1;
                while (j>0) {
                    if (grpId == pShmEntry->pRoutGrp[j-1].grpId) {
                        LOG_ERR("·����[%d]�ظ�����", grpId);
                        goto ERR_RET;
                    }
                    if (!pShmEntry->pRoutGrp[j-1].next) break;
                    j = pShmEntry->pRoutGrp[j-1].next;
                }
                pShmEntry->pRoutGrp[j-1].next = i;
            }
        }
        pShmEntry->pRoutGrp[i-1].grpId = grpId;

        /*script*/
        if (getXMLText(doc, "script", script, sizeof(script)) != 0) {
            LOG_ERR("getXMLText() error");
            goto ERR_RET;
        }

        if (tpRoutGrpLoad(pShmEntry, &pShmEntry->pRoutGrp[i-1], script) != 0) {
            LOG_ERR("tpRoutGrpLoad() error: grpId=[%d]", grpId);
            goto ERR_RET;
        }
        freeXML(doc);
    }
    pclose(pp);
    return 0;

ERR_RET:
    freeXML(doc);
    pclose(pp);
    return -1;
}

/***************************************************************
 * tpConvStmtParse
 **************************************************************/
static int tpConvStmtParse(int grpId, int line, char *buf, SHM_ENTRY *pShmEntry, JUMP_TAG jumpTags[])
{
    int  i;
    char *p,*p2,*ch,*ch2;
    char error[256];
    char tmpBuf[MAXEXPLEN+1];
    CONVSTMT *pConvStmt;
    STK_SWITCH *item;
    static STK_SWITCH *stack=NULL;

    p = buf;
    strTrim(p);

    /*����Ƿ������ǩ*/
    if (*p == '[' && (ch = strchr(p+1, ']'))) {
        *ch = 0;
        p += 1;
        strTrim(p); /*��ǩ��*/
        if (!(*p)) {
            LOG_ERR("ת����[%d]��[%d]��: ȱ�ٱ�ǩ��", grpId, line);
            return -1;
        }
        /*�ݴ��ǩ����λ��*/
        for (i=0; i<NMAXTAGS; i++) {
            if (!jumpTags[i].name[0]) break;
            if (!strncmp(p, jumpTags[i].name, MAXKEYLEN)) {
                LOG_ERR("ת����[%d]��[%d]��: ��ǩ���ظ�", grpId, line);
                return -1;
            }
        }
        if (i >= NMAXTAGS) {
            LOG_ERR("ת����[%d]��[%d]��: ��ǩ������", grpId, line);
            return -1;
        }
        strncpy(jumpTags[i].name, p, MAXKEYLEN);
        jumpTags[i].index = pShmEntry->pShmList->convStmt.nItems;
        p = ch+1;
        strTrim(p);
        if (!(*p)) {
            if (!(pConvStmt = mallocOneConvStmt(pShmEntry))) return -1;
            pConvStmt->action = KW_NONE;
            return 0;
        }
    }

    /*IF: {IF �������ʽ THEN [...] [ENDIF]}*/
    if (memcmp(p, "IF ", 3) == 0) {
        p += 3;
        strTrim(p);
        if (!(*p)) {
            LOG_ERR("ת����[%d]��[%d]��: ȱ���������ʽ", grpId, line);
            return -1;
        }
        if (!(pConvStmt = mallocOneConvStmt(pShmEntry))) return -1;
        pConvStmt->action = KW_IF;
        snprintf(tmpBuf, MAXEXPLEN+1, "NOT(%s)", p);
        preScanExpress(pConvStmt->express, tmpBuf, error);
        if (error[0]) {
            LOG_ERR("ת����[%d]��[%d]��: %s", grpId, line, error); 
            return -1;
        }
        return 0;
    }

    /*ELSEIF: {ELSEIF �������ʽ THEN [...] [ENDIF]}*/
    if (memcmp(p, "ELSEIF ", 7) == 0) {
        if (!(pConvStmt = mallocOneConvStmt(pShmEntry))) return -1;
        pConvStmt->action = KW_ENDTHEN;
        p += 7;
        strTrim(p);
        if (!(*p)) {
            LOG_ERR("ת����[%d]��[%d]��: ȱ���������ʽ", grpId, line);
            return -1;
        }
        if (!(pConvStmt = mallocOneConvStmt(pShmEntry))) return -1;
        pConvStmt->action = KW_ELSEIF;
        snprintf(tmpBuf, MAXEXPLEN+1, "NOT(%s)", p);
        preScanExpress(pConvStmt->express, tmpBuf, error);
        if (error[0]) {
            LOG_ERR("ת����[%d]��[%d]��: %s", grpId, line, error);
            return -1;
        }
        return 0;
    }

    /*ELSE THEN [...] [ENDIF]*/
    if (memcmp(p, "ELSE", 4) == 0) {
        if (!(pConvStmt = mallocOneConvStmt(pShmEntry))) return -1;
        pConvStmt->action = KW_ENDTHEN;
        if (!(pConvStmt = mallocOneConvStmt(pShmEntry))) return -1;
        pConvStmt->action = KW_ELSE;
        return 0;
    }

    /*ENDIF*/
    if (memcmp(p, "ENDIF", 5) == 0) {
        if (!(pConvStmt = mallocOneConvStmt(pShmEntry))) return -1;
        pConvStmt->action = KW_ENDIF;
        return 0;
    }
    
    /*WHILE: {WHILE �������ʽ}*/
    if (memcmp(p, "WHILE ", 6) == 0) {
        p += 6;
        strTrim(p);
        if (!(*p)) {
            LOG_ERR("ת����[%d]��[%d]��: ȱ���������ʽ", grpId, line);
            return -1;
        }
        if (!(pConvStmt = mallocOneConvStmt(pShmEntry))) return -1;
        pConvStmt->action = KW_WHILE;
        snprintf(tmpBuf, MAXEXPLEN+1, "NOT(%s)", p);
        preScanExpress(pConvStmt->express, tmpBuf, error);
        if (error[0]) {
            LOG_ERR("ת����[%d]��[%d]��: %s", grpId, line, error);
            return -1;
        }
        return 0;
    }

    /*CONTINUE*/
    if (memcmp(p, "CONTINUE", 8) == 0) {
        if (!(pConvStmt = mallocOneConvStmt(pShmEntry))) return -1;
        pConvStmt->action = KW_CONTINUE;
        return 0;
    }

    /*EXIT*/
    if (memcmp(p, "EXIT", 4) == 0) {
        if (!(pConvStmt = mallocOneConvStmt(pShmEntry))) return -1;
        pConvStmt->action = KW_EXIT;
        return 0;
    }

    /*ENDWHILE*/
    if (memcmp(p, "ENDWHILE", 8) == 0) {
        if (!(pConvStmt = mallocOneConvStmt(pShmEntry))) return -1;
        pConvStmt->action = KW_ENDWHILE;
        return 0;        
    }

    /*SWITCH: {SWITCH �������ʽ}*/
    if (memcmp(p, "SWITCH ", 7) == 0) {
        p += 7;
        strTrim(p);
        if (!(*p)) {
            LOG_ERR("ת����[%d]��[%d]��: ȱ���������ʽ", grpId, line);
            return -1; 
        }  
        if ((item = (STK_SWITCH *)malloc(sizeof(STK_SWITCH))) == NULL) {
            LOG_ERR("malloc() error: %s", strerror(errno));
            return -1;
        }
        strncpy(item->express, p, MAXEXPLEN);
        item->next = stack;
        stack = item;
        if (!(pConvStmt = mallocOneConvStmt(pShmEntry))) return -1;
        pConvStmt->action = KW_SWITCH;
        return 0;
    }

    /*CASE: {CASE �������ʽ}*/
    if (memcmp(p, "CASE ", 5) == 0) {
        p += 5;
        strTrim(p);
        if (!(*p)) {
            LOG_ERR("ת����[%d]��[%d]��: ȱ���������ʽ", grpId, line);
            return -1; 
        }  
        if (!(pConvStmt = mallocOneConvStmt(pShmEntry))) return -1;
        pConvStmt->action = KW_CASE;
        if (!stack) {
            LOG_ERR("·����[%d]��[%d]��: �﷨����[>>>CASE]", grpId, line);
            return -1;
        }
        snprintf(tmpBuf, MAXEXPLEN+1, "NOT(SEQ(%s,%s))", p, stack->express);
        preScanExpress(pConvStmt->express, tmpBuf, error);
        if (error[0]) {
            LOG_ERR("ת����[%d]��[%d]��: %s", grpId, line, error);
            return -1;
        }
        return 0;
    }

    /*BREAK*/
    if (memcmp(p, "BREAK", 5) == 0) {
        if (!(pConvStmt = mallocOneConvStmt(pShmEntry))) return -1;
        pConvStmt->action = KW_BREAK;
        return 0;
    }

    /*DEFAULT*/
    if (memcmp(p, "DEFAULT", 7) == 0) {
        if (!(pConvStmt = mallocOneConvStmt(pShmEntry))) return -1;
        pConvStmt->action = KW_DEFAULT;
        return 0;
    }

    /*ENDSWITCH*/
    if (memcmp(p, "ENDSWITCH", 9) == 0) {
        if (!(pConvStmt = mallocOneConvStmt(pShmEntry))) return -1;
        pConvStmt->action = KW_ENDSWITCH;
        return 0;        
    }
    
    /*JUMP: {JUMP ��ǩ��}*/
    if (memcmp(p, "JUMP ", 5) == 0) {
        p += 5;
        strTrim(p);
        if (!(*(p))) {
            LOG_ERR("ת����[%d]��[%d]��: ȱ�ٱ�ǩ��", grpId, line);
            return -1;
        }
        if (!(pConvStmt = mallocOneConvStmt(pShmEntry))) return -1;
        pConvStmt->action = KW_JUMP;
        strncpy(pConvStmt->express, p, MAXKEYLEN);
        return 0;
    }

    /*VAR: {VAR ��ʱ������ = ��ʱ����ֵ}*/
    if (memcmp(p, "VAR ", 4) == 0) {
        p += 4;
        if (!(ch = strchr(p, '='))) {
            LOG_ERR("ת����[%d]��[%d]��: ȱ�ٹؼ���=", grpId, line);
            return -1;
        }
        *ch = 0;
        strTrim(p); /*��ʱ������*/
        if (!(*p)) {
            LOG_ERR("ת����[%d]��[%d]��: ȱ����ʱ������", grpId, line);
            return -1;
        }
        if (!(pConvStmt = mallocOneConvStmt(pShmEntry))) return -1;
        pConvStmt->action = KW_VAR;

        preScanExpress(pConvStmt->fldName, p, error);
        if (error[0]) {
            LOG_ERR("ת����[%d]��[%d]��: %s", grpId, line, error);
            return -1;
        }
        p = ch+1;
        strTrim(p); /*��ֵ���ʽ*/
        if (!(*p)) {
            LOG_ERR("ת����[%d]��[%d]��: ȱ�ٸ�ֵ���ʽ", grpId, line);
            return -1;
        }
        /*VAR v = ALLOC(z) ==> EXEC ALLOC(v,z)*/
        if (memcmp(p, "ALLOC", 5) != 0) {
            preScanExpress(pConvStmt->express, p, error);
            if (error[0]) {
                LOG_ERR("ת����[%d]��[%d]��: %s", grpId, line, error);
                return -1;
            }
        } else {
            p += 5;
            strTrim(p);
            if (p[0] != '(' || p[strlen(p)-1] != ')') {
                LOG_ERR("ת����[%d]��[%d]��: �﷨����", grpId, line, error);
                return -1;
            }
            p[strlen(p)-1] = 0;
            preScanExpress(pConvStmt->express, p+1, error);
            if (error[0]) {
                LOG_ERR("ת����[%d]��[%d]��: %s", grpId, line, error);
                return -1;
            }
            pConvStmt->action = KW_ALLOC;
        }
        return 0;
    }

    /*SET: {SET [�������ʽ] = ��ֵ���ʽ}*/
    if (memcmp(p, "SET ", 4) == 0) {
        p += 4;
        if (!(ch = strchr(p, '='))) {
            LOG_ERR("ת����[%d]��[%d]��: ȱ�ٹؼ���=", grpId, line);
            return -1;
        }
        *ch = 0;
        strTrim(p); /*�������ʽ*/
        if (!(pConvStmt = mallocOneConvStmt(pShmEntry))) return -1;
        pConvStmt->action = KW_SET;
     
        if (!(ch2 = strchr(p, '['))) {
            preScanExpress(pConvStmt->fldName, p, error);
            if (error[0]) {
                LOG_ERR("ת����[%d]��[%d]��: %s", grpId, line, error);
                return -1;
            }
        } else { 
            p2 = ch2+1;
            memcpy(pConvStmt->fldName, p, p2-p);
            pConvStmt->fldName[p2-p] = 0;
            if (!(ch2 = strchr(p2, ']'))) {
                LOG_ERR("ת����[%d]��[%d]��: �������ʽ����ȷ", grpId, line);
                return -1;
            }
            memcpy(tmpBuf, p2, ch2-p2);
            tmpBuf[ch2-p2] = 0;
            preScanExpress(pConvStmt->fldName+strlen(pConvStmt->fldName), tmpBuf, error);
            if (error[0]) {
                LOG_ERR("·����[%d]��[%d]��: %s", grpId, line, error);
                return -1;
            }
            strcat(pConvStmt->fldName, "]");
        }
        p = ch+1;
        strTrim(p); /*��ֵ���ʽ*/
        if (!(*p)) {
            LOG_ERR("ת����[%d]��[%d]��: ȱ����ֵ���ʽ", grpId, line);
            return -1;
        }
        preScanExpress(pConvStmt->express, p, error);
        if (error[0]) {
            LOG_ERR("ת����[%d]��[%d]��: %s", grpId, line, error);
            return -1;
        }
        return 0;
    }

    /*EXEC: {EXEC ���ʽ}*/ 
    if (memcmp(p, "EXEC ", 5) == 0) {
        p += 5;
        strTrim(p);
        if (!(*p)) {
            LOG_ERR("ת����[%d]��[%d]��: ȱ�ٱ��ʽ", grpId, line);
            return -1;
        }
        if (!(pConvStmt = mallocOneConvStmt(pShmEntry))) return -1;
        pConvStmt->action = KW_EXEC;
        preScanExpress(pConvStmt->express, p, error);
        if (error[0]) {
            LOG_ERR("ת����[%d]��[%d]��: %s", grpId, line, error);
            return -1;
        }
        return 0;
    }

    /*CONV: {CONV USING ת����}*/
    if (memcmp(p, "CONV ", 5) == 0) {
        p += 5;
        if (!(ch = strstr(p, "USING "))) {
            LOG_ERR("ת����[%d]��[%d]��: ȱ�ٹؼ���USING", grpId, line);
            return -1;
        }
        p = ch+6;
        strTrim(p); /*ת����*/
        if (!(*p)) {
            LOG_ERR("ת����[%d]��[%d]��: ȱ��ת����", grpId, line);
            return -1;
        }
        if (atoi(p)<=0) {
            LOG_ERR("ת����[%d]��[%d]��: ת���鲻��ȷ", grpId, line);
            return -1;
        }
        if (!(pConvStmt = mallocOneConvStmt(pShmEntry))) return -1;
        pConvStmt->action = KW_CONV;
        strncpy(pConvStmt->express, p, MAXEXPLEN);
        return 0;
    }

    /*RETURN: {RETURN}*/ 
    if (memcmp(p, "RETURN", 6) == 0) {
        if (!(pConvStmt = mallocOneConvStmt(pShmEntry))) return -1;
        pConvStmt->action = KW_RETURN;
        return 0;
    }
    LOG_ERR("ת����[%d]��[%d]��: �﷨����", grpId, line);
    return -1;
}

/***************************************************************
 * tpConvGrpLoad
 **************************************************************/
static int tpConvGrpLoad(SHM_ENTRY *pShmEntry, CONVGRP *pConvGrp, char *script)
{
    int i,j,k,grpId,line;
    char *p,*ch,*ap[3];
    CONVSTMT *pConvStmt;
    JUMP_TAG jumpTags[NMAXTAGS];
    STK_ITEM *item,*stack=NULL;
    STK_ITEM *item2,*stack2=NULL;

    grpId = pConvGrp->grpId;
    pConvGrp->nStart = pShmEntry->pShmList->convStmt.nItems;
    memset(jumpTags, 0, (NMAXTAGS * sizeof(JUMP_TAG)));

    for (i=0, line=0, p=script; script[i]!='\0'; i++) {
        if (script[i] == '\n') {
            script[i] = 0;
            if ((ch = strchr(p, '#'))) *ch = 0; /*����ע��*/
            strTrim(p);
            if (!p[0]) {
                p = &(script[i+1]);
                continue;
            }
            line++;
            if (preParseIF(p, ap) != 0) {
                LOG_ERR("ת����[%d]��[%d]��: �﷨����", grpId, line);        
                return -1;
            }
            for (j=0; j<3 && ap[j]!= NULL; j++) {
                if (tpConvStmtParse(grpId, line, ap[j], pShmEntry, jumpTags) != 0) { 
                    LOG_ERR("tpConvStmtParse() error");
                    return -1;
                }
            }
            p = &script[i+1];
        }
    }
    if (*p) { /*���һ�нű���ѭ���п���δ������*/
        if ((ch = strchr(p, '#'))) *ch = 0;
        strTrim(p);
        if (*p) {
            line++;
            if (preParseIF(script, ap) != 0) {
                LOG_ERR("ת����[%d]��[%d]��: �﷨����", grpId, line);        
                return -1;
            }
            for (j=0; j<3 && ap[j]!= NULL; j++) {
                if (tpConvStmtParse(grpId, line, ap[j], pShmEntry, jumpTags) != 0) { 
                    LOG_ERR("tpConvStmtParse() error");
                    return -1;
                }
            }
        }
    }
    pConvGrp->nStmts = pShmEntry->pShmList->convStmt.nItems - pConvGrp->nStart;

    for (i=pConvGrp->nStart; i<pShmEntry->pShmList->convStmt.nItems; i++) {
        pConvStmt = &pShmEntry->pConvStmt[i];
        switch (pConvStmt->action) {
            case KW_IF:
            case KW_ELSEIF:
            case KW_ELSE:
            case KW_ENDTHEN:
            case KW_WHILE:
            case KW_CONTINUE:
            case KW_EXIT:
            case KW_SWITCH:
            case KW_CASE:
            case KW_BREAK:
            case KW_DEFAULT:
                /*ѹջ*/
                if ((item = (STK_ITEM *)malloc(sizeof(STK_ITEM))) == NULL) {
                    LOG_ERR("malloc() error: %s", strerror(errno));
                    goto ERR_RET;
                }
                item->index = i;
                item->action = pConvStmt->action;
                item->next = stack;
                stack = item;
                break;
            case KW_JUMP:
                for (j=0; j<NMAXTAGS; j++) {
                    if (!jumpTags[j].name[0] || !strcmp(jumpTags[j].name, pConvStmt->express)) break;
                }
                if (j>=NMAXTAGS || !jumpTags[j].name[0]) {
                    LOG_ERR("ת����[%d]��δ�����ǩ[%s]", grpId, pConvStmt->express);
                    goto ERR_RET;
                }
                pConvStmt->action = DO_JUMP;
                pConvStmt->jump = jumpTags[j].index;
                break;        
            case KW_ENDIF:
                pConvStmt->action = DO_NONE;
                j = i;
                /*������ƥ���IF��ELSEIF��ELSE*/
                while (stack) {
                    if (stack->action == (char)KW_ELSE) {
                        pConvStmt = &pShmEntry->pConvStmt[stack->index];
                        pConvStmt->action = DO_NONE;
                        j = stack->index;
                        item = stack;
                        stack = stack->next;
                        free(item);
                    } else if (stack->action == (char)KW_ELSEIF) {
                        pConvStmt = &pShmEntry->pConvStmt[stack->index];
                        pConvStmt->action = DO_JUMP;
                        pConvStmt->jump = j; /*��ת����һ��ELSEIF��ELSE��ENDIF*/
                        j = stack->index;
                        item = stack;
                        stack = stack->next;
                        free(item);
                    } else if (stack->action == (char)KW_ENDTHEN) {
                        pConvStmt = &pShmEntry->pConvStmt[stack->index];
                        pConvStmt->action = DO_JUMP;
                        pConvStmt->jump = i; /*��ת��ENDIF*/
                        item = stack;
                        stack = stack->next;
                        free(item);
                    } else if (stack->action == (char)KW_IF) {
                        pConvStmt = &pShmEntry->pConvStmt[stack->index];
                        pConvStmt->action = DO_JUMP;
                        pConvStmt->jump = j; /*��ת��ELSEIF��ELSE��ENDIF*/
                        item = stack;
                        stack = stack->next;
                        free(item);
                        /*stack2��ջ->stackѹջ(���½�ջ)*/
                        item = stack2;
                        while (item) {
                            item2 = item->next;
                            item->next = stack;
                            stack = item;
                            item = item2;
                        }
                        stack2 = NULL;
                        break;
                    } else if (stack->action == (char)KW_EXIT ||
                               stack->action == (char)KW_CONTINUE) {
                        /*stack��ջ->stack2ѹջ(��ʱ��ջ)*/
                        item2 = stack;
                        stack = stack->next;
                        item2->next = stack2;
                        stack2 = item2;
                    } else {
                        LOG_ERR("ת����[%d]: �﷨����[>>>ENDIF]", grpId);
                        goto ERR_RET;
                    }
                } 
                break;
            case KW_ENDWHILE:
                /*������ƥ���WHILE*/
                j = -1;
                item = stack;
                while (item) {
                    if (item->action == (char)KW_WHILE) {
                        j = item->index;
                        break;
                    }
                    item = item->next;
                }
                if (j<0) {
                    LOG_ERR("ת����[%d]: �﷨����[>>>ENDWHILE]", grpId);
                    goto ERR_RET;
                }
                pConvStmt->action = DO_JUMP;
                pConvStmt->jump = j;
                /*������ƥ���CONTINUE��EXIT��WHILE*/
                while (stack) {
                    if (stack->action == (char)KW_CONTINUE) {
                        pConvStmt = &pShmEntry->pConvStmt[stack->index];
                        pConvStmt->action = DO_JUMP;
                        pConvStmt->jump = j; /*��ת��WHILE*/
                        item = stack;
                        stack = stack->next;
                        free(item);
                    } else if (stack->action == (char)KW_EXIT) {
                        pConvStmt = &pShmEntry->pConvStmt[stack->index];
                        pConvStmt->action = DO_JUMP;
                        pConvStmt->jump = i+1; /*��ת��ENDWHILE��һ��*/
                        item = stack;
                        stack = stack->next;
                        free(item);
                    } else if (stack->action == (char)KW_WHILE) {
                        pConvStmt = &pShmEntry->pConvStmt[stack->index];
                        pConvStmt->action = DO_JUMP;
                        pConvStmt->jump = i+1; /*��ת��ENDWHILE��һ��*/
                        item = stack;
                        stack = stack->next;
                        free(item);
                        break;
                    } else {
                        LOG_ERR("ת����[%d]: �﷨����[>>>ENDWHILE]", grpId);
                        goto ERR_RET;
                    }
                } 
                break;
            case KW_ENDSWITCH:
                pConvStmt->action = DO_NONE;
                j = i;
                /*������ƥ���SWITCH��CASE��BREAK��DEFAULT*/
                while (stack) {
                    if (stack->action == (char)KW_BREAK) {
                        pConvStmt = &pShmEntry->pConvStmt[stack->index];
                        pConvStmt->action = DO_JUMP;
                        pConvStmt->jump = j; /*��ת��ENDSWITCH*/
                        item = stack;
                        stack = stack->next;
                        free(item);
                    } else if (stack->action == (char)KW_DEFAULT) {
                        pConvStmt = &pShmEntry->pConvStmt[stack->index];
                        pConvStmt->action = DO_NONE;
                        k = stack->index;
                        item = stack;
                        stack = stack->next;
                        free(item);
                    } else if (stack->action == (char)KW_CASE) {
                        pConvStmt = &pShmEntry->pConvStmt[stack->index];
                        pConvStmt->action = DO_JUMP;
                        /*pConvStmt->jump = k; *��ת��DEFAULT���һCASE*/
                        if (k < 0) {
                            pConvStmt->jump = i;
                        } else {
                            pConvStmt->jump = k;
                        }
                        k = stack->index;
                        item = stack;
                        stack = stack->next;
                        free(item);
                    } else if (stack->action == (char)KW_SWITCH) {
                        pConvStmt = &pShmEntry->pConvStmt[stack->index];
                        pConvStmt->action = DO_NONE;
                        item = stack;
                        stack = stack->next;
                        free(item);
                        /*stack2��ջ->stackѹջ(���½�ջ)*/
                        item = stack2;
                        while (item) {
                            item2 = item->next;
                            item->next = stack;
                            stack = item;
                            item = item2;
                        }
                        stack2 = NULL;
                        break;
                    } else if (stack->action == (char)KW_EXIT ||
                               stack->action == (char)KW_CONTINUE) {
                        /*stack��ջ->stack2ѹջ(��ʱ��ջ)*/
                        item2 = stack;
                        stack = stack->next;
                        item2->next = stack2;
                        stack2 = item2;
                    } else {
                        LOG_ERR("ת����[%d]: �﷨����[>>>ENDSWITCH]", grpId);
                        goto ERR_RET;
                    }
                } 
                break;
            default:
                break;
        }

    }
    if (stack) {
        LOG_ERR("ת����[%d]: �﷨����", grpId);
        goto ERR_RET;
    }
    return 0;

ERR_RET:
    while (stack) {
        item = stack;
        stack = stack->next;
        free(item);
    }
    return -1;
}

/***************************************************************
 * tpConvLoad
 **************************************************************/
int tpConvLoad(SHM_ENTRY *pShmEntry)
{
    FILE *pp;
    int  i,j,grpId;
    char pathFile[256],buf[1024],script[1024*32];
    XMLDOC *doc;

    snprintf(pathFile, sizeof(pathFile), "ls %s/etc/TPCONV/*.xml", getenv("TPBUS_HOME"));
    if ((pp = popen(pathFile, "r")) == NULL) {
        LOG_ERR("popen() error: %s", strerror(errno));
        return -1;
    }

    while (fgets(pathFile, sizeof(pathFile), pp) != NULL) {
        if ((doc = loadXMLFile(strTrim(pathFile))) == NULL) {
            LOG_ERR("loadXMLFile() error");
            pclose(pp);
            return -1;
        }

        /*grpId*/
        if (getXMLText(doc, "grpId", buf, sizeof(buf)) != 0) {
            LOG_ERR("getXMLText() error");
            goto ERR_RET;
        }
        grpId = atoi(buf);
        i = (grpId % HASH_SIZE)+1;

        if (!pShmEntry->pConvGrp) {
            pShmEntry->pShmList->convGrp.nItems = HASH_SIZE;
            pShmEntry->pConvGrp = (CONVGRP *)malloc(sizeof(CONVGRP) * pShmEntry->pShmList->convGrp.nItems);
            if (!pShmEntry->pConvGrp) {
                LOG_ERR("malloc() error: %s", strerror(errno));
                goto ERR_RET;
            }
            memset(pShmEntry->pConvGrp, 0, (sizeof(CONVGRP) * pShmEntry->pShmList->convGrp.nItems)); 
        } else {
            if (pShmEntry->pConvGrp[i-1].grpId > 0) {
                pShmEntry->pShmList->convGrp.nItems += 1;
                pShmEntry->pConvGrp = (CONVGRP *)realloc(pShmEntry->pConvGrp, (sizeof(CONVGRP) * pShmEntry->pShmList->convGrp.nItems));
                if (!pShmEntry->pConvGrp) {
                    LOG_ERR("realloc() error: %s", strerror(errno));
                    goto ERR_RET;
                }
                i = pShmEntry->pShmList->convGrp.nItems;
                memset(&pShmEntry->pConvGrp[i-1], 0, sizeof(CONVGRP));
                j = (grpId % HASH_SIZE)+1;
                while (j>0) {
                    if (grpId == pShmEntry->pConvGrp[j-1].grpId) {
                        LOG_ERR("ת����[%d]�ظ�����", grpId);
                        goto ERR_RET;
                    }
                    if (!pShmEntry->pConvGrp[j-1].next) break;
                    j = pShmEntry->pConvGrp[j-1].next;
                }
                pShmEntry->pConvGrp[j-1].next = i;
            }
        }
        pShmEntry->pConvGrp[i-1].grpId = grpId;

        /*script*/
        if (getXMLText(doc, "script", script, sizeof(script)) != 0) {
            LOG_ERR("getXMLText() error");
            goto ERR_RET;
        }

        if (tpConvGrpLoad(pShmEntry, &pShmEntry->pConvGrp[i-1], script) != 0) {
            LOG_ERR("tpConvGrpLoad() error: grpId=[%d]", grpId);
            goto ERR_RET;
        }
        freeXML(doc);
    }
    pclose(pp);
    return 0;

ERR_RET:
    freeXML(doc);
    pclose(pp);
    return -1;
}

/***************************************************************
 * tpBizProcLoad
 **************************************************************/
int tpBizProcLoad(void **ppArea)
{
    int  ret,size;
    char libFile[256];
    char bizFile[256];
    char *err=NULL;
    void *hdl=NULL;
    struct stat stat_buf;
    int (*ptr)(char *, void *);

    *ppArea = NULL;

    snprintf(libFile, sizeof(libFile), "%s/lib/libtpbiz.so", getenv("TPBUS_HOME"));
    if (stat(libFile, &stat_buf) == -1) {
        if (errno == ENOENT) return 0;
        LOG_ERR("stat(%s) error: %s", libFile, strerror(errno));
        return -1;
    }

    snprintf(bizFile, sizeof(bizFile), "%s/etc/bizproc.tps", getenv("TPBUS_HOME"));
    if (stat(bizFile, &stat_buf) == -1) {
        if (errno == ENOENT) return 0;
        LOG_ERR("stat(%s) error: %s", bizFile, strerror(errno));
        return -1;
    }

    while (1) {
        if (!(hdl = dlopen(libFile, RTLD_LAZY))) {
            LOG_ERR("dlopen(%s) error: %s", libFile, dlerror());
            break;
        }
        dlerror();

        *(void **)(&ptr) = dlsym(hdl, "tpsLoad");
        if ((err = dlerror()) != NULL) {
            LOG_ERR("dlsym(%s) error: %s", "tpsLoad", err);
            break;
        }

        if ((size = (*ptr)(bizFile, NULL)) < 0) {
            LOG_ERR("tpsLoad(%s) retcode=[%d]", bizFile, size);
            break;
        }
        if (size == 0) return 0;

        if (!(*ppArea = (void *)malloc(size))) {
            LOG_ERR("malloc(%d) error: %s", size, strerror(errno));
            break;
        }

        if ((ret = (*ptr)(bizFile, *ppArea)) < 0) {
            LOG_ERR("tpsLoad(%s) retcode=[%d]", bizFile, ret);
            break;
        }
        dlclose(hdl);
        return size;
    }
    if (hdl) dlclose(hdl);
    if (*ppArea) free(*ppArea);
    return -1;
}

/***************************************************************
 * tpIndexPreSet
 **************************************************************/
int tpIndexPreSet(SHM_ENTRY *pShmEntry)
{
    int i,j,h,l,g;
    char *p;

    memcpy(&gShm, pShmEntry, sizeof(SHM_ENTRY));

    /*TPPORT*/
    for (i=NMINBINDQ; i<=NMAXBINDQ; i++) {
        if (!gShm.ptpPort[i-1].convGrp) continue;
        j = tpConvShmGet(gShm.ptpPort[i-1].convGrp, NULL);
        if (j<=0) { /*ֻ��鵫��Ԥ������*/
            LOG_ERR("ת����[%d]������", gShm.ptpPort[i-1].convGrp);
            return -1;
        }
    }

    /*TPSERV*/
    for (h=1; h<=HASH_SIZE; h++) {
        i = h;
        while (i>0 && gShm.ptpServ[i-1].servCode[0]) {
            /*routGrp*/
            j = tpRoutShmGet(gShm.ptpServ[i-1].routGrp, NULL);
            if (j<=0) {
                LOG_ERR("·����[%d]������", gShm.ptpServ[i-1].routGrp);
                return -1;
            }
            gShm.ptpServ[i-1].routGrp = j;

            i = gShm.ptpServ[i-1].next;
        }
    }

    /*TPROC*/
    for (h=NMINBINDQ; h<=NMAXBINDQ; h++) {
        i = h;
        while (i>0 && gShm.ptpProc[i-1].desQ) {
            /*packGrp*/
            j = tpConvShmGet(gShm.ptpProc[i-1].packGrp, NULL);
            if (j<=0) {
                LOG_ERR("ת����[%d]������", gShm.ptpProc[i-1].packGrp);
                return -1;
            }
            gShm.ptpProc[i-1].packGrp = j;

            /*unpkGrp*/
            j = tpConvShmGet(gShm.ptpProc[i-1].unpkGrp, NULL);
            if (j<=0) {
                LOG_ERR("ת����[%d]������", gShm.ptpProc[i-1].unpkGrp);
                return -1;
            }
            gShm.ptpProc[i-1].unpkGrp = j;

            /*revPackGrp*/
            if (gShm.ptpProc[i-1].revPackGrp > 0) {
                j = tpConvShmGet(gShm.ptpProc[i-1].revPackGrp, NULL);
                if (j<=0) { /*ֻ��鵫��Ԥ������*/
                    LOG_ERR("ת����[%d]������", gShm.ptpProc[i-1].revPackGrp);
                    return -1;
                }
            }

            /*revUnpkGrp*/
            if (gShm.ptpProc[i-1].revUnpkGrp > 0) {
                j = tpConvShmGet(gShm.ptpProc[i-1].revUnpkGrp, NULL);
                if (j<=0) { /*ֻ��鵫��Ԥ������*/
                    LOG_ERR("ת����[%d]������", gShm.ptpProc[i-1].revUnpkGrp);
                    return -1;
                }
            }

            i = gShm.ptpProc[i-1].next;
        }
    }

    /*TPMAPP*/
    for (h=NMINBINDQ; h<=NMAXBINDQ; h++) {
        i = h;
        while (i>0 && gShm.ptpMapp[i-1].orgQ) {
            if (!gShm.ptpPort[gShm.ptpMapp[i-1].orgQ-1].bindQ) {
                LOG_ERR("����˵�[%d]������", gShm.ptpMapp[i-1].orgQ);
                return -1;
            }
            if (gShm.ptpPort[gShm.ptpMapp[i-1].orgQ-1].portType != 0) {
                LOG_ERR("����˵�[%d]���Ͳ���ȷ", gShm.ptpMapp[i-1].orgQ);
                return -1;
            }

            /*servIndex*/
            j = tpServShmGet(gShm.ptpMapp[i-1].servCode, NULL);
            if (j<=0) {
                LOG_ERR("�޷���[%s]ע����Ϣ", gShm.ptpMapp[i-1].servCode);
                return -1;
            }
            gShm.ptpMapp[i-1].servIndex = j;

            /*tranCodeReg*/
            for (l=0,p=gShm.ptpMapp[i-1].tranCode; *p; p++) {
                switch (*p) {
                    case '*':
                        memcpy(gShm.ptpMapp[i-1].tranCodeReg+l, ".*", 2);
                        l += 2;
                        break;
                    case '?':
                        memcpy(gShm.ptpMapp[i-1].tranCodeReg+l, ".{1}", 4);
                        l += 4;
                        break;
                    default:
                        gShm.ptpMapp[i-1].tranCodeReg[l++] = *p;
                        break;
                }
            }

            if (!gShm.ptpMapp[i-1].isThrough) {
                /*unpkGrp*/
                j = tpConvShmGet(gShm.ptpMapp[i-1].unpkGrp, NULL);
                if (j<=0) {
                    LOG_ERR("ת����[%d]������", gShm.ptpMapp[i-1].unpkGrp);
                    return -1;
                }
                gShm.ptpMapp[i-1].unpkGrp = j;

                /*packGrp*/
                if (gShm.ptpMapp[i-1].packGrp > 0) {
                    j = tpConvShmGet(gShm.ptpMapp[i-1].packGrp, NULL);
                    if (j<0) {
                        LOG_ERR("ת����[%d]������", gShm.ptpMapp[i-1].packGrp);
                        return -1;
                    }
                    gShm.ptpMapp[i-1].packGrp = j;
                }
            }
            i = gShm.ptpMapp[i-1].next;
        }
    }

    /*TPROUT*/
    for (i=1; i<=gShm.pShmList->routStmt.nItems; i++) {
        if (DO_SEND == gShm.pRoutStmt[i-1].action) {
            if (!gShm.ptpPort[gShm.pRoutStmt[i-1].desQ-1].bindQ) {
                LOG_ERR("Ŀ��˵�[%d]������", gShm.pRoutStmt[i-1].desQ);
                return -1;
            }
            if (gShm.ptpPort[gShm.pRoutStmt[i-1].desQ-1].portType == 2) { /*��������˵�*/
                if (strcmp(gShm.pRoutStmt[i-1].procCode, "NONE") == 0) {
                    gShm.pRoutStmt[i-1].procCode[0] = 0;
                    continue;
                }
            }

            /*procCode*/
            g = atoi(gShm.pRoutStmt[i-1].procCode);
            if (g>0) {
                if ((j = tpConvShmGet(g, NULL)) <= 0) {
                    LOG_ERR("ת����[%d]������", g);
                    return -1;
                }
                snprintf(gShm.pRoutStmt[i-1].procCode, MAXKEYLEN+1, "%d", j);
            }
        } else if (DO_CALL == gShm.pRoutStmt[i-1].action) {
            if (!gShm.ptpPort[gShm.pRoutStmt[i-1].desQ-1].bindQ) {
                LOG_ERR("Ŀ��˵�[%d]������", gShm.pRoutStmt[i-1].desQ);
                return -1;
            }
            if (!gShm.ptpPort[gShm.pRoutStmt[i-1].desQ-1].portType) {
                LOG_ERR("Ŀ��˵�[%d]���Ͳ���ȷ", gShm.pRoutStmt[i-1].desQ);
                return -1;
            }
            if (gShm.ptpPort[gShm.pRoutStmt[i-1].desQ-1].portType == 2) { /*��������˵�*/
                if (strcmp(gShm.pRoutStmt[i-1].procCode, "NONE") == 0) {
                    gShm.pRoutStmt[i-1].procCode[0] = 0;
                    continue;
                }
            }
            if (strcmp(gShm.pRoutStmt[i-1].procCode, "NONE") == 0) {
                LOG_ERR("Ŀ��˵�[%d]���Ͳ���ȷ", gShm.pRoutStmt[i-1].desQ);
                return -1;
            }
            if (gShm.ptpPort[gShm.pRoutStmt[i-1].desQ-1].portType == 2) continue;
            
            /*procCode*/
            j = tpProcShmGet(gShm.pRoutStmt[i-1].desQ, gShm.pRoutStmt[i-1].procCode, NULL);
            if (j<=0) {
                LOG_ERR("�˵�[%d]->����[%s]������", gShm.pRoutStmt[i-1].desQ, gShm.pRoutStmt[i-1].procCode);
                return -1;
            }
            gShm.pRoutStmt[i-1].procIndex = j;

            if (gShm.pRoutStmt[i-1].revFlag) {
                if (gShm.ptpProc[j-1].revPackGrp <= 0) {
                    LOG_ERR("����[%s]δ���ó������ת����", gShm.pRoutStmt[i-1].procCode);
                    return -1;
                }
                if (!gShm.pRoutStmt[i-1].noRespRev && gShm.ptpProc[j-1].revUnpkGrp <= 0) {
                    LOG_ERR("����[%s]δ���ó������ת����", gShm.pRoutStmt[i-1].procCode);
                    return -1;
                }
            }
        }
    }
    return 0;
}

