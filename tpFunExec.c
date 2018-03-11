/***************************************************************
 * �ű�������
 **************************************************************/
#include <dlfcn.h>
#include "tpbus.h"
static int tp_CTX       (int argc, TPVAR argv[], char *result, int size);
static int tp_NVL       (int argc, TPVAR argv[], char *result, int size);
static int tp_OFFSET    (int argc, TPVAR argv[], char *result, int size);
static int tp_OFFMOV    (int argc, TPVAR argv[], char *result, int size);
static int tp_CSV       (int argc, TPVAR argv[], char *result, int size);
static int tp_TLV       (int argc, TPVAR argv[], char *result, int size);
static int tp_TLVSTR    (int argc, TPVAR argv[], char *result, int size);
static int tp_FML       (int argc, TPVAR argv[], char *result, int size);
static int tp_XML       (int argc, TPVAR argv[], char *result, int size);
static int tp_XMLNS     (int argc, TPVAR argv[], char *result, int size);
static int tp_CHRSET    (int argc, TPVAR argv[], char *result, int size);
static int tp_XMLENC    (int argc, TPVAR argv[], char *result, int size);
static int tp_XMLINIT   (int argc, TPVAR argv[], char *result, int size);
static int tp_XMLNODE   (int argc, TPVAR argv[], char *result, int size);
static int tp_XMLTEXT   (int argc, TPVAR argv[], char *result, int size);
static int tp_XMLPROP   (int argc, TPVAR argv[], char *result, int size);
static int tp_ISO       (int argc, TPVAR argv[], char *result, int size);
static int tp_HDR       (int argc, TPVAR argv[], char *result, int size);
static int tp_PREHDR_LEN(int argc, TPVAR argv[], char *result, int size);
static int tp_PREHDR_SET(int argc, TPVAR argv[], char *result, int size);
static int tp_CHR       (int argc, TPVAR argv[], char *result, int size);
static int tp_STR       (int argc, TPVAR argv[], char *result, int size);
static int tp_LEN       (int argc, TPVAR argv[], char *result, int size);
static int tp_TRIM      (int argc, TPVAR argv[], char *result, int size);
static int tp_TRIML     (int argc, TPVAR argv[], char *result, int size);
static int tp_TRIMR     (int argc, TPVAR argv[], char *result, int size);
static int tp_FILL      (int argc, TPVAR argv[], char *result, int size);
static int tp_CAT       (int argc, TPVAR argv[], char *result, int size);
static int tp_MID       (int argc, TPVAR argv[], char *result, int size);
static int tp_LEFT      (int argc, TPVAR argv[], char *result, int size);
static int tp_RIGHT     (int argc, TPVAR argv[], char *result, int size);
static int tp_AND       (int argc, TPVAR argv[], char *result, int size);
static int tp_OR        (int argc, TPVAR argv[], char *result, int size);
static int tp_NOT       (int argc, TPVAR argv[], char *result, int size);
static int tp_EQ        (int argc, TPVAR argv[], char *result, int size);
static int tp_SEQ       (int argc, TPVAR argv[], char *result, int size);
static int tp_GT        (int argc, TPVAR argv[], char *result, int size);
static int tp_GE        (int argc, TPVAR argv[], char *result, int size);
static int tp_LT        (int argc, TPVAR argv[], char *result, int size);
static int tp_LE        (int argc, TPVAR argv[], char *result, int size);
static int tp_ADD       (int argc, TPVAR argv[], char *result, int size);
static int tp_SUB       (int argc, TPVAR argv[], char *result, int size);
static int tp_MUL       (int argc, TPVAR argv[], char *result, int size);
static int tp_DIV       (int argc, TPVAR argv[], char *result, int size);
static int tp_IF        (int argc, TPVAR argv[], char *result, int size);
static int tp_REG       (int argc, TPVAR argv[], char *result, int size);
static int tp_DATETIME  (int argc, TPVAR argv[], char *result, int size);
static int tp_NULL      (int argc, TPVAR argv[], char *result, int size);
static int tp_KEYVAL_SET(int argc, TPVAR argv[], char *result, int size);


static int tp_XMLNODE_Noencoding(int argc, TPVAR argv[], char *result, int size); /* �½�XML�ڵ�     */
static int tp_XMLTEXT_Noencoding(int argc, TPVAR argv[], char *result, int size); /* ����XML�ڵ��ı� */
static int tp_XML_Noencoding(int argc, TPVAR argv[], char *result, int size);     /* ȡXML������     */
static int tp_XMLPROP_Noencoding(int argc, TPVAR argv[], char *result, int size); /* ����XML�ڵ�����     */
static int tp_XML_GET  (int argc, TPVAR argv[], char *result, int size);          /* ȡxmlѭ���ڵ��ĳ��ֵ*/
static int tp_XML_NGET (int argc, TPVAR argv[], char *result, int size);          /* ȡxmlѭ���ڵ��ĳ��ֵ Noencoding*/
static int tp_XML_GET_NODECOUNT(int argc, TPVAR argv[], char *result, int size);   /*��ȡXML�ڵ����*/


/* �ű��������� */
static int   m_size = 0;
static TPFUN m_list[512];

/* �ű������б� */
static TPFUN  TPFUN_LIST[] = {
       {"CTX",          tp_CTX        },  /* ȡ�������� */
       {"NVL",          tp_NVL        },  /* ȡ���������� */
       {"OFFSET",       tp_OFFSET     },  /* ����NVLƫ���� */
       {"OFFMOV",       tp_OFFMOV     },  /* �Ӽ�NVLƫ���� */
       {"CSV",          tp_CSV        },  /* ȡ�ָ��������� */
       {"TLV",          tp_TLV        },  /* ȡTLV������ */
       {"TLVSTR",       tp_TLVSTR     },  /* ����TLV��ʽ�� */
       {"FML",          tp_FML        },  /* ȡFML������ */
       {"XML",          tp_XML        },  /* ȡXML������ */
       {"XMLNS",        tp_XMLNS      },  /* ���������ռ� */
       {"CHRSET",       tp_CHRSET     },  /* ����XML�����ַ��� */
       {"XMLENC",       tp_XMLENC     },  /* ����XML�����ַ��� */
       {"XMLINIT",      tp_XMLINIT    },  /* ��ʼ��XML�ĵ� */
       {"XMLNODE",      tp_XMLNODE    },  /* �½�XML�ڵ� */
       {"XMLTEXT",      tp_XMLTEXT    },  /* ����XML�ڵ��ı� */
       {"XMLPROP",      tp_XMLPROP    },  /* ����XML�ڵ����� */
       {"ISO",          tp_ISO        },  /* ȡISO8583������ */
       {"HDR",          tp_HDR        },  /* ȡ����ͷ�� */
       {"PREHDR_LEN",   tp_PREHDR_LEN },  /* ����ǰ�ñ���ͷ���� */
       {"PREHDR_SET",   tp_PREHDR_SET },  /* ���ú��ñ����峤�� */
       {"CHR",          tp_CHR        },  /* ����һ��������ͬ�ַ���ɵĴ� */
       {"STR",          tp_STR        },  /* ����һ��������ͬ�ַ���ɵĴ� */
       {"LEN",          tp_LEN        },  /* ȡ����ֵ���� */
       {"TRIM",         tp_TRIM       },  /* ȥ�����ҿո� */
       {"TRIML",        tp_TRIML      },  /* ȥ����߿ո� */
       {"TRIMR",        tp_TRIMR      },  /* ȥ���ұ߿ո� */
       {"FILL",         tp_FILL       },  /* ����ַ��� */
       {"CAT",          tp_CAT        },  /* ���Ӷ���ַ��� */
       {"SAD",          tp_CAT        },  /* ���Ӷ���ַ���(������ǰ�÷�) */
       {"MID",          tp_MID        },  /* ȡ�Ӵ� */
       {"LEFT",         tp_LEFT       },  /* ȡ����Ӵ� */
       {"RIGHT",        tp_RIGHT      },  /* ȡ�ұ��Ӵ� */
       {"AND",          tp_AND        },  /* �߼��� */
       {"OR",           tp_OR         },  /* �߼��� */
       {"NOT",          tp_NOT        },  /* �߼��� */
       {"EQ",           tp_EQ         },  /* �Ƚ�������ֵ�Ƿ���� */
       {"SEQ",          tp_SEQ        },  /* �Ƚ������ַ����Ƿ���� */
       {"GT",           tp_GT         },  /* �ж��Ƿ���� */
       {"GE",           tp_GE         },  /* �ж��Ƿ���ڵ��� */
       {"LT",           tp_LT         },  /* �ж��Ƿ�С�� */
       {"LE",           tp_LE         },  /* �ж��Ƿ�С�ڵ��� */
       {"ADD",          tp_ADD        },  /* ������ֵ��� */
       {"SUB",          tp_SUB        },  /* ������ֵ��� */
       {"MUL",          tp_MUL        },  /* ������ֵ��� */
       {"MULTI",        tp_MUL        },  /* ������ֵ���(������ǰ�÷�) */
       {"DIV",          tp_DIV        },  /* ������ֵ��� */
       {"IF",           tp_IF         },  /* ������ֵ */
       {"REG",          tp_REG        },  /* ����ƥ�� */
       {"DATETIME",     tp_DATETIME   },  /* ȡϵͳ��ǰ���ں�ʱ�� */
       {"KEYVAL_SET",   tp_KEYVAL_SET },  /* �����첽KEYֵ */
       {"NULL",         tp_NULL       },  /* ���ؿ�ֵ */
       {"XMLNODE_N",    tp_XMLNODE_Noencoding  },  /* �½�XML�ڵ� */       
       {"XMLTEXT_N",    tp_XMLTEXT_Noencoding  },  /* ����XML�ڵ��ı� */
       {"XML_N",        tp_XML_Noencoding      },  /* ȡXML������ */
       {"XMLPROP_N",    tp_XMLPROP_Noencoding  },  /* ����XML�ڵ����� */    
       {"XML_GET",      tp_XML_GET             },  /* ȡxmlѭ���ڵ��ĳ��ֵ */
       {"XML_NGET",     tp_XML_NGET            },  /* ȡxmlѭ���ڵ��ĳ��ֵNoencoding */
       {"XML_GET_NODECOUNT",     tp_XML_GET_NODECOUNT     },  /* ��ȡxml�ڵ���� */
       {"",             NULL          }   /* END! */
};

