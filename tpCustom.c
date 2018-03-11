/***************************************************************
 * �ͻ����ű�������
 **************************************************************/
#include "tpbus.h"
#include "xml.h"

static char *m_macroDefs = NULL;

static char b64encmap[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static int tp_DEMO(int argc, TPVAR argv[], char *result, int size);
static int tp_CHGRET(int argc, TPVAR argv[], char *result, int size);
static int tp_PINCONV(int argc, TPVAR argv[], char *result, int size);
static int Str2Hex(char *sSrc, char *sDest, int nSrcLen);
static int Hex2Str( char *sSrc,  char *sDest, int nSrcLen );
static int tp_SPLITDT(int argc, TPVAR argv[], char *result, int size);
static int tp_BASE64ENC(int argc, TPVAR argv[], char *result, int size);
static int tp_BASE64DEC(int argc, TPVAR argv[], char *result, int size);
static int tp_DOUB2INT(int argc, TPVAR argv[], char *result, int size);
static int tp_INT2DOUB(int argc, TPVAR argv[], char *result, int size);
static int tp_ENCODING(int argc, TPVAR argv[], char *result, int size);
static int tp_ISSEQ(int argc, TPVAR argv[], char *result, int size);
static int tp_CUSTINFO2NETACQ(int argc, TPVAR argv[], char *result, int size);
static int tp_SYSDT(int argc, TPVAR argv[], char *result, int size);
static int tp_CHKCARDRELATION(int argc, TPVAR argv[], char *result, int size);
static int tp_INFO(int argc, TPVAR argv[], char *result, int size);
static int tp_TRIMLCHR(int argc, TPVAR argv[], char *result, int size);
static int tp_MKTIME(int argc, TPVAR argv[], char *result , int size);
static int tp_SETIDNO15_18(int argc, TPVAR argv[], char *result, int size);
static int tp_CHECKAMOUNT(int argc, TPVAR argv[], char *result, int size);
static int tp_CALHSMMAC(int argc, TPVAR argv[], char *result, int size);
static int convert(iconv_t cd, unsigned char *inbuf, int *inlen, unsigned char *outbuf, int *outlen);

static TPFUN  TPFUN_LIST[] = {
       {"DEMO",         tp_DEMO       }, /* ���� */
       {"CHGRET",       tp_CHGRET     }, /* ���� */
       {"PINCONV",      tp_PINCONV    }, /* 16λpinblockת8λѹ���� */
       {"SPLITDT",      tp_SPLITDT    }, /* YYMMDDHHMMSSת��YYYY-MM-DD HHMMSS */
       {"BASE64ENC",    tp_BASE64ENC  }, /* base64���� */
       {"BASE64DEC",    tp_BASE64DEC  }, /* base64���� */
       {"DOUB2INT",     tp_DOUB2INT   }, /* double����ֵתΪ���ͱ�ʾ */
       {"INT2DOUB",     tp_INT2DOUB   }, /* ������ֵתΪdouble�ͱ�ʾ */
       {"ENCODING",     tp_ENCODING   }, /* �ַ�������ת�� */
       {"ISSEQ",        tp_ISSEQ      }, /* �ж��ַ���(Ӧ����)����ȫĳ���ַ� */
       {"GETNETACQAUTHINFO",  tp_CUSTINFO2NETACQ},/* �������յ��ͻ���Ϣת�� */
       {"SETIDNO15_18", tp_SETIDNO15_18},/* ʡ��֤��15λת18λ */
       {"GETAUTHINFO",  tp_INFO},        /* ת��ʵ����֤�������ÿͻ���Ϣ��ʽ */       
       {"SYSDT",        tp_SYSDT      },          /* ��ȡϵͳ����ʱ��YYYYMMDDhhmmssXX */
       {"CHKCARDRELATION",    tp_CHKCARDRELATION},/* ���󶨹�ϵ�Ƿ���� */
       {"CHECKAMOUNT",  tp_CHECKAMOUNT}, /* �ж��Ƿ���� */              
       {"TRIMLCHR",     tp_TRIMLCHR   }, /* ɾ���ַ������ָ���ַ� */
       {"MKTIME",       tp_MKTIME     }, /* ʱ��YYYYMMDDhhmmssת��Ϊʱ��� */
       {"HSMMAC",		tp_CALHSMMAC  }, /* ����MAC����*/
       {"",             NULL          }           /* END! */
};

int tpGetParamString(const char *params, const char *name, char *value)
{
    int tmpLen,offset=0;
    const char *p;

    while (1) {
        p = params+offset;
        while (params[offset++]) {}
        if (!(*p)) break;
        tmpLen = ((unsigned char)params[offset])*255 + ((unsigned char)params[offset+1]);
        offset += 2;
        if (strcmp(p, name) == 0) {
            memcpy(value, params+offset, tmpLen);
            value[tmpLen] = 0;
            return 0;
        }
        offset += tmpLen;
    }
    return -1;
}

/***************************************************************
 * �Զ����ע��
 **************************************************************/
TPFUN *tpCustomInit()
{
    return TPFUN_LIST;
}

/***************************************************************
 * ��������
 **************************************************************/
static int tp_DEMO(int argc, TPVAR argv[], char *result, int size)
{
    strncpy(result, "Return by custom", size-1);
    return strlen(result);
}

static int tp_CHGRET(int argc, TPVAR argv[], char *result, int size)
{
    if (argc != 1) {
        LOG_ERR("������������ȷ");
        return -1;
    }

    result[0] = 0; 
    if (memcmp(argv[0].buf, "000000", 6) == 0) {
      memcpy(result, "S000", 4);
    }
    else if (memcmp(argv[0].buf, "999999", 6) == 0) {
      memcpy(result, "S999", 4);
    }
    else {

    }

    return 4;
}

static int tp_PINCONV( int argc, TPVAR argv[], char *result, int size)
{
    if (argc != 1) {
        LOG_ERR("������������ȷ");
        return -1;
    }
    if (argv[0].len != 16) {
        LOG_ERR("�������Ȳ���ȷ");
        return -2;
    }
    memset(result, 0x00, argv[0].len);

    Str2Hex(argv[0].buf, result, argv[0].len);
    return argv[0].len/2;
}

static int Str2Hex( char *sSrc, char *sDest, int nSrcLen )
{
    int i, nHighBits, nLowBits;
 
    for( i = 0; i < nSrcLen; i += 2 )
    {
        nHighBits = sSrc[i];
        nLowBits  = sSrc[i + 1];
 
        if( !isxdigit(nHighBits) )
            return -1;
 
        if( nHighBits > 0x39 )
            nHighBits -= 0x37;
        else
            nHighBits -= 0x30;
 
        if( i == nSrcLen - 1 )
            nLowBits = 0;
        else if( !isxdigit(nLowBits) )
            return -1;
        else if( nLowBits > 0x39 )
            nLowBits -= 0x37;
        else
            nLowBits -= 0x30;
 
        sDest[i / 2] = (nHighBits << 4) | (nLowBits & 0x0f);
    }
 
    return 0;
 
} /* End of Str2Hex */


static int Hex2Str( char *sSrc,  char *sDest, int nSrcLen )
{
    int  i;
    char szTmp[3];
 
    for( i = 0; i < nSrcLen; i++ )
    {
        sprintf( szTmp, "%02X", (unsigned char) sSrc[i] );
        memcpy( &sDest[i * 2], szTmp, 2 );
    }
    return 0;
}

/* ����:
 *     TRANDATE   YYYYMMDD
 *     FLD7       MMDDHHMMSS
 *     option     D/T
 */
static int tp_SPLITDT(int argc, TPVAR argv[], char *result, int size)
{
    char syear[4+1]; 
    char smonth[2+1];
    char sday[2+1];
    char stm[6+1];

    if (argc != 3) {
        LOG_ERR("������������ȷ[%d][3]",argc);
        return -1;
    }
    if (argv[0].len != 8) {
        LOG_ERR("����1���Ȳ���ȷ[%d][8]",argv[0].len);
        return -2;
    }
    if (argv[1].len != 10) {
        LOG_ERR("����2���Ȳ���ȷ[%d][10]",argv[1].len);
        return -3;
    }
    if (argv[2].buf[0] != 'D' && argv[2].buf[0] != 'T') {
        LOG_ERR("����3����ȷ[%c][D/T]",argv[0].buf[0]);
        return -4;
    }
    memset(result, 0x00, argv[0].len);

    memcpy(syear, argv[0].buf, 4);
    syear[4] = 0x00;
    memcpy(smonth, argv[1].buf, 2);
    smonth[2] = 0x00;
    memcpy(sday, &argv[1].buf[2], 2);
    sday[2] = 0x00;
    memcpy(stm, &argv[1].buf[4], 6);
    switch (argv[2].buf[0])
    {
        case 'D':
            sprintf(result, "%04s-%02s-%02s",syear, smonth, sday);
            break;
        case 'T':
            sprintf(result, "%06s",stm);
            break;
        default:
            return 0;
    }
    return strlen(result);
}

static char GetCharIndex(char c)
{
    if((c >= 'A') && (c <= 'Z'))  
        return c - 'A';  
    else if((c >= 'a') && (c <= 'z'))  
        return c - 'a' + 26;  
    else if((c >= '0') && (c <= '9'))  
        return c - '0' + 52;  
    else if(c == '+')  
        return 62;  
    else if(c == '/')  
        return 63;  
    else if(c == '=')  
        return 0;  
    return 0;  
}

/*���룬��������������ģ����ĳ��� */
/*���뺯��*/
static int base64_decode(char *lpString, const char *lpSrc, int sLen)
{
    char    lpCode[4];
    int     vLen = 0;
    /*����ָ�� */
    char    *ind;
    /*��ʼ��ָ��*/
    ind     = (char *)lpSrc;

    /*Base64���볤�ȱض���4�ı���������'='*/
    if(sLen % 4)
    {
        lpString[0] = '\0';
        return -1;
    }
    /*���������ַ�������*/
    while(sLen > 2)
    {
        lpCode[0] = GetCharIndex(lpSrc[0]);  
        lpCode[1] = GetCharIndex(lpSrc[1]);  
        lpCode[2] = GetCharIndex(lpSrc[2]);  
        lpCode[3] = GetCharIndex(lpSrc[3]);

        *lpString++ = (lpCode[0] << 2) | (lpCode[1] >> 4);
        *lpString++ = (lpCode[1] << 4) | (lpCode[2] >> 2);
        *lpString++ = (lpCode[2] << 6) | (lpCode[3]);

        lpSrc += 4;
        sLen -= 4;
        vLen += 3;
    }
    if (lpCode[3] == 0x00) vLen--;
    if (lpCode[2] == 0x00) vLen--;
    lpString[vLen] = 0x00;
    return vLen;
}

static int tp_BASE64DEC(int argc, TPVAR argv[], char *result, int size)
{
    int len;
    if (argc != 1) {
        LOG_ERR("������������ȷ[%d][1]",argc);
        return -1;
    }

    len = base64_decode(result, argv[0].buf, argv[0].len); 
    return len;
}

/* 
 * * Encode a buffer into base64 format 
 * * Return -1   encode fail
 * *        n > 0 encoded msg len
 * */  
/** base64 encoding map for shifted byte **/

static int base64_encode(char *dst, const char *src, int slen)  
{  
    int i, n,nl;  
    unsigned int C1, C2, C3;  
    const char *p;  
    char *ptr = dst;

    if (0 == slen)  
        return -1;  

    n = slen/3;  
    nl = slen%3;

    /* 3-bytes groups left 1/2 bytes,need added in */
    if (nl)
        n += 1;

    p = src;
    for (i=0;i<n;i++)
    {
        C1 = *p++;
        if (1 == nl && (n-1) == i)
            C2 = 0;
        else
            C2 = *p++;
        if (nl && (n-1) == i)
            C3 = 0;
        else
            C3 = *p++;


        *ptr++ = b64encmap[(C1 >> 2) & 0x3F];
        *ptr++ = b64encmap[(((C1 << 6) >> 2) & 0x3F) | ((C2 >> 4) & 0x0F)];
        *ptr++ = b64encmap[(((C2 << 4) >> 2) & 0x3F) | ((C3 >> 6) & 0x03)];
        *ptr++ = b64encmap[C3 & 0x3F];
    }
    *ptr = '\0';
    switch (nl)
    {
        case 1:
            *(ptr-2) = '=';
        case 2:
            *(ptr-1) = '=';
            break;
        default:
            break;
    }
    return n << 2;
} 

static int tp_BASE64ENC(int argc, TPVAR argv[], char *result, int size)
{
    int len;

    if (argc != 1) {
        LOG_ERR("������������ȷ[%d][1]",argc);
        return -1;
    }

    len = base64_encode(result, argv[0].buf, argv[0].len); 
    return len;
}

/* �����п�ֵ 0ֵ
 * ��ʽ���ϸ�����99.99
 * �����벻ʹ��atof() -> / 100�Ĳ���
 */
static int tp_DOUB2INT(int argc, TPVAR argv[], char *result, int size)
{
    char lres[8];
    int  dotn;
	char *pi;

    if (argc != 2) {
        LOG_ERR("������������ȷ[%d][2]",argc);
        return -1;
    }

    if (argv[1].len !=1) {
        LOG_ERR("����2���ȴ���[%d][1]",argv[1].len);
        return -1;
    }

    if (!isdigit(argv[1].buf[0])) {
        LOG_ERR("����2���ʹ���[%s][number]", argv[1].buf);
        return -1;
    }

    dotn = atoi(argv[1].buf);

    pi = strchr(argv[0].buf, '.');
    if (argv[0].buf + argv[0].len - 1 - pi != dotn) {
        LOG_ERR("����1��ʽ����[%s][double]", argv[0].buf);
        return -2;
    }

    snprintf(result, pi - argv[0].buf + 1, "%s", argv[0].buf);
    sprintf(lres, "%s", pi+1);
    strcat(result, lres);

    return strlen(result);
}

/* ���ַ��� ���Զ�����Ϊ0 */
static int tp_INT2DOUB(int argc, TPVAR argv[], char *result, int size)
{
    int dotn;
    char lval[24];
    int len,llen;

    if (argc != 2) {
        LOG_ERR("������������ȷ[%d][2]",argc);
        return -1;
    }

    if (argv[1].len !=1) {
        LOG_ERR("����2���ȴ���[%d][1]",argv[1].len);
        return -1;
    }

    if (!isdigit(argv[1].buf[0])) {
        LOG_ERR("����2���ʹ���[%s][number]", argv[1].buf);
        return -1;
    }

    dotn = atoi(argv[1].buf);
    llen = dotn + 1 - argv[0].len;

    if (llen > 0)
        sprintf(lval, "%0*d%s", llen, 0, argv[0].buf);
    else
        sprintf(lval, "%s", argv[0].buf);

    len = strlen(lval);

    sprintf(result, "%*.*s.%s", len-dotn, len-dotn, lval, lval+len-dotn);
    return strlen(result);
}

/***************************************************************
 * ????��a??
 **************************************************************/
/*int encoding(const char *from, const char *to, char **buf)
{
    int i;
    iconv_t cd;
    xmlChar *out;
    int i_len,o_len;

    if (strlen(from) == strlen(to)) {
        for (i=0; to[i]; i++) if (toupper(from[i]) != toupper(to[i])) break; 
    }
    if (!to[i]) return 0;

    if ((cd = iconv_open(to, from)) == (iconv_t)-1) {
        LOG_ERR("iconv_open(%s->%s) error [%d]", from, to, errno);
        return -1;
    }

    i_len = strlen(*buf) + 1;
    o_len = i_len * 2;
    out = xmlMalloc(o_len);

    if (convert(cd, (unsigned char *)(*buf), &i_len, (unsigned char *)out, &o_len)) {
        xmlFree(out);
        return -1;
    }
    iconv_close(cd);
    xmlFree(*buf);
    *buf = (char *)out;
    return 0;
}
*/

/* �ַ�������ת�� from , to, buf*/
static int tp_ENCODING(int argc, TPVAR argv[], char *result, int size)
{
    char *codefrom, *codeto, *codebuf;
    char codeout[MAXMSGLEN];
    int ret = -1; 
    iconv_t cd;
    int i=0;
    int i_len,o_len;
    
    if (argc != 3) {
        LOG_ERR("������������ȷ[%d][3]",argc);
        return -1;
    }
    
    codefrom = argv[0].buf;
    codeto   = argv[1].buf;
    codebuf  = argv[2].buf;
    memset(codeout , 0x00, MAXMSGLEN);
    
    /* LOG_MSG("[%s][%s][%s][%s]",codefrom,codeto,codebuf,argv[3].buf); */
       
    if (strlen(codefrom) == strlen(codeto)) {
        for (i=0; codeto[i]; i++) if (toupper(codefrom[i]) != toupper(codeto[i])) break; 
    }
    if (!codeto[i]) return 0;

    if ((cd = iconv_open(codeto, codefrom)) == (iconv_t)-1) {
        LOG_ERR("iconv_open(%s->%s) error [%d]", codefrom, codeto, errno);
        return -1;
    }
    
    i_len = strlen(codebuf) + 1;
    o_len = i_len * 2;
    
    if (convert(cd, (unsigned char *)codebuf, &i_len, (unsigned char *)codeout, &o_len)) {
        return -1;
    }
    
    iconv_close(cd);
    /* LOG_MSG("[%s][%s][%s][%s]",codefrom,codeto,codebuf,codeout); */
    sprintf(result, "%s", codeout);
    return strlen(result);
}

/***************************************************************
 * convert
 **************************************************************/
static int convert(iconv_t cd, unsigned char *inbuf, int *inlen, unsigned char *outbuf, int *outlen)
{
    int ret;
    size_t icv_inlen = *inlen, icv_outlen = *outlen;
    const char *icv_inbuf = (const char *)inbuf;
    char *icv_outbuf = (char *)outbuf;

    ret = iconv(cd, (char **) &icv_inbuf, &icv_inlen, &icv_outbuf, &icv_outlen);
    if (inbuf != NULL) {
        *inlen -= icv_inlen;
        *outlen -= icv_outlen;
    } else {
        *inlen = 0;
        *outlen = 0;
    }
    if ((icv_inlen != 0) || (ret == -1)) return -1;
    return 0;
}

static int tp_ISSEQ( int argc, TPVAR argv[], char *result, int size)
{
    char stchr;
    int i;

    if (argc != 2) {
        LOG_ERR("������������ȷ");
        return -1;
    }
    if (argv[1].len != 1) {
        LOG_ERR("�������Ȳ���ȷ");
        return -2;
    }
    memset(result, 0x00, argv[0].len);

    stchr = argv[1].buf[0];
    for(i = 0; i<argv[0].len && argv[0].buf[i] == stchr; i++);

        /* �����ַ����������ַ�һ�� */
    result[0] = i==argv[0].len ? 1 : 0;

    return 1;
}


/** ת���������յ��ͻ���Ϣ��ʽ*/
static int tp_CUSTINFO2NETACQ(int argc, TPVAR argv[], char *result, int size)
{
    char custtomerinfo[512];
    char idtype[512];
    char idno[512];
    char idname[512];
    char mobile[512];
    char pwd[512];
        
    memset(custtomerinfo    , 0x20, sizeof(custtomerinfo));
    memset(idtype           , 0x00, sizeof(idtype));
    memset(idno             , 0x00, sizeof(idno));
    memset(idname           , 0x00, sizeof(idname));
    memset(mobile           , 0x00, sizeof(mobile));
    memset(pwd              , 0x00, sizeof(pwd));
    
    switch(atoi(argv[1].buf))
    {
        case 1:
        case 2:
            memcpy(idtype   ,"01",   2);
            break;
        case 3:
            memcpy(idtype   ,"06",   2);
            break;
        case 4:
            memcpy(idtype   ,"05",   2);
            break;
        case 5:
        case 10:
            memcpy(idtype   ,"03",   2);
            break;
        default:
            memcpy(idtype   ,"99",   2);
            break;
    }
    memcpy(idno     ,argv[2].buf    ,   argv[2].len);
    memcpy(idname   ,argv[3].buf    ,   argv[3].len);
    memcpy(mobile   ,argv[4].buf    ,   argv[4].len);
    memcpy(pwd      ,argv[5].buf    ,   argv[5].len);
    
    
    
    switch(atoi(argv[0].buf))
    {   
        case 5:     /*card + idtype + idno + name + mobile */
            sprintf(custtomerinfo, "%-2.2s|%-20.20s|%-32.32s|%-20.20s|%-6.6s|%-256.256s|%-3.3s|%-4.4s", idtype,strTrim(idno),strTrim(idname),strTrim(mobile)," "," "," "," "  );
        case 6:     /*card + idtype + idno + name + mobile + pwd */
            sprintf(custtomerinfo, "%-2.2s|%-20.20s|%-32.32s|%-20.20s|%-6.6s|%-256.256s|%-3.3s|%-4.4s", idtype,strTrim(idno),strTrim(idname),strTrim(mobile)," ",strTrim(pwd)," "," "  );
        default:    /*card + idtype + idno + name*/
            sprintf(custtomerinfo, "%-2.2s|%-20.20s|%-32.32s|%-20.20s|%-6.6s|%-256.256s|%-3.3s|%-4.4s", idtype,strTrim(idno),strTrim(idname)," "," "," "," "," "  );
            break;
    }
    
    custtomerinfo[sizeof(custtomerinfo)-1] ='\0';
    sprintf(result, "%s", custtomerinfo);
    return strlen(result);
}

/** ת��ʵ����֤�������ÿͻ���Ϣ��ʽ*/
static int tp_INFO(int argc, TPVAR argv[], char *result, int size)
{
    char custtomerinfo[512];
    char idtype[512];
    char idno[512];
    char idname[512];
    char mobile[512];
    char pwd[512];
        
    memset(custtomerinfo    , 0x20, sizeof(custtomerinfo));
    memset(idtype           , 0x00, sizeof(idtype));
    memset(idno             , 0x00, sizeof(idno));
    memset(idname           , 0x00, sizeof(idname));
    memset(mobile           , 0x00, sizeof(mobile));
    memset(pwd              , 0x00, sizeof(pwd));
    
    switch(atoi(argv[1].buf))
    {
        case 1:
        case 2:
            memcpy(idtype   ,"01",   2);
            break;
        case 3:
            memcpy(idtype   ,"06",   2);
            break;
        case 4:
            memcpy(idtype   ,"05",   2);
            break;
        case 5:
        case 10:
            memcpy(idtype   ,"03",   2);
            break;
        default:
            memcpy(idtype   ,"99",   2);
            break;
    }
    memcpy(idno     ,argv[2].buf    ,   argv[2].len);
    memcpy(idname   ,argv[3].buf    ,   argv[3].len);
    memcpy(mobile   ,argv[4].buf    ,   argv[4].len);
    memcpy(pwd      ,argv[5].buf    ,   argv[5].len);
    
    LOG_MSG("tp_INFO[%s][%s][%s][%s]", idtype,strTrim(idno),strTrim(idname),strTrim(mobile) );
    if(argv[0].buf[0]=='N')	
    	memset(mobile, 0x00, sizeof(mobile));

    if(argv[0].buf[1]=='N')	
    	memset(idtype, 0x00, sizeof(idtype));

    if(argv[0].buf[2]=='N')	
    	memset(idno, 0x00, sizeof(idno));

    if(argv[0].buf[3]=='N')	
    	memset(idname, 0x00, sizeof(idname)); 
    	   	    	    	
	sprintf(custtomerinfo, "%-2.2s|%-20.20s|%-32.32s|%-20.20s|%-6.6s|%-256.256s|%-3.3s|%-4.4s", idtype,strTrim(idno),strTrim(idname),strTrim(mobile)," "," "," "," "  );

    LOG_MSG("custtomerinfo=[%s]", custtomerinfo);
    
    custtomerinfo[sizeof(custtomerinfo)-1] ='\0';
    sprintf(result, "%s", custtomerinfo);
    return strlen(result);
}

/* ��ȡϵͳʱ��YYYYMMDDhhmmssXX */
static int tp_SYSDT(int argc, TPVAR argv[], char *result, int size)
{
    time_t  ttime;
    struct  tm       *timenow;
    struct  timeval  tval;
    char    buftime[32];
    
    memset(buftime   , 0x00, sizeof(buftime));
    time(&ttime);
    timenow = localtime(&ttime);
    
    gettimeofday(&tval, NULL);
    
    if (argc <= 1)                                               /*"YYYYMMDDhhmmssXX"*/
    {   
        sprintf(buftime,"%04d%02d%02d%02d%02d%02d%d",timenow->tm_year+1900,timenow->tm_mon+1,timenow->tm_mday,timenow->tm_hour,timenow->tm_min,timenow->tm_sec,tval.tv_usec);
        memset(buftime+16   , 0x00, sizeof(buftime)-16);
    }
    else 
    {
        if ( 0 == strcmp(argv[0].buf, "YYYYMMDDhhmmss"))       /*"YYYYMMDDhhmmss"*/
        {   sprintf(buftime,"%04d%02d%02d%02d%02d%02d",timenow->tm_year+1900,timenow->tm_mon+1,timenow->tm_mday,timenow->tm_hour,timenow->tm_min,timenow->tm_sec);}
        else if ( 0 == strcmp(argv[0].buf, "YYYYMMDDhhmmssXX"))       /*"YYYYMMDDhhmmss"*/
        {   
            sprintf(buftime,"%04d%02d%02d%02d%02d%02d%d",timenow->tm_year+1900,timenow->tm_mon+1,timenow->tm_mday,timenow->tm_hour,timenow->tm_min,timenow->tm_sec,tval.tv_usec);
            buftime[16]=0x00;
        }
        if( 0 == strcmp(argv[0].buf, "YYYYMMDD"))                   /*"YYYYMMDD"*/
        {   sprintf(buftime,"%04d%02d%02d",timenow->tm_year+1900,timenow->tm_mon+1,timenow->tm_mday);}
        else if ( 0 == strcmp(argv[0].buf, "hhmmss"))               /*"hhmmss"*/
        {   sprintf(buftime,"%02d%02d%02d",timenow->tm_hour,timenow->tm_min,timenow->tm_sec);}
        else                                                        /*"YYYYMMDDhhmmssXXXXXX"*/
        {   sprintf(buftime,"%04d%02d%02d%02d%02d%02d%d",timenow->tm_year+1900,timenow->tm_mon+1,timenow->tm_mday,timenow->tm_hour,timenow->tm_min,timenow->tm_sec,tval.tv_usec);}
    }   
    sprintf(result, "%s", buftime);    
    
    return strlen(result);
    
}

/* ���󶨹�ϵ�Ƿ���� */
/**
*@param1 retcode
*@param2 card_in
*@param3 card_bind_num
*@param4 card_bind_1
*@param5 card_bind_2
*.......
*/
static int tp_CHKCARDRELATION(int argc, TPVAR argv[], char *result, int size)
{
    char    buf[128];
    char    card_bind[128];
    int     card_bind_num;
    int     i,flag_find;
    
    if (argc < 4)                                              
    {   
        LOG_ERR("������������ȷ");
        LOG_ERR("@param1 retcode, @param2 card_in, @param3 card_bind_num, @param4 card_bind_1, ...");
        sprintf(result, "%s", "CE999999");
        return strlen(result);
    }

    /*����ѯӦ����*/
    memset(buf  ,   0x00            ,   sizeof(buf));
    memcpy(buf  ,   argv[0].buf     ,   argv[0].len);
    strTrim(buf);
    if(memcmp(buf,   "00000000"     ,   strlen(buf)) != 0)
    {
        LOG_MSG("chk card relationship query ret[%s], refuse transaction", buf);
        sprintf(result, "CA%-.6s",buf);
        return strlen(result);
    }
    
    /*��鷵�ذ󶨹�ϵ��Ŀ*/
    card_bind_num = atoi(argv[2].buf);
    if(card_bind_num <=0 || card_bind_num >10)
    {
        LOG_MSG("chk card relationship query num[%d] not in 1~10, refuse transaction", card_bind_num);
        sprintf(result, "%s", "CE999021");
        return strlen(result);
    }    
    
    /*���󶨹�ϵ*/
    i=0;
    flag_find = 0;
    memset(buf  ,   0x00            ,   sizeof(buf));
    memcpy(buf  ,   argv[1].buf     ,   argv[1].len);
    strTrim(buf);
    while(card_bind_num > 0)
    {
        memset(card_bind  ,   0x00            ,   sizeof(card_bind));
        memcpy(card_bind  ,   argv[3+i].buf   ,   argv[3+i].len);
        strTrim(card_bind);
        if(strcmp(buf, card_bind) == 0)
        {
            flag_find = 1;
            break;
        }    

        i++;
        card_bind_num--;
    }
    
    LOG_MSG("chk card relationship [0-N][1-Y] res[%d]", flag_find);
    
    if(flag_find == 0)  /*not find */
    {
        sprintf(result, "%s", "CE999021");
        return strlen(result);
    }
    else                /*    find */
    {
        sprintf(result, "%s", "1");
        return strlen(result);
    }    
    
}

/**
*@param1 strbuf
*@param2 char to del from strbuf left
*/
static int tp_TRIMLCHR(int argc, TPVAR argv[], char *result, int size)
{
    int i,l;
    char del;
    
    if (argc != 2)                                              
    {   
        LOG_ERR("������������ȷ");
        LOG_ERR("[%d]@param1 strbuf, @param2 char ", argc);
        return -1;
    }
    
    LOG_MSG("TRIMLCHR:param1[%d][%s]",argv[0].len,argv[0].buf);
    LOG_MSG("TRIMLCHR:param2[%d][%s]",argv[1].len,argv[1].buf);
    
    if(argv[1].len != 1)
    {
        LOG_ERR("������ʽ����ȷ");
        LOG_ERR("@param2 [%s]" , argv[1].buf);
        return -1;
    }
    
    del = argv[1].buf[0];
    
    for(l =0; l< argv[0].len; l++)
    {
        if(argv[0].buf[l] != del)
            break;
    }
    if(l == argv[0].len)    result[0] = '\0';
    
    for(i=l; i<argv[0].len; i++)
        result[i-l] = argv[0].buf[i];
    
    result[i-l] = '\0';
    
    LOG_MSG("TRIMLCHR:result[%s]",result);
    
    return strlen(result);   
}

/* ʱ��YYYYMMDDhhmmssת��Ϊʱ��� */
static int tp_MKTIME(int argc, TPVAR argv[], char *result , int size)
{
    struct tm t_buf;
    char year[4+1];
    char mon[2+1];
    char day[2+1];
    char hour[2+1];
    char min[2+1];
    char sec[2+1];   
    memset(&t_buf, 0x00, sizeof(struct tm));

	if (argc != 1) 
	{
        LOG_ERR("������������ȷ");
        return -1;
    }
    
    if(argv[0].len != 14 )
   	{
   		LOG_ERR("���ڸ�ʽ����ȷ��YYYYMMDDhhmmss");
        return -1;
   	}
   
    strncpy(year,argv[0].buf,4);
	strncpy(mon,argv[0].buf+4,2);
	strncpy(day,argv[0].buf+6,2);
	strncpy(hour,argv[0].buf+8,2);
	strncpy(min,argv[0].buf+10,2);
	strncpy(sec,argv[0].buf+12,2);

    t_buf.tm_year = atoi(year) - 1900;
    t_buf.tm_mon  = atoi(mon) - 1;
    t_buf.tm_mday = atoi(day);
    t_buf.tm_hour = atoi(hour);
    t_buf.tm_min  = atoi(min);
    t_buf.tm_sec  = atoi(sec);

	sprintf(result, "%ld", mktime(&t_buf));
		
    return strlen(result);
    
}


/* ���֤��15λת18λ */
static int tp_SETIDNO15_18(int argc, TPVAR argv[], char *result , int size)
{
    char sIdtype[128],sIdno[256],sIdno18[18+1];
    const long weight[17]={7,9,10,5,8,4,2,1,6,3,7,9,10,5,8,4,2};
    const char pEnd[12]="10X98765432";
    long data[17];
    long k,nyear=0,nsum=0,tail=0;

	if (argc != 2) 
	{
        LOG_ERR("������������ȷ");
        return -1;
    }
    
    LOG_MSG("���֤��15λת18λIDTYPE[%s],IDNO[%s],RESULT[%s]",argv[0].buf,argv[1].buf,result);
    /*֤������*/
    memset(sIdtype,0x00,sizeof(sIdtype));
    strcpy(sIdtype, argv[0].buf);
    strTrim(sIdtype);
     
    /*֤������*/
    memset(sIdno,0x00,sizeof(sIdno));
    strcpy(sIdno,   argv[1].buf);
    strTrim(sIdno);

    /*ֻ��֤��������01-���֤���Һ�����15λʱ�Ż���ת��*/
    if(memcmp(sIdtype,"01",2)==0 && strlen(sIdno)==15)
    {
    	for(k=0;k<6;k++)
    	{
    		data[k]=sIdno[k]-'0';
    	}
    	/*������ݼ�������*/
    	nyear=(sIdno[6]-'0')*10+(sIdno[7]-'0');
    	if(nyear>9)
    	{
    		data[6]=1;
    		data[7]=9;
    	}
    	else
    	{
    		data[6]=2;
    		data[7]=0;
    	}
    	for(k=8;k<17;k++)
    	{
    		data[k]=sIdno[k-2]-'0';
    	}
    	for(k=0;k<17;k++)
    	{
    		nsum+=data[k]*weight[k];
    	}
    	tail=nsum%11;
    	memset(sIdno18,0x00,sizeof(sIdno18));
    	for(k=0;k<17;k++)
    	{
    		sIdno18[k]=data[k]+'0';
    		sIdno18[17]=pEnd[tail];
    		sIdno18[18]='\0';
    	}
        sprintf(result, "%s", sIdno18);
    }
    else{
        sprintf(result, "%s", sIdno);
    }
	LOG_MSG("���֤��15λת18λIDTYPE[%s],IDNO[%s],RESULT[%s]",argv[0].buf,argv[1].buf,result); 
    return strlen(result);    
}



static int tp_CHECKAMOUNT( int argc, TPVAR argv[], char *result, int size)
{
    char stchr;
    int i;

    if (argc != 1) {
        LOG_ERR("������������ȷ");
        return -1;
    }
 
    for(i = 0; i<argv[0].len; i++)
    {
        if(argv[0].buf[i] <= '9' && argv[0].buf[i] >='0')
        {
            result[0] = 1;
        }
        else
        {
            result[0] = 0;
            return 1;
        } 
    }
    
    return 1;
}

/***************************************************************
 * ����mac������
 * 0-suc, -1 - fail
 * mac_elem - mac������
 * mac_elem_ret - ������mac������
 **************************************************************/
int combmac( char *mac_elem, char * mac_elem_ret8,char * mac_elem_ret16,char * mac_elem_ret32 )
{
	int 	macBufLen;
	int 	macelemLen;
	int     vecsize;
	char    mac_buf_16[16];
    char    mac_buf_32[32];


	macelemLen = strlen(mac_elem);  /*�����в�Ӧ�ú��� /0 �ַ�  ����ú�������*/
	
	if((macelemLen % 8) == 0)
		vecsize = macelemLen/8;
	else
		vecsize = macelemLen / 8 + 1;
    LOG_DBG("combmac: input      = [%s]",mac_elem); 
	LOG_DBG("combmac: macelemLen = [%d]",macelemLen); 
	LOG_DBG("combmac: vecsize    = [%d]",vecsize); 

	char (*mac_elemvec)[9] = (char(*)[9])malloc(sizeof(int) * 9 * vecsize);
	
	memset(mac_elemvec,0x00,sizeof(int) * 9 *vecsize);

	int len = 0;
	int maclen = macelemLen;
	int i;
	
	for(i = 0; i < vecsize; i++)
	{
		if(maclen >= 8)
		{
			memcpy(mac_elemvec[i], mac_elem + len, 8);
			len += 8;
		}
		else
			memcpy(mac_elemvec[i], mac_elem + len, maclen);
		maclen -= 8;
        
        LOG_DBG("combmac: mac_elemvec[%d]    = [%s]",i,mac_elemvec[i]); 
	}
    
    
    
	for(i = 0; i < (vecsize - 1); i++)
	{
		mac_elemvec[i + 1][0] ^= mac_elemvec[i][0];
		mac_elemvec[i + 1][1] ^= mac_elemvec[i][1];
		mac_elemvec[i + 1][2] ^= mac_elemvec[i][2];
		mac_elemvec[i + 1][3] ^= mac_elemvec[i][3];
		mac_elemvec[i + 1][4] ^= mac_elemvec[i][4];
		mac_elemvec[i + 1][5] ^= mac_elemvec[i][5];
		mac_elemvec[i + 1][6] ^= mac_elemvec[i][6];
		mac_elemvec[i + 1][7] ^= mac_elemvec[i][7];
	}
    memcpy(mac_elem_ret8,mac_elemvec[vecsize - 1],8); 
	LOG_DBG("combmac: mac_elem_ret8    = [%s]",mac_elemvec[vecsize - 1]);
    
	Hex2Str(mac_elemvec[vecsize - 1], mac_buf_16, 8);
	macBufLen = sizeof(mac_buf_16);
    memcpy(mac_elem_ret16,mac_buf_16,macBufLen); 
    LOG_DBG("combmac: mac_elem_ret16   = [%s]",mac_buf_16);
    
    Hex2Str(mac_buf_16, mac_buf_32, macBufLen);
	macBufLen = sizeof(mac_buf_32);
    memcpy(mac_elem_ret32,mac_buf_32,macBufLen); 
    LOG_DBG("combmac: mac_elem_ret32   = [%s]",mac_buf_32);
   
    return 0;
}

int hsmcall(char *host,int port,char* buff)
{
    int sockfd;
    int in_len, com_len, i;
    unsigned char com_buff[4*1024 + 1];
    char len_str[5];

    if ((sockfd = tcp_connect("0.0.0.0",host, port, 0)) == -1) {
        LOG_ERR("tcp_connect() error");
        return -1;
    }
 
    if (set_sock_linger(sockfd, 1, 1) == -1) {
        LOG_ERR("set_sock_linger() error");
        close(sockfd);
        return -1;
    }

    /*send request string*/
    for (i = 0; i < 4; i++)
        len_str[i] = buff[i];
    len_str[i] = '\0';

    com_len = atoi (len_str);
    com_buff[0] = com_len / 256;
    com_buff[1] = com_len % 256;

    for (i = 0; i < com_len; i++)
        com_buff[i + 2] = buff[i +4];
    com_buff[i+2] = '\0';

    if (tcp_send(sockfd, com_buff, com_len + 2, 0, 0) == -1) {
        LOG_ERR("tcp_send() error");
        close(sockfd);
        return -1;
    }
    LOG_DBG("���ͼ��ܻ�[%2.2X%2.2X][%s]", com_buff[0], com_buff[1], com_buff+2);


    if ((com_len = tcp_recv(sockfd, com_buff, 0, 0, 0)) == -1) {
        LOG_ERR("tcp_recv() error: %s", strerror(errno));
        close(sockfd);
        return -1;
    }

    close(sockfd);
    
    /*write response data back*/
    com_len = com_buff[1] + com_buff[0] * 256;
    
    LOG_DBG("[%c]",com_buff);
    LOG_DBG("[%c]",com_buff+1);
    LOG_DBG("[%s]",com_buff+2);
    LOG_DBG("[%8.8s]",com_buff+2+12);
    memset(buff, 0x00, com_len + 4 + 1);
    sprintf(buff, "%4.4d", com_len);

    for (i = 0; i < com_len; i++)
        buff[i + 4] = com_buff[i + 2];

    return 0;
}


/*
HSMMAC($KEY_ZAK,$BUF_MAC)
*/
static int tp_CALHSMMAC(int argc, TPVAR argv[], char *result, int size)
{
    int     len_key_zak,ret,i;
    char    buf_mac08[8+1];
    char    buf_mac16[16+1];
    char    buf_mac32[32+1];
    char    buf_hsm[4*1024];
    char    buf_hsm_des[4*1024];
    char    *host,*key_zak, *macdata;
    int     port;
    
    ret             = 0;
    len_key_zak     = 0;
    memset(buf_mac32,     0x00, sizeof(buf_mac32));
    memset(buf_mac16,     0x00, sizeof(buf_mac16));
    memset(buf_mac08,     0x00, sizeof(buf_mac08));
    memset(buf_hsm,         0x00, sizeof(buf_hsm));
    memset(buf_hsm_des,     0x00, sizeof(buf_hsm_des));
    
    host    = argv[0].buf;
    port    = atoi(argv[1].buf);
    key_zak = argv[2].buf;
    macdata = argv[3].buf;
    
    strTrim(key_zak);
    len_key_zak = strlen(key_zak);
    /* У������Ϸ��� */
    if( key_zak ==NULL )
    {
        LOG_ERR("creat_mac: key_zak is null,erro");
        sprintf(result,"%6.6s%32.32s","999999"," ");
        result[6+32]='\0';
        return strlen(result);
    }
    strTrim(macdata);
    if(macdata ==NULL )
    {
        LOG_ERR("creat_mac: macdata is null,erro");
        sprintf(result,"%6.6s%32.32s","999999"," ");
        result[6+32]='\0';
        return strlen(result);
    }

    LOG_MSG("creat_mac: key_zak= [%d][%s]",strlen(key_zak),key_zak);
    LOG_MSG("creat_mac: macdata= [%d][%s]",strlen(macdata),macdata);
    
    combmac(macdata,buf_mac08,buf_mac16,buf_mac32);
    
    /* ����ܻ����� */
    /*
    for(i=0; i<16; i++)
    {
        if(buf_mac16[i] >='A' && buf_mac16[i] <='Z' )
            buf_mac16[i]+= 32; 
    }
    */
    memcpy(buf_hsm+4+0   ,"12345678",8);
    memcpy(buf_hsm+4+8   ,"MS0101",6);
    memcpy(buf_hsm+4+8+6   ,key_zak,len_key_zak);
    sprintf(buf_hsm+4+8+6+len_key_zak     ,"%04X",16/2);
    memcpy(buf_hsm+4+8+6+len_key_zak+4    ,buf_mac16,16);
    sprintf(buf_hsm_des,"%04d%s",strlen(buf_hsm+4),buf_hsm+4);
    LOG_MSG("creat_mac: before call hsm[buf_hsm_des]=[%s]",buf_hsm_des);
    
    ret = hsmcall(host,port,buf_hsm_des);
    if(ret != 0 || memcmp(buf_hsm_des+4+8+2,"00",2) != 0)
    {
        LOG_ERR("creat_mac: call hsm erro[%d]",ret);
        sprintf(result,"%6.6s%32.32s","999999"," ");
        result[6+32]='\0';
        return strlen(result);
    }
    
    memcpy(result,"000000",6);
    memcpy(result+6,buf_hsm_des+4+8+2+2,16);
    result[6+32]='\0';
    return strlen(result);

}
