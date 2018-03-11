/***************************************************************
 * ��־��������
 ***************************************************************
 * setLogFile():        ������־�ļ�
 * setLogLevel():       ������־����
 * hasLogLevel():       �����־����
 * logPrefixEnable():   ����ǰ׺��ʶ
 * setLogPrefix():      ����ǰ׺����
 * getLogPrefix():      ȡ��ǰ׺����
 * setLogCallBack():    ���ûص�����
 * openLogFile():       ����־�ļ�
 * logOut():            ��ӡ��־��Ϣ
 * logOutHex():         ��ӡʮ�����Ʊ���
 * logOutStart():       ��ʼ��־��ӡ(�����ⲿ�����Զ������)
 * logOutEnd():         ������־��ӡ(�����ⲿ�����Զ������)
 **************************************************************/
#include "log.h"

static char m_file[256] = {0};
static char m_level = ERROR|WARN|INFO;
static char m_prefix = 0;
static void (*m_cb)(const char *) = NULL;

static pthread_mutex_t m_mtx;
static pthread_once_t once_ctl = PTHREAD_ONCE_INIT;

/***************************************************************
 * ��ʼ����־����
 **************************************************************/
static void logInit()
{
    pthread_mutex_init(&m_mtx, NULL);
}

/***************************************************************
 * ������־�ļ�
 **************************************************************/
char *getLogFile(char *file)
{
    strncpy(file,m_file, sizeof(m_file)-1);
    return file;
}
/***************************************************************
 * ������־�ļ�
 **************************************************************/
void setLogFile(const char *file)
{
    strncpy(m_file, file, sizeof(m_file)-1);
}


/***************************************************************
 * ������־����
 **************************************************************/
void setLogLevel(int level)
{
    switch (level) {
        case ERROR:
            m_level = ERROR;
            break;
        case WARN:
            m_level = ERROR|WARN;
            break;
        case INFO:
            m_level = ERROR|WARN|INFO;
            break;
        case DEBUG:
            m_level = ERROR|WARN|INFO|DEBUG;
            break;
        default:
            break;
    }
}

/***************************************************************
 * �����־����
 **************************************************************/
int hasLogLevel(int level)
{
    switch (level) {
        case ERROR:
        case WARN:
        case INFO:
        case DEBUG:
            return (level & m_level);          
        default:
          return 0;
    }
}

/***************************************************************
 * ����ǰ׺��ʶ
 **************************************************************/
int logPrefixEnable()
{
    if (pthread_key_create(&TSD_LOG_KEY, NULL) != 0) return -1;
    if (pthread_setspecific(TSD_LOG_KEY, NULL) != 0) return -1;
    m_prefix = 1;
    return 0;
}

/***************************************************************
 * ����ǰ׺����
 **************************************************************/
int setLogPrefix(const char *prefix)
{
    if (!m_prefix) return -1;
    if (pthread_setspecific(TSD_LOG_KEY, prefix) != 0) return -1;
    return 0;
}

/***************************************************************
 * ȡ��ǰ׺����
 **************************************************************/
char *getLogPrefix()
{
    if (!m_prefix) return NULL;
    return (char *)pthread_getspecific(TSD_LOG_KEY);
}

/***************************************************************
 * ���ûص�����
 **************************************************************/
void setLogCallBack(void (*cb)(const char *))
{
    m_cb = cb;
}

/***************************************************************
 * ����־�ļ�
 **************************************************************/
static FILE *openLogFile()
{
    FILE *fp;
    char cmd[256];
    time_t t;
    struct stat stat_buf;

    if (stat(m_file, &stat_buf) == -1) {
        if ((fp = fopen(m_file, "w+")) == NULL) return NULL;
    } else {
        if (stat_buf.st_size < MAX_LOGFILE_SIZE) {
            if ((fp = fopen(m_file, "a")) == NULL) return NULL;
        } else {
            time(&t);
            snprintf(cmd, sizeof(cmd), "mv %s %s.%ld", m_file, m_file, t);
            system(cmd);
            if ((fp = fopen(m_file, "w+")) == NULL) return NULL;
        }
    }
    return fp;
}

/***************************************************************
 * ��ӡ��־��Ϣ
 **************************************************************/