static pthread_once_t once_ctl = PTHREAD_ONCE_INIT;

/***************************************************************
 * װ����չ�ű�������
 **************************************************************/
static TPFUN *loadExtra(char *file)
{
    void *hdl;
    char *err,pathFile[256];
    TPFUN *(*ptr)();

    snprintf(pathFile, sizeof(pathFile), "%s/lib/%s", getenv("TPBUS_HOME"), file);

    if (!(hdl = dlopen(pathFile, RTLD_LAZY))) {
        LOG_ERR("dlopen(%s) error: %s", pathFile, dlerror());
        return NULL;
    }
    dlerror();

    *(void **)(&ptr) = dlsym(hdl, "tpCustomInit");
    if ((err = dlerror()) != NULL) {
        LOG_ERR("dlsym(%s) error: %s", "tpCustomInit", err);
        dlclose(hdl);
        return NULL;
    }
    return (*ptr)();
}

/***************************************************************
 * ��ʼ���ű������б�
 **************************************************************/
static void loadAndInit()
{
    int flag,size;
    char *p,*ch,buf[256];
    TPFUN *list;

    /*װ�ر�׼�ű�������*/
    for (size=0; TPFUN_LIST[size].fn[0]; size++) {}
    memcpy(m_list, TPFUN_LIST, sizeof(TPFUN)*size);
    m_size = size;

    /*װ����չ�ű�������*/
    if ((p = getenv("TPBUS_EXTRA")) != NULL) {
        ch = p;
        flag = 0;
        while(1) {
            switch (*ch) {
                case ':':
                case '\0':
                    memcpy(buf, p, ch-p);
                    buf[ch-p] = 0;
                    if (!(list = loadExtra(strTrim(buf)))) {
                        LOG_ERR("װ����չ�ű�������[%s]����", p);
                        if (*ch) {
                            p = ++ch;
                        } else {
                            flag = 1;
                        }
                        break;
                    }
                    for (size=0; list[size].fn[0]; size++) {}
                    memcpy(m_list+m_size, list, sizeof(TPFUN)*size);
                    m_size += size;
                    if (*ch) {
                        p = ++ch;
                    } else {
                        flag = 1;
                    }
                    break;
                default:
                    ch++;
                    break;
            }
            if (flag) break;
        }
    }
}

/***************************************************************
 * ���ؽű������б�
 **************************************************************/
int tpFunGetList(TPFUN **ppList)
{
    loadAndInit();
    *ppList = m_list;
    return m_size;
}

/***************************************************************
 * ���ر��ʽ������
 **************************************************************/
int tpFunExec(char *express, char *result, int size)
{
    int i,j,k,l,flag=0;
    int argc,len,tmpLen;
    char *p,*ch;
    char tmpBuf[MAXVALLEN+1];

    TPFUN *ptpFun;

#define NMAXARGS  10
    TPVAR argv[NMAXARGS];

#define STKSIZE  100
    struct {
        char  buf[MAXVALLEN+1];
        short len;
        char  flg;
    } stk[STKSIZE];

    l = 0;
    i = -1;
    p = ch = express;

    pthread_once(&once_ctl, loadAndInit);

    while (*ch) {
        switch (*ch) {
        case '(':
        case ',':
            if (flag) {
                flag = 0;
                l = 0;
                p = ++ch;
                break;
            }

            if (++i >= STKSIZE) {
                LOG_ERR("���ʽ����");
                return -1;
            }
            stk[i].len = memTrimCopy(stk[i].buf, p, l);
            stk[i].flg = (*ch==',')? 0:1;
            l = 0;
            p = ++ch;
            break;
        case ')':
            if (++i >= STKSIZE) {
                LOG_ERR("���ʽ����");
                return -1;
            }
            stk[i].len = memTrimCopy(stk[i].buf, p, l);
            stk[i].flg = 0;
            if (stk[i].len <= 0) i--;
            l = 0;
            p = ++ch;
                
            for (j=i; j>=0; j--) { if (stk[j].flg) break; }
            if (j<0) {
                LOG_ERR("���ʽ�﷨����");
                return -1;
            }
            stk[j].buf[stk[j].len] = 0;
            
            argc = i-j;
            for (k=1; k<=argc; k++) {
                argv[k-1].len = stk[j+k].len;
                memcpy(argv[k-1].buf, stk[j+k].buf, argv[k-1].len);
                argv[k-1].buf[argv[k-1].len] = 0;
            }
            if (argc < NMAXARGS) {
                argv[argc].len = 0;
                argv[argc].buf[0] = 0;
            }

            if (stk[j].len > 0){
                ptpFun = &m_list[atoi(stk[j].buf)];
                if ((tmpLen = (*ptpFun->fp)(argc, argv, tmpBuf, MAXVALLEN)) < 0)
                {
                    LOG_ERR("ִ�нű�����[%s]����", ptpFun->fn);
                    return -1;
                }
                flag = 1;
                memcpy(stk[j].buf, tmpBuf, tmpLen);
                stk[j].len = tmpLen;
            } else {
                if (argc != 1) {
                    LOG_ERR("���ʽ�﷨����");
                    return -1;
                }
                memcpy(stk[j].buf, argv[0].buf, argv[0].len);
                stk[j].len = argv[0].len;
            }
            stk[j].flg = 0;
            i = j;
            break;
        default :
            l++;
            ch++;
            break;
        }        
    }
    if (i<0) {
        if (size < l+1) l = size-1;
        len = memTrimCopy(result, p, l);
        result[len] = 0;
        return len;
    }

    if (i>0) {
        LOG_ERR("���ʽ�﷨����");
        return -1;
    }
    if (stk[0].flg) {
        LOG_ERR("���ʽ�﷨����");
        return -1;
    }
    len = (size < stk[0].len+1)? size-1 : stk[0].len;
    memcpy(result, stk[0].buf, len);
    result[len] = 0;
    return len;
}

/***************************************************************
 * �����������߳�����
 **************************************************************/
int tpCtxSetTSD(TPCTX *pCtx)
{
    if (pthread_setspecific(TSD_CTX_KEY, pCtx) != 0) {
        LOG_ERR("pthread_setspecific() error: %s", strerror(errno));
        return -1;
    }
    return 0;
} 

/***************************************************************
 * ��ȡ�������߳�����
 **************************************************************/
TPCTX *tpCtxGetTSD()
{
    void *data;
  
    if ((data = pthread_getspecific(TSD_CTX_KEY)) == NULL) {
        LOG_ERR("pthread_getspecific() error: %s", strerror(errno));
        return NULL;
    }
    return (TPCTX *)data;
}

/***************************************************************
 * ȡ��������
 **************************************************************/
static int tp_CTX(int argc, TPVAR argv[], char *result, int size)
{
    int i,len;
    char key[MAXKEYLEN+1];
    TPCTX *pCtx;

    /*argv[0]: ����*/
    /*argv[1]: ��ID(��ѡ)*/
    
    if (argc < 1 || argc > 2) {
        LOG_ERR("������������ȷ");
        return -1;
    }
    
    pCtx = tpCtxGetTSD();
    if (!pCtx || !pCtx->msgCtx) {
        LOG_ERR("�����Ĳ���ȷ");
        return -1;
    }

    if (argc == 1) {
        if ((len = tpMsgCtxHashGet(pCtx->msgCtx, argv[0].buf, result, size)) < 0) {
            LOG_ERR("tpMsgCtxHashGet(%s) error", argv[0].buf);
            return -1;
        }
    } else {
        i = atoi(argv[1].buf);
        if (i < 0 || i > 999) {
            LOG_ERR("��2����������ȷ");
            return -1;
        }
        snprintf(key, MAXKEYLEN+1, "%s#%d", argv[0].buf, i);
        if ((len = tpMsgCtxHashGet(pCtx->msgCtx, key, result, size)) < 0) {
            LOG_ERR("tpMsgCtxHashGet(%s) error", key);
            return -1;
        }
    }
    return len;
}

/***************************************************************
 * ȡ����������
 **************************************************************/
static int tp_NVL(int argc, TPVAR argv[], char *result, int size)
{
    TPCTX *pCtx;
    int len,offset;

    pCtx = tpCtxGetTSD();
    if (!pCtx) {
        LOG_ERR("�����Ĳ���ȷ");
        return -1;
    }

    switch (argc) {
        case 1: /*argv[0]: ֵ����*/
            len = atoi(argv[0].buf);
            if (len <= 0) return 0;
            if (len > size) len = size;
            if (len > pCtx->tpMsg.head.bodyLen-pCtx->preBuf.offset) {
                len = pCtx->tpMsg.head.bodyLen-pCtx->preBuf.offset;
                if (len <= 0) return 0;
            }
            memcpy(result, pCtx->tpMsg.body + pCtx->preBuf.offset, len);
            pCtx->preBuf.offset += len;
            break;
        case 2: /*argv[0]: ƫ����, argv[1]: ֵ����*/ 
            len = atoi(argv[1].buf);
            if (len <= 0) return 0;
            offset = atoi(argv[0].buf);
            if (len > size) len = size;
            if (len > pCtx->tpMsg.head.bodyLen-offset) {
                len = pCtx->tpMsg.head.bodyLen-offset;
                if (len <= 0) return 0;
            }
            memcpy(result, pCtx->tpMsg.body + offset, len);
            break;
        default:
            LOG_ERR("������������ȷ");
            return -1;
    }
    return len;
}

/***************************************************************
 * ����NVLƫ����
 **************************************************************/
static int tp_OFFSET(int argc, TPVAR argv[], char *result, int size)
{
    TPCTX *pCtx;

    if (argc != 1) {
        LOG_ERR("������������ȷ");
        return -1;
    }

    pCtx = tpCtxGetTSD();
    if (!pCtx) {
        LOG_ERR("�����Ĳ���ȷ");
        return -1;
    }

    pCtx->preBuf.offset = atoi(argv[0].buf);
    if (pCtx->preBuf.offset < 0 ) pCtx->preBuf.offset = 0;
    return 0;
}

/***************************************************************
 * �Ӽ�NVLƫ����
 **************************************************************/
static int tp_OFFMOV(int argc, TPVAR argv[], char *result, int size)
{
    TPCTX *pCtx;

    if (argc != 1) {
        LOG_ERR("������������ȷ");
        return -1;
    }

    pCtx = tpCtxGetTSD();
    if (!pCtx) {
        LOG_ERR("�����Ĳ���ȷ");
        return -1;
    }

    pCtx->preBuf.offset += atoi(argv[0].buf);
    if (pCtx->preBuf.offset < 0 ) pCtx->preBuf.offset = 0;
    return 0;
}

/***************************************************************
 * ȡ�ָ���������
 **************************************************************/
static int tp_CSV(int argc, TPVAR argv[], char *result, int size)
{
    int fId,rId,gId,len;
    char key[MAXKEYLEN+1];
    TPCTX *pCtx;

    switch (argc) {
        case 1: /* argv[0]: ��ID */
        case 2: /* argv[0]: ��ID, argv[1]: ��ID */
        case 3: /* argv[0]: ��ID, argv[1]: ��ID, argv[2]: ��ID*/
            pCtx = tpCtxGetTSD();
            if (!pCtx || !pCtx->preBuf.ptr) {
                LOG_ERR("�����Ĳ���ȷ");
                return -1;
            }
            fId = rId = gId = 0;
            if (argc < 2) {
                fId = atoi(argv[0].buf);
            } else if (argc < 3) {
                rId = atoi(argv[0].buf);
                fId = atoi(argv[1].buf);
            } else {
                gId = atoi(argv[0].buf);
                rId = atoi(argv[1].buf);
                fId = atoi(argv[2].buf);
            }
            snprintf(key, sizeof(key), "%d.%d.%d", gId, rId, fId);
            if ((len = tpMsgCtxHashGet(pCtx->preBuf.ptr, key, result, size)) < 0) {
                LOG_ERR("tpMsgCtxHashGet(%s) error", key);
                return -1;
            }
            return len;
            break;
        default:
            LOG_ERR("������������ȷ");
            return -1;
    }
    return -1;
}

/***************************************************************
 * ȡTLV������
 **************************************************************/
static int tp_TLV(int argc, TPVAR argv[], char *result, int size)
{
    TPCTX *pCtx;
    int   tlvLen;
    char *tlvTag,*tlvBuf;

    switch (argc) {
        case 1: /* argv[0]: TAG*/
            if (!(pCtx = tpCtxGetTSD())) {
                LOG_ERR("�����Ĳ���ȷ");
                return -1;
            }
            tlvTag = argv[0].buf;
            tlvBuf = pCtx->tpMsg.body+pCtx->tpMsg.head.bodyOffset;
            tlvLen = pCtx->tpMsg.head.bodyLen;
            break;
        case 2: /* argv[0]: TLV, argv[1]: TAG*/
            tlvBuf = argv[0].buf;
            tlvLen = argv[0].len;
            tlvTag = argv[1].buf;
            break;
        default:
            LOG_ERR("������������ȷ");
            return -1;
    }
    return tlvGet(tlvBuf, tlvLen, tlvTag, result, size);
}

/***************************************************************
 * ����TLV��ʽ��
 **************************************************************/
static int tp_TLVSTR(int argc, TPVAR argv[], char *result, int size)
{
    /* argv[0]: TAG*/
    /* argv[1]: VALUE */

    if (argc != 2) {
        LOG_ERR("������������ȷ");
        return -1;
    }
    return tlvCat(result, size, argv[0].buf, argv[1].buf, argv[1].len);
}

/***************************************************************
 * ȡFML������
 **************************************************************/