void logOut(int level, const char *label, const char *file, int line, const char *fmt, ...)
{
    FILE *fp;
    int  off;
    char dtm[20],buf[MAX_LOGDUMP_SIZE];
    char *pprefix = NULL;
    va_list args;

    if (!(level & m_level)) return;

    pthread_once(&once_ctl, logInit);
    pthread_mutex_lock(&m_mtx);

    if ((fp = openLogFile()) == NULL) {
        pthread_mutex_unlock(&m_mtx);
        return;
    }

    if (m_prefix) pprefix = (char *)pthread_getspecific(TSD_LOG_KEY);
    if (!pprefix) pprefix = "\0";
    snprintf(buf, sizeof(buf), "%s%s|%s|%s->%d: ", pprefix, label, getDateTime(dtm), file, line);
    off = strlen(buf);
    va_start(args, fmt);
    vsnprintf(buf+off, sizeof(buf)-off, fmt, args);
    va_end(args);
    fprintf(fp, buf);
    fprintf(fp, "\n");

    fflush(fp);
    fclose(fp);
    pthread_mutex_unlock(&m_mtx);

    /* ����򾯸���־�ɻص�Ӧ�ú��������ض����� */
    if (m_cb && (level == ERROR || level == WARN)) m_cb(buf);
}


/***************************************************************
 * ��ӡʮ�����Ʊ���
 **************************************************************/
void logOutHex(const char *buf, int len)
{
    FILE *fp;
    int i,j,n;
    char *pprefix = NULL;
    unsigned char *abuf,*hbuf;

    pthread_once(&once_ctl, logInit);
    pthread_mutex_lock(&m_mtx);

    if ((fp = fopen(m_file, "a")) == NULL) return;

    hbuf = (unsigned char *)buf;
    abuf = (unsigned char *)buf;

    if (m_prefix) pprefix = (char *)pthread_getspecific(TSD_LOG_KEY);
    if (!pprefix) pprefix = "\0";

    fprintf(fp, "%s*** ʮ�����Ʊ��Ŀ�ʼ ***\n", pprefix);
    for (i=0, n=len/16; i<n; i++) {
        fprintf(fp, "%s%6.6d=>", pprefix, i*16);
        for (j=0; j<16; j++) fprintf(fp, "%02x ", *hbuf++);
        for (j=0; j<16; j++, abuf++) fprintf(fp, "%c", (*abuf>31) ? ((*abuf<127) ? *abuf : '*') : '.');
        fprintf(fp, "\n");
    }
    if ((i=len%16) > 0) {
        fprintf(fp, "%s%6.6d=>", pprefix, len-len%16);
        for (j=0; j<i; j++) fprintf(fp, "%02x ",*hbuf++);
        for (j=i; j<16; j++) fprintf(fp, "   ");
        for (j=0; j<i; j++, abuf++) fprintf(fp, "%c", (*abuf>31) ? ((*abuf<127) ? *abuf : '*') : '.');
        fprintf(fp, "\n");
    }
    fprintf(fp, "%s*** ʮ�����Ʊ��Ľ��� ***\n", pprefix);

    fflush(fp);
    fclose(fp);
    pthread_mutex_unlock(&m_mtx);
}

/***************************************************************
 * ��ʼ��־��ӡ(�����ⲿ�����Զ������)
 **************************************************************/
FILE *logOutStart()
{
    FILE *fp;

    pthread_once(&once_ctl, logInit);
    pthread_mutex_lock(&m_mtx);

    if ((fp = fopen(m_file, "a")) == NULL) {
        pthread_mutex_unlock(&m_mtx);
        return NULL;
    }
    return fp;
}

/***************************************************************
 * ������־��ӡ(�����ⲿ�����Զ������)
 **************************************************************/
void logOutEnd(FILE *fp)
{
    fflush(fp);
    fclose(fp);
    pthread_mutex_unlock(&m_mtx);
}

/*############################################################*/
/* ����˳��������ʵ��С����                                 */
/*############################################################*/

/***************************************************************
 * ȥ���ַ������ҿո�
 **************************************************************/
char *strTrim(char *s)
{
    int i,l,r,len;

    for (len=0; s[len]; len++);
    for (l=0; (s[l]==' ' || s[l]=='\t' || s[l]=='\n'); l++);
    if (l==len) {
        s[0] = '\0';
        return s;
    }
    for (r=len-1; (s[r]==' ' || s[r]=='\t' || s[r]=='\n'); r--);
    for (i=l; i<=r; i++) s[i-l] = s[i];
    s[r-l+1] = '\0';
    return s;
}

/***************************************************************
 * ȥ���ַ�����߿ո�
 **************************************************************/
char *strTrimL(char *s)
{
    int i,l,r,len;

    for (len=0; s[len]; len++);
    for (l=0; (s[l]==' ' || s[l]=='\t' || s[l]=='\n'); l++);
    if (l==len) {
        s[0]='\0';
        return s;
    }
    r = len-1;
    for (i=l; i<=r; i++) s[i-l] = s[i];
    s[r-l+1] = '\0';
    return s;
}