static int tp_FML(int argc, TPVAR argv[], char *result, int size)
{
    int i,len;
    char key[MAXKEYLEN+1];
    TPCTX  *pCtx;
    MSGCTX *pMsgCtx;

    /*argv[0]: ����*/
    /*argv[1]: ��ID(��ѡ)*/

    if (argc < 1 || argc > 2) {
        LOG_ERR("������������ȷ");
        return -1;
    }
    
    pCtx = tpCtxGetTSD();
    if (!pCtx || !pCtx->preBuf.ptr) {
        LOG_ERR("�����Ĳ���ȷ");
        return -1;
    }
    pMsgCtx = (MSGCTX *)pCtx->preBuf.ptr;

    if (argc == 1) {
        if ((len = tpMsgCtxHashGet(pMsgCtx, argv[0].buf, result, size)) < 0) {
            LOG_ERR("tpMsgCtxHashGet(%s) error", argv[0].buf);
            return -1;
        }
    } else {
        i = atoi(argv[1].buf);
        if (i < 0 || i > 999) {
            LOG_ERR("��2����������ȷ");
            return -1;
        }
        snprintf(key, MAXKEYLEN+1, "%s#%d", argv[0].buf, i);
        if ((len = tpMsgCtxHashGet(pMsgCtx, key, result, size)) < 0) {
            LOG_ERR("tpMsgCtxHashGet(%s) error", key);
            return -1;
        }
    }
    return len;
}

/***************************************************************
 * ȡXML������
 **************************************************************/
static int tp_XML(int argc, TPVAR argv[], char *result, int size)
{
    TPCTX *pCtx;
    XMLDOC *pDoc;
    
    result[0] = 0x00;

    /* argv[0]: �ڵ�·�� */
    /* argv[1]: ��������(��ѡ) */

    if (argc < 1) {
        LOG_ERR("������������ȷ");
        return -1;
    }

    pCtx = tpCtxGetTSD();
    if (!pCtx || !pCtx->preBuf.ptr) {
        LOG_ERR("�����Ĳ���ȷ");
        return -1;
    }

    pDoc = (XMLDOC *)(pCtx->preBuf.ptr);
    if (!pDoc) {
        LOG_ERR("����Ԥ���ΪNULL");
        return -1;
    }

    if (argc == 2) {
        if (getXMLProp(pDoc, argv[0].buf, argv[1].buf, result, size) != 0) {
            LOG_ERR("getXMLProp(%s,%s) error", argv[0].buf, argv[1].buf);
            return -1;
        }
    } else {
        if (getXMLText(pDoc, argv[0].buf, result, size) != 0) {
            /* LOG_ERR("getXMLText(%s) error", argv[0].buf); */     /*mod by dwxiong 2014-11-5 18:48:22*/
            LOG_WRN("getXMLText(%s) error", argv[0].buf);
            /* return -1; */                                        /*del by dwxiong 2014-11-5 18:51:38*/
        }
    }
    return strlen(result);
}

/***************************************************************
 * ȡXML������
 * ʾ����XML_NGET(params/param, name, bindAddr, value)
 *      ��˼��ѭ����ȡparams/param[N]/name�����ֵ, �����bindAddr
 *      ���params/param[N]/value��ֵ����
 **************************************************************/
static int tp_XML_GET(int argc, TPVAR argv[], char *result, int size)
{
    TPCTX *pCtx;
    XMLDOC *pDoc;
    
    char  name_buf[MAXVALLEN];          /*��Ҫ�ȶԵĽڵ�*/
    char  value_buf[MAXVALLEN];         /*��Ҫ��ֵ�Ľڵ�*/
    int n;
    char tmp_result[MAXVALLEN+1];
    char tmp_value[MAXKEYLEN+1];
    
    result[0] = 0x00;

    /* argv[0]: ѭ���ڵ�·�� ��params/param*/
    /* argv[1]: ��Ҫ�ȶԵĽڵ� ��name */
    /* argv[2]: ��Ҫ�ȶԵ�ֵ  ��bindAddr*/
    /* argv[3]: ��Ҫ��ֵ�Ľڵ� ��value*/

    if (argc < 3) {
        LOG_ERR("������������ȷ");
        return -1;
    }
    /*�����Ҫ��ֵ�Ľڵ㲻������Ĭ����value*/
    memset(tmp_value, 0x00, sizeof(tmp_value));
    if (argc <=3) {
        sprintf(tmp_value, "%s", "value");
    } else {
        sprintf(tmp_value, "%s", argv[3].buf);
    }
    
    pCtx = tpCtxGetTSD();
    if (!pCtx || !pCtx->preBuf.ptr) {
        LOG_ERR("�����Ĳ���ȷ");
        return -1;
    }

    pDoc = (XMLDOC *)(pCtx->preBuf.ptr);
    if (!pDoc) {
        LOG_ERR("����Ԥ���ΪNULL");
        return -1;
    }

    n = 1;
    while(1){
    	  /*ȡname��ֵ*/
    	  memset(name_buf, 0x00, sizeof(name_buf));
        sprintf(name_buf, "%s[%d]/%s", argv[0].buf, n, argv[1].buf);
        if (getXMLText(pDoc, name_buf, tmp_result, MAXVALLEN) != 0) {
            LOG_WRN("getXMLText(%s) error", name_buf);
            break;
        }
        strTrim(tmp_result);
        argv[2].buf[argv[2].len] = 0x00;
        if(strcmp(tmp_result, argv[2].buf) == 0){
        	  /*ȡvalue��ֵ*/
        	  memset(value_buf, 0x00, sizeof(value_buf));
            sprintf(value_buf, "%s[%d]/%s", argv[0].buf, n, tmp_value);
            if (getXMLText(pDoc, value_buf, result, size) != 0) {
                LOG_WRN("getXMLText(%s) error", value_buf);
                break;
            }        	
        	  break;
        }
        n++;
    }
    return strlen(result);
}

/***************************************************************
 * ȡXML������
 * ʾ����XML_NGET(params/param, name, bindAddr, value)
 *      ��˼��ѭ����ȡparams/param[N]/name�����ֵ, �����bindAddr
 *      ���params/param[N]/value��ֵ����
 **************************************************************/
static int tp_XML_NGET(int argc, TPVAR argv[], char *result, int size)
{
    TPCTX *pCtx;
    XMLDOC *pDoc;
    
    char  name_buf[MAXVALLEN];          /*��Ҫ�ȶԵĽڵ�*/
    char  value_buf[MAXVALLEN];         /*��Ҫ��ֵ�Ľڵ�*/
    int n;
    char tmp_result[MAXVALLEN+1];
    char tmp_value[MAXKEYLEN+1];
    
    result[0] = 0x00;

    /* argv[0]: ѭ���ڵ�·�� ��params/param*/
    /* argv[1]: ��Ҫ�ȶԵĽڵ� ��name */
    /* argv[2]: ��Ҫ�ȶԵ�ֵ  ��bindAddr*/
    /* argv[3]: ��Ҫ��ֵ�Ľڵ� ��value*/

    if (argc < 3) {
        LOG_ERR("������������ȷ");
        return -1;
    }
    /*�����Ҫ��ֵ�Ľڵ㲻������Ĭ����value*/
    memset(tmp_value, 0x00, sizeof(tmp_value));
    if (argc <=3) {
        sprintf(tmp_value, "%s", "value");
    } else {
        sprintf(tmp_value, "%s", argv[3].buf);
    }
    
    pCtx = tpCtxGetTSD();
    if (!pCtx || !pCtx->preBuf.ptr) {
        LOG_ERR("�����Ĳ���ȷ");
        return -1;
    }

    pDoc = (XMLDOC *)(pCtx->preBuf.ptr);
    if (!pDoc) {
        LOG_ERR("����Ԥ���ΪNULL");
        return -1;
    }

    n = 1;
    while(1){
    	  /*ȡname��ֵ*/
    	  memset(name_buf, 0x00, sizeof(name_buf));
        sprintf(name_buf, "%s[%d]/%s", argv[0].buf, n, argv[1].buf);
        if (getXMLText_Noencoding(pDoc, name_buf, tmp_result, MAXVALLEN) != 0) {
            LOG_WRN("getXMLText(%s) error", name_buf);
            break;
        }
        strTrim(tmp_result);
        argv[2].buf[argv[2].len] = 0x00;
        if(strcmp(tmp_result, argv[2].buf) == 0){
        	  /*ȡvalue��ֵ*/
        	  memset(value_buf, 0x00, sizeof(value_buf));
            sprintf(value_buf, "%s[%d]/%s", argv[0].buf, n, tmp_value);
            if (getXMLText_Noencoding(pDoc, value_buf, result, size) != 0) {
                LOG_WRN("getXMLText(%s) error", value_buf);
                break;
            }        	
        	  break;
        }
        n++;
    }
    return strlen(result);
}