/***************************************************************
 * ȥ���ַ����ұ߿ո�
 **************************************************************/
char *strTrimR(char *s)
{
    int r,len;

    for (len=0; s[len]; len++);
    for (r=len-1; (s[r]==' ' || s[r]=='\t' || s[r]=='\n'); r--);
    s[r+1] = '\0';
    return s;
}

/***************************************************************
 * �ַ����滻
 **************************************************************/
char *strRepl(char *s, const char *s1, const char *s2)
{
    int len,cnt;
    char *p,*sp,*dp,*pos=s;

    while (1) {
        if ((p = strstr(pos, s1)) == NULL) return s;
        len = strlen(s2) - strlen(s1);
        if (len) {
            sp = p + strlen(s1);
            dp = sp + len;
            cnt = strlen(sp) + 1;
            memmove(p+strlen(s1)+len, p+strlen(s1), cnt);
        }
        memcpy(p, s2, strlen(s2));
        pos = p + strlen(s2);
        if (*pos == 0) return s;
    }
}

/***************************************************************
 * memTrimCopy
 **************************************************************/
int memTrimCopy(char *ds, char *ss, int n)
{
    int i,j,l;

    if (n<=0) return 0;
    for (i=0;i<n && (ss[i]   == ' ' || ss[i]   == '\t' || ss[i]   == '\n'); i++) {}
    for (j=n;j>0 && (ss[j-1] == ' ' || ss[j-1] == '\t' || ss[j-1] == '\n'); j--) {}
    l = j-i;
    if (l<=0) return 0;
    memcpy(ds, ss+i, l);
    return l;
}

/***************************************************************
 * ������ʽƥ��
 **************************************************************/
int regMatch(const char *str, const char *exp)
{
    int ret;
    regex_t regbuf;

    if ((ret = regcomp(&regbuf, exp, REG_EXTENDED)) != 0) {
        regfree(&regbuf);
        return ret;
    }
    ret = regexec(&regbuf, str, 0, NULL, 0);
    regfree(&regbuf);
    return ret; /*ƥ��ʱ����0*/
}

/***************************************************************
 * ȡϵͳ��ǰ���ں�ʱ��
 **************************************************************/
char *getDateTime(char *result)
{
    time_t t;
    struct tm *t_buf;
     struct tm st_buf;
    time(&t);
     t_buf = localtime_r(&t,&st_buf);
    sprintf(result, "%04d-%02d-%02d %02d:%02d:%02d",
            t_buf->tm_year+1900, t_buf->tm_mon+1, t_buf->tm_mday, 
            t_buf->tm_hour, t_buf->tm_min, t_buf->tm_sec);
    return result; /* yyyy-MM-dd hh:mm:ss */
}

/***************************************************************
 * ���ظ�ʽ�����ʱ�䴮
 **************************************************************/
char *getTimeString(time_t t, const char *fmt, char *result)
{
    char buf[5];
    struct tm *t_buf;
     struct tm st_buf;
     t_buf = localtime_r(&t,&st_buf);

    if (fmt) strcpy(result, fmt);
    else strcpy(result, "yyyy-MM-dd hh:mm:ss");
        

    sprintf(buf, "%04d", t_buf->tm_year+1900);
    strRepl(result, "yyyy", buf);
    sprintf(buf, "%02d", t_buf->tm_year-100);
    strRepl(result, "yy", buf);
    sprintf(buf, "%02d", t_buf->tm_mon+1);
    strRepl(result, "MM", buf);
    sprintf(buf, "%02d", t_buf->tm_mday);
    strRepl(result, "dd", buf);
    sprintf(buf, "%02d", t_buf->tm_hour);
    strRepl(result, "hh", buf);
    sprintf(buf, "%02d", t_buf->tm_min);
    strRepl(result, "mm", buf);
    sprintf(buf, "%02d", t_buf->tm_sec);
    strRepl(result, "ss", buf);
    
    return result;
}

/***************************************************************
 * ����ָ��������ʱ��������ʱ��
 **************************************************************/
time_t makeTime(int year, int mon, int day, int hour, int min, int sec)
{
    time_t t;
    struct tm *t_buf;
     struct tm st_buf;
    time(&t);
     t_buf = localtime_r(&t,&st_buf);


    t_buf->tm_year = year - 1900;
    t_buf->tm_mon  = mon - 1;
    t_buf->tm_mday = day;
    t_buf->tm_hour = hour;
    t_buf->tm_min  = min;
    t_buf->tm_sec  = sec;

    return mktime(t_buf);
}