/***************************************************************
 * ȡxml���Ľڵ����
 * �÷�: XML_GETNODECOUNT(TRANCODE)
 *       XML_GETNODECOUNT(LIST/lists)
 **************************************************************/
static int tp_XML_GET_NODECOUNT(int argc, TPVAR argv[], char *result, int size)
{
    TPCTX *pCtx;
    XMLDOC *pDoc;
    int count;
   
    count = 0;
    result[0] = 0x00;

    /* argv[0]: xml�ڵ��� */

    if (argc < 1) {
        LOG_ERR("������������ȷ");
        return -1;
    }

    pCtx = tpCtxGetTSD();
    if (!pCtx || !pCtx->preBuf.ptr) {
        LOG_ERR("�����Ĳ���ȷ");
        return -1;
    }

    pDoc = (XMLDOC *)(pCtx->preBuf.ptr);
    if (!pDoc) {
        LOG_ERR("����Ԥ���ΪNULL");
        return -1;
    }

    if ((count = getXMLNodeCount(pDoc, argv[0].buf)) <= 0) {
        count = 0;
        LOG_WRN("getXMLNodeCount(%s) error", argv[0].buf);
    }
    sprintf(result, "%d", count);
    return strlen(result);
}

/***************************************************************
 * ȡXML������(no encoding)
 **************************************************************/
static int tp_XML_Noencoding(int argc, TPVAR argv[], char *result, int size)
{
    TPCTX *pCtx;
    XMLDOC *pDoc;
    
    result[0] = 0x00;

    /* argv[0]: �ڵ�·�� */
    /* argv[1]: ��������(��ѡ) */

    if (argc < 1) {
        LOG_ERR("������������ȷ");
        return -1;
    }

    pCtx = tpCtxGetTSD();
    if (!pCtx || !pCtx->preBuf.ptr) {
        LOG_ERR("�����Ĳ���ȷ");
        return -1;
    }

    pDoc = (XMLDOC *)(pCtx->preBuf.ptr);
    if (!pDoc) {
        LOG_ERR("����Ԥ���ΪNULL");
        return -1;
    }

    if (argc == 2) {
        if (getXMLProp_Noencoding(pDoc, argv[0].buf, argv[1].buf, result, size) != 0) {
            LOG_ERR("getXMLProp(%s,%s) error", argv[0].buf, argv[1].buf);
            return -1;
        }
    } else {
        if (getXMLText_Noencoding(pDoc, argv[0].buf, result, size) != 0) {
            /* LOG_ERR("getXMLText(%s) error", argv[0].buf); */     /*mod by dwxiong 2014-11-5 18:48:22*/
            LOG_WRN("getXMLText(%s) error", argv[0].buf);
            /* return -1; */                                        /*del by dwxiong 2014-11-5 18:51:38*/
        }
    }
    return strlen(result);
}

/***************************************************************
 * ���������ռ�
 **************************************************************/
static int tp_XMLNS(int argc, TPVAR argv[], char *result, int size)
{
    TPCTX *pCtx;
    XMLDOC *pDoc;

    if (!(pCtx = tpCtxGetTSD())) {
        LOG_ERR("�����Ĳ���ȷ");
        return -1;
    }

    if (!(pDoc = (XMLDOC *)(pCtx->preBuf.ptr))) {
        LOG_ERR("XML��δ��ʼ��");
        return -1;
    }

    if (argc == 2) {
        /* argv[0]: prefix */
        /* argv[1]: href */
        return setXMLNameSpace(pDoc, NULL, argv[0].buf, argv[1].buf);
    } else if (argc == 3) {
        /* argv[0]: xpath */
        /* argv[1]: prefix */
        /* argv[2]: href */
        return setXMLNameSpace(pDoc, argv[0].buf, argv[1].buf, argv[2].buf);
    } else {
        LOG_ERR("������������ȷ");
        return -1;
    }
}

/***************************************************************
 * ����XML�����ַ���
 **************************************************************/
static int tp_CHRSET(int argc, TPVAR argv[], char *result, int size)
{
    TPCTX *pCtx;
    XMLDOC *pDoc;

    if (argc != 1) {
        LOG_ERR("������������ȷ");
        return -1;
    }

    if (!(pCtx = tpCtxGetTSD())) {
        LOG_ERR("�����Ĳ���ȷ");
        return -1;
    }

    if (!(pDoc = (XMLDOC *)(pCtx->preBuf.ptr))) {
        LOG_ERR("XML��δ��ʼ��");
        return -1;
    }

	strncpy(pDoc->encoding_char, argv[0].buf, sizeof(pDoc->encoding_char)-1);
    return 0;
}

/***************************************************************
 * ����XML�����ַ���
 **************************************************************/
static int tp_XMLENC(int argc, TPVAR argv[], char *result, int size)
{
    TPCTX *pCtx;
    XMLDOC *pDoc;

    if (argc != 1) {
        LOG_ERR("������������ȷ");
        return -1;
    }

    if (!(pCtx = tpCtxGetTSD())) {
        LOG_ERR("�����Ĳ���ȷ");
        return -1;
    }

    if (!(pDoc = (XMLDOC *)(pCtx->preBuf.ptr))) {
        LOG_ERR("XML��δ��ʼ��");
        return -1;
    }

	strncpy(pDoc->encoding_dump, argv[0].buf, sizeof(pDoc->encoding_dump)-1);
    return 0;
}

/***************************************************************
 * ��ʼ��XML�ĵ�
 **************************************************************/
static int tp_XMLINIT(int argc, TPVAR argv[], char *result, int size)
{
    TPCTX *pCtx;
    XMLDOC *pDoc;

    /* argv[0]: ���ڵ����� */

    if (argc < 1 || argc > 2) {
        LOG_ERR("������������ȷ");
        return -1;
    }

    if (!(pCtx = tpCtxGetTSD())) {
        LOG_ERR("�����Ĳ���ȷ");
        return -1;
    }
    if (pCtx->preBuf.ptr) {
        LOG_WRN("XML���ڵ��Ѵ���");
        return 0;
    }

    if (argc != 2 || argv[1].len <= 0) {
        if (!(pDoc = initXML(argv[0].buf, NULL))) {
            LOG_ERR("initXML(%s) error", argv[0].buf);
            return -1;
        }
    } else {
        if (!(pDoc = initXML(argv[0].buf, argv[1].buf))) {
            LOG_ERR("initXML(%s,%s) error", argv[0].buf, argv[1].buf);
            return -1;
        }
    }
    pCtx->preBuf.ptr = (void *)pDoc;
    return 0;
}

/***************************************************************
 * �½�XML�ڵ�
 **************************************************************/
static int tp_XMLNODE(int argc, TPVAR argv[], char *result, int size)
{
    TPCTX *pCtx;
    XMLDOC *pDoc;

    /* argv[0]: ���ڵ�(xpath) */
    /* argv[1]: �½ڵ����� */
    /* argv[2]: �½ڵ��ı� */

    if (!(pCtx = tpCtxGetTSD())) {
        LOG_ERR("�����Ĳ���ȷ");
        return -1;
    }

    if (!(pDoc = (XMLDOC *)(pCtx->preBuf.ptr))) {
        LOG_WRN("XML��δ��ʼ��");
        return -1;
    }

    if (argc == 2) {
        return newXMLNode(pDoc, argv[0].buf, argv[1].buf, NULL);
    } else if (argc == 3) {
        return newXMLNode(pDoc, argv[0].buf, argv[1].buf, argv[2].buf);
    } else {
        LOG_ERR("������������ȷ");
        return -1;
    }
}

/***************************************************************
 * �½�XML�ڵ�(no encoding)
 **************************************************************/
static int tp_XMLNODE_Noencoding(int argc, TPVAR argv[], char *result, int size)
{
    TPCTX *pCtx;
    XMLDOC *pDoc;

    /* argv[0]: ���ڵ�(xpath) */
    /* argv[1]: �½ڵ����� */
    /* argv[2]: �½ڵ��ı� */

    if (!(pCtx = tpCtxGetTSD())) {
        LOG_ERR("�����Ĳ���ȷ");
        return -1;
    }

    if (!(pDoc = (XMLDOC *)(pCtx->preBuf.ptr))) {
        LOG_WRN("XML��δ��ʼ��");
        return -1;
    }

    if (argc == 2) {
        return newXMLNode_Noencoding(pDoc, argv[0].buf, argv[1].buf, NULL);
    } else if (argc == 3) {
        return newXMLNode_Noencoding(pDoc, argv[0].buf, argv[1].buf, argv[2].buf);
    } else {
        LOG_ERR("������������ȷ");
        return -1;
    }
}

/***************************************************************
 * ����XML�ڵ��ı�
 **************************************************************/
static int tp_XMLTEXT(int argc, TPVAR argv[], char *result, int size)
{
    TPCTX *pCtx;
    XMLDOC *pDoc;

    /* argv[0]: �ڵ�(xpath) */
    /* argv[1]: �ı�ֵ */

    if (argc != 2) {
        LOG_ERR("������������ȷ");
        return -1;
    }

    if (!(pCtx = tpCtxGetTSD())) {
        LOG_ERR("�����Ĳ���ȷ");
        return -1;
    }

    if (!(pDoc = (XMLDOC *)(pCtx->preBuf.ptr))) {
        LOG_WRN("XML��δ��ʼ��");
        return -1;
    }
    return setXMLText(pDoc, argv[0].buf, argv[1].buf);
}

/***************************************************************
 * ����XML�ڵ��ı�(no encoding)
 **************************************************************/
static int tp_XMLTEXT_Noencoding(int argc, TPVAR argv[], char *result, int size)
{
    TPCTX *pCtx;
    XMLDOC *pDoc;

    /* argv[0]: �ڵ�(xpath) */
    /* argv[1]: �ı�ֵ */

    if (argc != 2) {
        LOG_ERR("������������ȷ");
        return -1;
    }

    if (!(pCtx = tpCtxGetTSD())) {
        LOG_ERR("�����Ĳ���ȷ");
        return -1;
    }

    if (!(pDoc = (XMLDOC *)(pCtx->preBuf.ptr))) {
        LOG_WRN("XML��δ��ʼ��");
        return -1;
    }
    return setXMLText_Noencoding(pDoc, argv[0].buf, argv[1].buf);
}

/***************************************************************
 * ����XML�ڵ�����
 **************************************************************/
static int tp_XMLPROP(int argc, TPVAR argv[], char *result, int size)
{
    TPCTX *pCtx;
    XMLDOC *pDoc;

    /* argv[0]: �ڵ�(xpath) */
    /* argv[1]: ������ */
    /* argv[2]: ����ֵ */

    if (argc != 3) {
        LOG_ERR("������������ȷ");
        return -1;
    }

    if (!(pCtx = tpCtxGetTSD())) {
        LOG_ERR("�����Ĳ���ȷ");
        return -1;
    }

    if (!(pDoc = (XMLDOC *)(pCtx->preBuf.ptr))) {
        LOG_WRN("XML��δ��ʼ��");
        return -1;
    }
    return setXMLProp(pDoc, argv[0].buf, argv[1].buf, argv[2].buf);
}

/***************************************************************
 * ����XML�ڵ�����
 **************************************************************/
static int tp_XMLPROP_Noencoding(int argc, TPVAR argv[], char *result, int size)
{
    TPCTX *pCtx;
    XMLDOC *pDoc;

    /* argv[0]: �ڵ�(xpath) */
    /* argv[1]: ������ */
    /* argv[2]: ����ֵ */

    if (argc != 3) {
        LOG_ERR("������������ȷ");
        return -1;
    }

    if (!(pCtx = tpCtxGetTSD())) {
        LOG_ERR("�����Ĳ���ȷ");
        return -1;
    }

    if (!(pDoc = (XMLDOC *)(pCtx->preBuf.ptr))) {
        LOG_WRN("XML��δ��ʼ��");
        return -1;
    }
    return setXMLProp_Noencoding(pDoc, argv[0].buf, argv[1].buf, argv[2].buf);
}


/***************************************************************
 * ȡISO8583������
 **************************************************************/
static int tp_ISO(int argc, TPVAR argv[], char *result, int size)
{
    int nId,len;
    TPCTX *pCtx;

    if (argc != 1) {
        LOG_ERR("������������ȷ");
        return -1;
    }

    pCtx = tpCtxGetTSD();
    if (!pCtx || !pCtx->preBuf.ptr) {
        LOG_ERR("�����Ĳ���ȷ");
        return -1;
    }
    nId = atoi(argv[0].buf);
    if (nId <= 0 || nId > NMAXBITS) {
        LOG_ERR("��IDֵ[%d]����ȷ", nId);
        return -1;
    }
    if ((len = tpMsgCtxGet(pCtx->preBuf.ptr, nId-1, result, size)) < 0) {
        LOG_ERR("tpMsgCtxGet(%d) error", nId-1);
        return -1;
    }
    return len;
}

/***************************************************************
 * ȡ����ͷ��
 **************************************************************/
static int tp_HDR(int argc, TPVAR argv[], char *result, int size)
{
    TPCTX  *pCtx;
    TPHEAD *ptpHead;

    if (argc != 1) {
        LOG_ERR("������������ȷ");
        return -1;
    }

    pCtx = tpCtxGetTSD();
    if (!pCtx) {
        LOG_ERR("�����Ĳ���ȷ");
        return -1;
    }
    ptpHead = &pCtx->tpMsg.head;

    if (!strcmp("retFlag", argv[0].buf)) {
        snprintf(result, size, "%d", ptpHead->retFlag);
    } else if (!strcmp("status", argv[0].buf)) {
        snprintf(result, size, "%d", ptpHead->status);
    } else if (!strcmp("tpId", argv[0].buf)) {
        snprintf(result, size, "%d", ptpHead->tpId);
    } else if (!strcmp("orgQ", argv[0].buf)) {
        snprintf(result, size, "%d", ptpHead->orgQ);
    } else if (!strcmp("desQ", argv[0].buf)) {
        snprintf(result, size, "%d", ptpHead->desQ);
    } else if (!strcmp("tranCode", argv[0].buf)) {
        snprintf(result, size, "%s", ptpHead->tranCode);
    } else if (!strcmp("servCode", argv[0].buf)) {
        snprintf(result, size, "%s", ptpHead->servCode);
    } else if (!strcmp("procCode", argv[0].buf)) {
        snprintf(result, size, "%s", ptpHead->procCode);
    } else if (!strcmp("beginTime", argv[0].buf)) {
        snprintf(result, size, "%ld", ptpHead->beginTime);
    } else if (!strcmp("msgType", argv[0].buf)) {
        snprintf(result, size, "%d", ptpHead->msgType);
    } else if (!strcmp("fmtType", argv[0].buf)) {
        snprintf(result, size, "%d", ptpHead->fmtType);
    } else if (!strcmp("bodyLen", argv[0].buf)) {
        snprintf(result, size, "%d", ptpHead->bodyLen);
    } else if (!strcmp("bodyOffset", argv[0].buf)) {
        snprintf(result, size, "%d", ptpHead->bodyOffset);
    } else if (!strcmp("shmIndex", argv[0].buf)) {
        snprintf(result, size, "%d", ptpHead->shmIndex);
    } else if (!strcmp("safId", argv[0].buf)) {
        snprintf(result, size, "%d", ptpHead->safId);
    } else {
        LOG_ERR("�ޱ���ͷ��[%s]", argv[0].buf);
        return -1;
    }
    return strlen(result);
}

/***************************************************************
 * ����ǰ�ñ���ͷ����
 **************************************************************/
static int tp_PREHDR_LEN(int argc, TPVAR argv[], char *result, int size)
{
    TPCTX *pCtx;

    if (argc != 1) {
        LOG_ERR("������������ȷ");
        return -1;
    }

    pCtx = tpCtxGetTSD();
    if (!pCtx) {
        LOG_ERR("�����Ĳ���ȷ");
        return -1;
    }

    pCtx->tpMsg.head.bodyOffset = atoi(argv[0].buf);
    return 0;
}

/***************************************************************
 * ���ú��ñ����峤��
 **************************************************************/
static int tp_PREHDR_SET(int argc, TPVAR argv[], char *result, int size)
{
    TPCTX *pCtx;
    char buf[21];

    /*argv[0]: ƫ����*/
    /*argv[1]: �򳤶�*/
    /*argv[2]: ǰ��ͷ����*/

    if (argc != 3) {
        LOG_ERR("������������ȷ");
        return -1;
    }

    pCtx = tpCtxGetTSD();
    if (!pCtx) {
        LOG_ERR("�����Ĳ���ȷ");
        return -1;
    }

    sprintf(buf, "%0*d", atoi(argv[1].buf), pCtx->tpMsg.head.bodyLen + atoi(argv[2].buf));
    memcpy(pCtx->tpMsg.body+atoi(argv[0].buf), buf, atoi(argv[1].buf));
    return 0;
}

/***************************************************************
 * ����һ��������ͬ�ַ���ɵĴ�
 **************************************************************/
static int tp_CHR(int argc, TPVAR argv[], char *result, int size)
{
    int c,len;

    if (argc < 1 || argc > 2) {
        LOG_ERR("������������ȷ");
        return -1;    
    }
   
    c = atoi(argv[0].buf);
    if (c<0 || c>255) { /*ʮ���Ʊ�ʾ��ASCII��*/
        LOG_ERR("��1������ֵ����ȷ");
        return -1;
    }

    len = (argc < 2)? 1:atoi(argv[1].buf);
    if (len <= 0) {
        LOG_ERR("��2������ֵ����ȷ");
        return -1;
    }

    memset(result, c, len); 
    return len;
}

/***************************************************************
 * ����һ��������ͬ�ַ���ɵĴ�
 **************************************************************/
static int tp_STR(int argc, TPVAR argv[], char *result, int size)
{
    int i,c,len;

    if (argc < 1) {
        LOG_ERR("������������ȷ");
        return -1;    
    }

    for (i=0,len=0; i<argc; i++) {
        if (argv[i].len <= 0) continue;
        c = atoi(argv[i].buf);
        if (c<0 || c>255) { /*ʮ���Ʊ�ʾ��ASCII��*/
            LOG_ERR("��%d������ֵ����ȷ", i+1);
            return -1;
        }
        result[len++] = c;
    }
    return len;
}

/***************************************************************
 * ȡ����ֵ����
 **************************************************************/
static int tp_LEN(int argc, TPVAR argv[], char *result, int size)
{
    if (argc != 1) {
        LOG_ERR("������������ȷ");
        return -1;    
    }
    snprintf(result, size, "%d", argv[0].len);
    return strlen(result);
}

/***************************************************************
 * ȥ�����ҿո�
 **************************************************************/
static int tp_TRIM(int argc, TPVAR argv[], char *result, int size)
{
    int len;

    if (argc != 1) {
        LOG_ERR("������������ȷ");
        return -1;    
    }
    if (!argv[0].len) return 0;

    len = (size < argv[0].len)? size : argv[0].len;
    memcpy(result, argv[0].buf, len);
    result[len] = 0;
    strTrim(result);
    return strlen(result);
}

/***************************************************************
 * ȥ����߿ո�
 **************************************************************/
static int tp_TRIML(int argc, TPVAR argv[], char *result, int size)
{
    int len;

    if (argc != 1) {
        LOG_ERR("������������ȷ");
        return -1;    
    }
    if (!argv[0].len) return 0;

    len = (size < argv[0].len)? size : argv[0].len;
    memcpy(result, argv[0].buf, len);
    result[len] = 0;
    strTrimL(result);
    return strlen(result);
}

/***************************************************************
 * ȥ���ұ߿ո�
 **************************************************************/
static int tp_TRIMR(int argc, TPVAR argv[], char *result, int size)
{
    int len;

    if (argc != 1) {
        LOG_ERR("������������ȷ");
        return -1;    
    }
    if (!argv[0].len) return 0;

    len = (size < argv[0].len)? size : argv[0].len;
    memcpy(result, argv[0].buf, len);
    result[len] = 0;
    strTrimR(result);
    return strlen(result);
}

/***************************************************************
 * ����ַ���
 **************************************************************/
static int tp_FILL(int argc, TPVAR argv[], char *result, int size)
{
    int len,tmpLen;

    /* argv[0]: ����䴮 */
    /* argv[1]: ����ַ� */
    /* argv[2]: ��䷽ʽ(L����/R����) */
    /* argv[3]: ���󳤶� */

    if (argc < 4) {
        LOG_ERR("������������ȷ");
        return -1;    
    }

    len = (size < atoi(argv[3].buf))? size : atoi(argv[3].buf);
    tmpLen = len - argv[0].len;
    if (tmpLen <= 0) {
        /*len = (size < argv[0].len)? size : argv[0].len;*/
        memcpy(result, argv[0].buf, len);
        return len;
    }
    if ('L' == toupper(argv[2].buf[0])) {
        memset(result, argv[1].buf[0], tmpLen);
        memcpy(result+tmpLen, argv[0].buf, argv[0].len);
    } else if ('R' == toupper(argv[2].buf[0])) {
        memcpy(result, argv[0].buf, argv[0].len);
        memset(result+argv[0].len, argv[1].buf[0], tmpLen);
    } else {
        LOG_ERR("��3����������ȷ");
        return -1;        
    }
    return len;
}

/***************************************************************
 * ���Ӷ���ַ���
 **************************************************************/
static int tp_CAT(int argc, TPVAR argv[], char *result, int size)
{
    int i,len,tmpLen;

    if (argc < 2) {
        LOG_ERR("������������ȷ");
        return -1;    
    }

    for (i=0,len=0; i<argc; i++) {
        if (argv[i].len <= 0) continue;
        tmpLen = (size < len+argv[i].len)? (len+argv[i].len-size) : argv[i].len;
        memcpy(result+len, argv[i].buf, tmpLen);
        len += tmpLen;
    }
    return len;
}

/***************************************************************
 * ȡ�Ӵ�
 **************************************************************/
static int tp_MID(int argc, TPVAR argv[], char *result, int size)
{
    int len,offset;

    /* argv[0]: ԭʼ�� */
    /* argv[1]: ƫ���� */
    /* argv[2]: ȡ�೤ */

    if (argc != 3) {
        LOG_ERR("������������ȷ");
        return -1;    
    }
    if (!argv[0].len) return 0;
    
    offset = atoi(argv[1].buf);
    if (offset >= argv[0].len) return 0;
    if (offset < 0) offset = 0;
    
    len = atoi(argv[2].buf);
    if (len <= 0) return 0;
    if (len > size) len = size;
    if (len > (argv[0].len-offset)) len = argv[0].len-offset;

    memcpy(result, argv[0].buf+offset, len);
    return len;
}

/***************************************************************
 * ȡ����Ӵ�
 **************************************************************/
static int tp_LEFT(int argc, TPVAR argv[], char *result, int size)
{
    /* argv[0]: ԭʼ�� */
    /* argv[1]: ȡ�೤ */

    int len;

    if (argc != 2) {
        LOG_ERR("������������ȷ");
        return -1;    
    }
    if (!argv[0].len) return 0;
    
    len = atoi(argv[1].buf);
    if (len <= 0) return 0;
    if (len > argv[0].len) len = argv[0].len;
    if (len > size) len = size;

    memcpy(result, argv[0].buf, len);
    return len;
}

/***************************************************************
 * ȡ�ұ��Ӵ�
 **************************************************************/
static int tp_RIGHT(int argc, TPVAR argv[], char *result, int size)
{
    int len,offset;

    /* argv[0]: ԭʼ�� */
    /* argv[1]: ȡ�೤ */

    if (argc != 2) {
        LOG_ERR("������������ȷ");
        return -1;    
    }
    if (!argv[0].len) return 0;

    len = atoi(argv[1].buf);
    if (len <= 0) return 0;
    if (len > argv[0].len) len = argv[0].len;
    if (len > size) len = size;

    offset = argv[0].len - len;
    if (offset < 0) offset = 0;

    memcpy(result, argv[0].buf+offset, len);
    return len;
}

/***************************************************************
 * �߼���
 **************************************************************/
static int tp_AND(int argc, TPVAR argv[], char *result, int size)
{
    int i;

    if (argc < 2) {
        LOG_ERR("������������ȷ");
        return -1;    
    }

    result[0] = 1;
    for (i=0; i<argc; i++) {
        if (argv[i].len == 0 || argv[i].buf[0] == 0 || argv[i].buf[0] == '0') {
            result[0] = 0;
            break;
        }
    }
    return 1;
}

/***************************************************************
 * �߼���
 **************************************************************/
static int tp_OR(int argc, TPVAR argv[], char *result, int size)
{
    int i;

    if (argc < 2) {
        LOG_ERR("������������ȷ");
        return -1;    
    }

    result[0] = 0;
    for (i=0; i<argc; i++) {
        if (argv[i].len > 0 && argv[i].buf[0] != 0 && argv[i].buf[0] != '0') {
            result[0] = 1;
            break;
        }
    }
    return 1;
}

/***************************************************************
 * �߼���
 **************************************************************/
static int tp_NOT(int argc, TPVAR argv[], char *result, int size)
{
    if (argc != 1) {
        LOG_ERR("������������ȷ");
        return -1;    
    }

    if (argv[0].len > 0 && argv[0].buf[0] != 0 && argv[0].buf[0] != '0') {
        result[0] = 0;
    } else {
        result[0] = 1;
    }
    return 1;
}

/***************************************************************
 * �Ƚ�������ֵ�Ƿ����
 **************************************************************/
static int tp_EQ(int argc, TPVAR argv[], char *result, int size)
{
    if (argc != 2) {
        LOG_ERR("������������ȷ");
        return -1;    
    }

    result[0] = (atof(argv[0].buf) == atof(argv[1].buf))? 1:0;
    return 1;
}

/***************************************************************
 * �Ƚ������ַ����Ƿ����
 **************************************************************/
static int tp_SEQ(int argc, TPVAR argv[], char *result, int size)
{
    if (argc != 2) {
        LOG_ERR("������������ȷ");
        return -1;    
    }

    result[0] = (argv[0].len != argv[1].len || memcmp(argv[0].buf, argv[1].buf, argv[1].len))? 0:1;
    return 1;
}

/***************************************************************
 * �ж��Ƿ����
 **************************************************************/
static int tp_GT(int argc, TPVAR argv[], char *result, int size)
{
    if (argc != 2) {
        LOG_ERR("������������ȷ");
        return -1;    
    }

    result[0] = (atof(argv[0].buf) > atof(argv[1].buf))? 1:0;
    return 1;
}

/***************************************************************
 * �ж��Ƿ���ڵ���
 **************************************************************/
static int tp_GE(int argc, TPVAR argv[], char *result, int size)
{
    if (argc != 2) {
        LOG_ERR("������������ȷ");
        return -1;    
    }

    result[0] = (atof(argv[0].buf) >= atof(argv[1].buf))? 1:0;   
    return 1;
}

/***************************************************************
 * �ж��Ƿ�С��
 **************************************************************/
static int tp_LT(int argc, TPVAR argv[], char *result, int size)
{
    if (argc != 2) {
        LOG_ERR("������������ȷ");
        return -1;    
    }

    result[0] = (atof(argv[0].buf) < atof(argv[1].buf))? 1:0;
    return 1;
}

/***************************************************************
 * �ж��Ƿ�С�ڵ���
 **************************************************************/
static int tp_LE(int argc, TPVAR argv[], char *result, int size)
{
    if (argc != 2) {
        LOG_ERR("������������ȷ");
        return -1;    
    }

    result[0] = (atof(argv[0].buf) <= atof(argv[1].buf))? 1:0;
    return 1;
}

/***************************************************************
 * ������ֵ���
 **************************************************************/
static int tp_ADD(int argc, TPVAR argv[], char *result, int size)
{
    double f;

    /* argv[0]: */
    /* argv[1]: */
    /* argv[2]: ������С��λ��(��ѡ)*/

    if (argc < 2 || argc > 3) {
        LOG_ERR("������������ȷ");
        return -1;    
    }

    f = atof(argv[0].buf) + atof(argv[1].buf);
    snprintf(result, size, "%.*lf", atoi(argv[2].buf), f);
    return strlen(result);
}

/***************************************************************
 * ������ֵ���
 **************************************************************/
static int tp_SUB(int argc, TPVAR argv[], char *result, int size)
{
    double f;

    /* argv[0]: */
    /* argv[1]: */
    /* argv[2]: ������С��λ��(��ѡ)*/

    if (argc < 2 || argc > 3) {
        LOG_ERR("������������ȷ");
        return -1;    
    }

    f = atof(argv[0].buf) - atof(argv[1].buf);
    snprintf(result, size, "%.*lf", atoi(argv[2].buf), f);
    return strlen(result);
}

/***************************************************************
 * ������ֵ���
 **************************************************************/
static int tp_MUL(int argc, TPVAR argv[], char *result, int size)
{
    double f;

    /* argv[0]: */
    /* argv[1]: */
    /* argv[2]: ������С��λ��(��ѡ)*/

    if (argc < 2 || argc > 3) {
        LOG_ERR("������������ȷ");
        return -1;    
    }

    f = atof(argv[0].buf) * atof(argv[1].buf);
    snprintf(result, size, "%.*lf", atoi(argv[2].buf), f);
    return strlen(result);
}

/***************************************************************
 * ������ֵ���
 **************************************************************/
static int tp_DIV(int argc, TPVAR argv[], char *result, int size)
{
    double f;

    /* argv[0]: */
    /* argv[1]: */
    /* argv[2]: ������С��λ��(��ѡ)*/

    if (argc < 2 || argc > 3) {
        LOG_ERR("������������ȷ");
        return -1;    
    }

    if (atof(argv[1].buf) == 0) {
        LOG_ERR("��2������ֵ����ȷ");
        return -1;
    }

    f = atof(argv[0].buf) / atof(argv[1].buf);
    snprintf(result, size, "%.*lf", atoi(argv[2].buf), f);
    return strlen(result);
}

/***************************************************************
 * ������ֵ
 **************************************************************/
static int tp_IF(int argc, TPVAR argv[], char *result, int size)
{
    int len;

    if (argc != 3) {
        LOG_ERR("������������ȷ");
        return -1;    
    }

    if (argv[0].len > 0 && argv[0].buf[0]) {
        len = (size < argv[1].len)? size : argv[1].len;
        memcpy(result, argv[1].buf, len);
    } else {
        len = (size < argv[2].len)? size : argv[2].len;
        memcpy(result, argv[2].buf, len);
    }
    return len;
}

/***************************************************************
 * ����ƥ��
 **************************************************************/
static int tp_REG(int argc, TPVAR argv[], char *result, int size)
{
    /* argv[0]: ƥ���ַ���*/
    /* argv[1]: ������ʽ*/

    if (argc != 2) {
        LOG_ERR("������������ȷ");
        return -1;    
    }

    result[0] = (regMatch(argv[0].buf, argv[1].buf))? 0:1;
    return 1;
}    

/***************************************************************
 * ȡϵͳ��ǰ���ں�ʱ��
 **************************************************************/
static int tp_DATETIME(int argc, TPVAR argv[], char *result, int size)
{
    time_t t;

    /* argv[0]: ָ����ʽ(��ѡ) */

    if (argc > 1) {
        LOG_ERR("������������ȷ");
        return -1;    
    }

    time(&t);
    if (argc < 1) {
        getTimeString(t, "yyyy-MM-dd hh:mm:ss", result);
    } else {
        getTimeString(t, argv[0].buf, result);
    }
    return strlen(result);
}

/***************************************************************
 * ���ؿ�ֵ
 **************************************************************/
static int tp_NULL(int argc, TPVAR argv[], char *result, int size)
{
    return 0;
}

static int tp_KEYVAL_SET(int argc, TPVAR argv[], char *result, int size)
{
    TPCTX *pCtx;
    char buf[21];

    /*argv[0]: ƫ����*/
    /*argv[1]: KEYֵ*/

    if (argc != 2) {
        LOG_ERR("������������ȷ");
        return -1;
    }

    pCtx = tpCtxGetTSD();
    if (!pCtx) {
        LOG_ERR("�����Ĳ���ȷ");
        return -1;
    }
/*
    sprintf(buf, "%0*d", atoi(argv[1].buf), pCtx->tpMsg.head.bodyLen + atoi(argv[2].buf));
*/
    memcpy(pCtx->tpMsg.body+atoi(argv[0].buf), argv[1].buf,argv[1].len);
    return 0;
}

