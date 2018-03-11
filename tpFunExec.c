/***************************************************************
 * 脚本函数库
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


static int tp_XMLNODE_Noencoding(int argc, TPVAR argv[], char *result, int size); /* 新建XML节点     */
static int tp_XMLTEXT_Noencoding(int argc, TPVAR argv[], char *result, int size); /* 设置XML节点文本 */
static int tp_XML_Noencoding(int argc, TPVAR argv[], char *result, int size);     /* 取XML报文域     */
static int tp_XMLPROP_Noencoding(int argc, TPVAR argv[], char *result, int size); /* 设置XML节点属性     */
static int tp_XML_GET  (int argc, TPVAR argv[], char *result, int size);          /* 取xml循环节点的某个值*/
static int tp_XML_NGET (int argc, TPVAR argv[], char *result, int size);          /* 取xml循环节点的某个值 Noencoding*/
static int tp_XML_GET_NODECOUNT(int argc, TPVAR argv[], char *result, int size);   /*获取XML节点个数*/


/* 脚本函数个数 */
static int   m_size = 0;
static TPFUN m_list[512];

/* 脚本函数列表 */
static TPFUN  TPFUN_LIST[] = {
       {"CTX",          tp_CTX        },  /* 取上下文域 */
       {"NVL",          tp_NVL        },  /* 取定长报文域 */
       {"OFFSET",       tp_OFFSET     },  /* 重置NVL偏移量 */
       {"OFFMOV",       tp_OFFMOV     },  /* 加减NVL偏移量 */
       {"CSV",          tp_CSV        },  /* 取分隔符报文域 */
       {"TLV",          tp_TLV        },  /* 取TLV报文域 */
       {"TLVSTR",       tp_TLVSTR     },  /* 返回TLV格式串 */
       {"FML",          tp_FML        },  /* 取FML报文域 */
       {"XML",          tp_XML        },  /* 取XML报文域 */
       {"XMLNS",        tp_XMLNS      },  /* 设置命名空间 */
       {"CHRSET",       tp_CHRSET     },  /* 设置XML操作字符集 */
       {"XMLENC",       tp_XMLENC     },  /* 设置XML编码字符集 */
       {"XMLINIT",      tp_XMLINIT    },  /* 初始化XML文档 */
       {"XMLNODE",      tp_XMLNODE    },  /* 新建XML节点 */
       {"XMLTEXT",      tp_XMLTEXT    },  /* 设置XML节点文本 */
       {"XMLPROP",      tp_XMLPROP    },  /* 设置XML节点属性 */
       {"ISO",          tp_ISO        },  /* 取ISO8583报文域 */
       {"HDR",          tp_HDR        },  /* 取报文头域 */
       {"PREHDR_LEN",   tp_PREHDR_LEN },  /* 设置前置报文头长度 */
       {"PREHDR_SET",   tp_PREHDR_SET },  /* 设置后置报文体长度 */
       {"CHR",          tp_CHR        },  /* 返回一个或多个相同字符组成的串 */
       {"STR",          tp_STR        },  /* 返回一个或多个不同字符组成的串 */
       {"LEN",          tp_LEN        },  /* 取参数值长度 */
       {"TRIM",         tp_TRIM       },  /* 去除左右空格 */
       {"TRIML",        tp_TRIML      },  /* 去除左边空格 */
       {"TRIMR",        tp_TRIMR      },  /* 去除右边空格 */
       {"FILL",         tp_FILL       },  /* 填充字符串 */
       {"CAT",          tp_CAT        },  /* 连接多个字符串 */
       {"SAD",          tp_CAT        },  /* 连接多个字符串(兼容先前用法) */
       {"MID",          tp_MID        },  /* 取子串 */
       {"LEFT",         tp_LEFT       },  /* 取左边子串 */
       {"RIGHT",        tp_RIGHT      },  /* 取右边子串 */
       {"AND",          tp_AND        },  /* 逻辑与 */
       {"OR",           tp_OR         },  /* 逻辑或 */
       {"NOT",          tp_NOT        },  /* 逻辑非 */
       {"EQ",           tp_EQ         },  /* 比较两个数值是否相等 */
       {"SEQ",          tp_SEQ        },  /* 比较两个字符串是否相等 */
       {"GT",           tp_GT         },  /* 判断是否大于 */
       {"GE",           tp_GE         },  /* 判断是否大于等于 */
       {"LT",           tp_LT         },  /* 判断是否小于 */
       {"LE",           tp_LE         },  /* 判断是否小于等于 */
       {"ADD",          tp_ADD        },  /* 两个数值相加 */
       {"SUB",          tp_SUB        },  /* 两个数值相减 */
       {"MUL",          tp_MUL        },  /* 两个数值相乘 */
       {"MULTI",        tp_MUL        },  /* 两个数值相乘(兼容先前用法) */
       {"DIV",          tp_DIV        },  /* 两个数值相除 */
       {"IF",           tp_IF         },  /* 条件赋值 */
       {"REG",          tp_REG        },  /* 正则匹配 */
       {"DATETIME",     tp_DATETIME   },  /* 取系统当前日期和时间 */
       {"KEYVAL_SET",   tp_KEYVAL_SET },  /* 设置异步KEY值 */
       {"NULL",         tp_NULL       },  /* 返回空值 */
       {"XMLNODE_N",    tp_XMLNODE_Noencoding  },  /* 新建XML节点 */       
       {"XMLTEXT_N",    tp_XMLTEXT_Noencoding  },  /* 设置XML节点文本 */
       {"XML_N",        tp_XML_Noencoding      },  /* 取XML报文域 */
       {"XMLPROP_N",    tp_XMLPROP_Noencoding  },  /* 设置XML节点属性 */    
       {"XML_GET",      tp_XML_GET             },  /* 取xml循环节点的某个值 */
       {"XML_NGET",     tp_XML_NGET            },  /* 取xml循环节点的某个值Noencoding */
       {"XML_GET_NODECOUNT",     tp_XML_GET_NODECOUNT     },  /* 获取xml节点个数 */
       {"",             NULL          }   /* END! */
};

static pthread_once_t once_ctl = PTHREAD_ONCE_INIT;

/***************************************************************
 * 装载扩展脚本函数库
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
 * 初始化脚本函数列表
 **************************************************************/
static void loadAndInit()
{
    int flag,size;
    char *p,*ch,buf[256];
    TPFUN *list;

    /*装载标准脚本函数库*/
    for (size=0; TPFUN_LIST[size].fn[0]; size++) {}
    memcpy(m_list, TPFUN_LIST, sizeof(TPFUN)*size);
    m_size = size;

    /*装载扩展脚本函数库*/
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
                        LOG_ERR("装载扩展脚本函数库[%s]出错", p);
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
 * 返回脚本函数列表
 **************************************************************/
int tpFunGetList(TPFUN **ppList)
{
    loadAndInit();
    *ppList = m_list;
    return m_size;
}

/***************************************************************
 * 返回表达式计算结果
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
                LOG_ERR("表达式过长");
                return -1;
            }
            stk[i].len = memTrimCopy(stk[i].buf, p, l);
            stk[i].flg = (*ch==',')? 0:1;
            l = 0;
            p = ++ch;
            break;
        case ')':
            if (++i >= STKSIZE) {
                LOG_ERR("表达式过长");
                return -1;
            }
            stk[i].len = memTrimCopy(stk[i].buf, p, l);
            stk[i].flg = 0;
            if (stk[i].len <= 0) i--;
            l = 0;
            p = ++ch;
                
            for (j=i; j>=0; j--) { if (stk[j].flg) break; }
            if (j<0) {
                LOG_ERR("表达式语法错误");
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
                    LOG_ERR("执行脚本函数[%s]出错", ptpFun->fn);
                    return -1;
                }
                flag = 1;
                memcpy(stk[j].buf, tmpBuf, tmpLen);
                stk[j].len = tmpLen;
            } else {
                if (argc != 1) {
                    LOG_ERR("表达式语法错误");
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
        LOG_ERR("表达式语法错误");
        return -1;
    }
    if (stk[0].flg) {
        LOG_ERR("表达式语法错误");
        return -1;
    }
    len = (size < stk[0].len+1)? size-1 : stk[0].len;
    memcpy(result, stk[0].buf, len);
    result[len] = 0;
    return len;
}

/***************************************************************
 * 设置上下文线程数据
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
 * 获取上下文线程数据
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
 * 取上下文域
 **************************************************************/
static int tp_CTX(int argc, TPVAR argv[], char *result, int size)
{
    int i,len;
    char key[MAXKEYLEN+1];
    TPCTX *pCtx;

    /*argv[0]: 域名*/
    /*argv[1]: 域ID(可选)*/
    
    if (argc < 1 || argc > 2) {
        LOG_ERR("参数个数不正确");
        return -1;
    }
    
    pCtx = tpCtxGetTSD();
    if (!pCtx || !pCtx->msgCtx) {
        LOG_ERR("上下文不正确");
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
            LOG_ERR("第2个参数不正确");
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
 * 取定长报文域
 **************************************************************/
static int tp_NVL(int argc, TPVAR argv[], char *result, int size)
{
    TPCTX *pCtx;
    int len,offset;

    pCtx = tpCtxGetTSD();
    if (!pCtx) {
        LOG_ERR("上下文不正确");
        return -1;
    }

    switch (argc) {
        case 1: /*argv[0]: 值长度*/
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
        case 2: /*argv[0]: 偏移量, argv[1]: 值长度*/ 
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
            LOG_ERR("参数个数不正确");
            return -1;
    }
    return len;
}

/***************************************************************
 * 重置NVL偏移量
 **************************************************************/
static int tp_OFFSET(int argc, TPVAR argv[], char *result, int size)
{
    TPCTX *pCtx;

    if (argc != 1) {
        LOG_ERR("参数个数不正确");
        return -1;
    }

    pCtx = tpCtxGetTSD();
    if (!pCtx) {
        LOG_ERR("上下文不正确");
        return -1;
    }

    pCtx->preBuf.offset = atoi(argv[0].buf);
    if (pCtx->preBuf.offset < 0 ) pCtx->preBuf.offset = 0;
    return 0;
}

/***************************************************************
 * 加减NVL偏移量
 **************************************************************/
static int tp_OFFMOV(int argc, TPVAR argv[], char *result, int size)
{
    TPCTX *pCtx;

    if (argc != 1) {
        LOG_ERR("参数个数不正确");
        return -1;
    }

    pCtx = tpCtxGetTSD();
    if (!pCtx) {
        LOG_ERR("上下文不正确");
        return -1;
    }

    pCtx->preBuf.offset += atoi(argv[0].buf);
    if (pCtx->preBuf.offset < 0 ) pCtx->preBuf.offset = 0;
    return 0;
}

/***************************************************************
 * 取分隔符报文域
 **************************************************************/
static int tp_CSV(int argc, TPVAR argv[], char *result, int size)
{
    int fId,rId,gId,len;
    char key[MAXKEYLEN+1];
    TPCTX *pCtx;

    switch (argc) {
        case 1: /* argv[0]: 域ID */
        case 2: /* argv[0]: 行ID, argv[1]: 域ID */
        case 3: /* argv[0]: 组ID, argv[1]: 行ID, argv[2]: 域ID*/
            pCtx = tpCtxGetTSD();
            if (!pCtx || !pCtx->preBuf.ptr) {
                LOG_ERR("上下文不正确");
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
            LOG_ERR("参数个数不正确");
            return -1;
    }
    return -1;
}

/***************************************************************
 * 取TLV报文域
 **************************************************************/
static int tp_TLV(int argc, TPVAR argv[], char *result, int size)
{
    TPCTX *pCtx;
    int   tlvLen;
    char *tlvTag,*tlvBuf;

    switch (argc) {
        case 1: /* argv[0]: TAG*/
            if (!(pCtx = tpCtxGetTSD())) {
                LOG_ERR("上下文不正确");
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
            LOG_ERR("参数个数不正确");
            return -1;
    }
    return tlvGet(tlvBuf, tlvLen, tlvTag, result, size);
}

/***************************************************************
 * 返回TLV格式串
 **************************************************************/
static int tp_TLVSTR(int argc, TPVAR argv[], char *result, int size)
{
    /* argv[0]: TAG*/
    /* argv[1]: VALUE */

    if (argc != 2) {
        LOG_ERR("参数个数不正确");
        return -1;
    }
    return tlvCat(result, size, argv[0].buf, argv[1].buf, argv[1].len);
}

/***************************************************************
 * 取FML报文域
 **************************************************************/
static int tp_FML(int argc, TPVAR argv[], char *result, int size)
{
    int i,len;
    char key[MAXKEYLEN+1];
    TPCTX  *pCtx;
    MSGCTX *pMsgCtx;

    /*argv[0]: 域名*/
    /*argv[1]: 域ID(可选)*/

    if (argc < 1 || argc > 2) {
        LOG_ERR("参数个数不正确");
        return -1;
    }
    
    pCtx = tpCtxGetTSD();
    if (!pCtx || !pCtx->preBuf.ptr) {
        LOG_ERR("上下文不正确");
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
            LOG_ERR("第2个参数不正确");
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
 * 取XML报文域
 **************************************************************/
static int tp_XML(int argc, TPVAR argv[], char *result, int size)
{
    TPCTX *pCtx;
    XMLDOC *pDoc;
    
    result[0] = 0x00;

    /* argv[0]: 节点路径 */
    /* argv[1]: 属性名称(可选) */

    if (argc < 1) {
        LOG_ERR("参数个数不正确");
        return -1;
    }

    pCtx = tpCtxGetTSD();
    if (!pCtx || !pCtx->preBuf.ptr) {
        LOG_ERR("上下文不正确");
        return -1;
    }

    pDoc = (XMLDOC *)(pCtx->preBuf.ptr);
    if (!pDoc) {
        LOG_ERR("报文预解池为NULL");
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
 * 取XML报文域
 * 示例：XML_NGET(params/param, name, bindAddr, value)
 *      意思是循环获取params/param[N]/name下面的值, 如果是bindAddr
 *      则把params/param[N]/value的值返回
 **************************************************************/
static int tp_XML_GET(int argc, TPVAR argv[], char *result, int size)
{
    TPCTX *pCtx;
    XMLDOC *pDoc;
    
    char  name_buf[MAXVALLEN];          /*需要比对的节点*/
    char  value_buf[MAXVALLEN];         /*需要赋值的节点*/
    int n;
    char tmp_result[MAXVALLEN+1];
    char tmp_value[MAXKEYLEN+1];
    
    result[0] = 0x00;

    /* argv[0]: 循环节点路径 如params/param*/
    /* argv[1]: 需要比对的节点 如name */
    /* argv[2]: 需要比对的值  如bindAddr*/
    /* argv[3]: 需要赋值的节点 如value*/

    if (argc < 3) {
        LOG_ERR("参数个数不正确");
        return -1;
    }
    /*如果需要赋值的节点不输入则默认是value*/
    memset(tmp_value, 0x00, sizeof(tmp_value));
    if (argc <=3) {
        sprintf(tmp_value, "%s", "value");
    } else {
        sprintf(tmp_value, "%s", argv[3].buf);
    }
    
    pCtx = tpCtxGetTSD();
    if (!pCtx || !pCtx->preBuf.ptr) {
        LOG_ERR("上下文不正确");
        return -1;
    }

    pDoc = (XMLDOC *)(pCtx->preBuf.ptr);
    if (!pDoc) {
        LOG_ERR("报文预解池为NULL");
        return -1;
    }

    n = 1;
    while(1){
    	  /*取name的值*/
    	  memset(name_buf, 0x00, sizeof(name_buf));
        sprintf(name_buf, "%s[%d]/%s", argv[0].buf, n, argv[1].buf);
        if (getXMLText(pDoc, name_buf, tmp_result, MAXVALLEN) != 0) {
            LOG_WRN("getXMLText(%s) error", name_buf);
            break;
        }
        strTrim(tmp_result);
        argv[2].buf[argv[2].len] = 0x00;
        if(strcmp(tmp_result, argv[2].buf) == 0){
        	  /*取value的值*/
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
 * 取XML报文域
 * 示例：XML_NGET(params/param, name, bindAddr, value)
 *      意思是循环获取params/param[N]/name下面的值, 如果是bindAddr
 *      则把params/param[N]/value的值返回
 **************************************************************/
static int tp_XML_NGET(int argc, TPVAR argv[], char *result, int size)
{
    TPCTX *pCtx;
    XMLDOC *pDoc;
    
    char  name_buf[MAXVALLEN];          /*需要比对的节点*/
    char  value_buf[MAXVALLEN];         /*需要赋值的节点*/
    int n;
    char tmp_result[MAXVALLEN+1];
    char tmp_value[MAXKEYLEN+1];
    
    result[0] = 0x00;

    /* argv[0]: 循环节点路径 如params/param*/
    /* argv[1]: 需要比对的节点 如name */
    /* argv[2]: 需要比对的值  如bindAddr*/
    /* argv[3]: 需要赋值的节点 如value*/

    if (argc < 3) {
        LOG_ERR("参数个数不正确");
        return -1;
    }
    /*如果需要赋值的节点不输入则默认是value*/
    memset(tmp_value, 0x00, sizeof(tmp_value));
    if (argc <=3) {
        sprintf(tmp_value, "%s", "value");
    } else {
        sprintf(tmp_value, "%s", argv[3].buf);
    }
    
    pCtx = tpCtxGetTSD();
    if (!pCtx || !pCtx->preBuf.ptr) {
        LOG_ERR("上下文不正确");
        return -1;
    }

    pDoc = (XMLDOC *)(pCtx->preBuf.ptr);
    if (!pDoc) {
        LOG_ERR("报文预解池为NULL");
        return -1;
    }

    n = 1;
    while(1){
    	  /*取name的值*/
    	  memset(name_buf, 0x00, sizeof(name_buf));
        sprintf(name_buf, "%s[%d]/%s", argv[0].buf, n, argv[1].buf);
        if (getXMLText_Noencoding(pDoc, name_buf, tmp_result, MAXVALLEN) != 0) {
            LOG_WRN("getXMLText(%s) error", name_buf);
            break;
        }
        strTrim(tmp_result);
        argv[2].buf[argv[2].len] = 0x00;
        if(strcmp(tmp_result, argv[2].buf) == 0){
        	  /*取value的值*/
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
 * 取xml报文节点个数
 * 用法: XML_GETNODECOUNT(TRANCODE)
 *       XML_GETNODECOUNT(LIST/lists)
 **************************************************************/
static int tp_XML_GET_NODECOUNT(int argc, TPVAR argv[], char *result, int size)
{
    TPCTX *pCtx;
    XMLDOC *pDoc;
    int count;
   
    count = 0;
    result[0] = 0x00;

    /* argv[0]: xml节点名 */

    if (argc < 1) {
        LOG_ERR("参数个数不正确");
        return -1;
    }

    pCtx = tpCtxGetTSD();
    if (!pCtx || !pCtx->preBuf.ptr) {
        LOG_ERR("上下文不正确");
        return -1;
    }

    pDoc = (XMLDOC *)(pCtx->preBuf.ptr);
    if (!pDoc) {
        LOG_ERR("报文预解池为NULL");
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
 * 取XML报文域(no encoding)
 **************************************************************/
static int tp_XML_Noencoding(int argc, TPVAR argv[], char *result, int size)
{
    TPCTX *pCtx;
    XMLDOC *pDoc;
    
    result[0] = 0x00;

    /* argv[0]: 节点路径 */
    /* argv[1]: 属性名称(可选) */

    if (argc < 1) {
        LOG_ERR("参数个数不正确");
        return -1;
    }

    pCtx = tpCtxGetTSD();
    if (!pCtx || !pCtx->preBuf.ptr) {
        LOG_ERR("上下文不正确");
        return -1;
    }

    pDoc = (XMLDOC *)(pCtx->preBuf.ptr);
    if (!pDoc) {
        LOG_ERR("报文预解池为NULL");
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
 * 设置命名空间
 **************************************************************/
static int tp_XMLNS(int argc, TPVAR argv[], char *result, int size)
{
    TPCTX *pCtx;
    XMLDOC *pDoc;

    if (!(pCtx = tpCtxGetTSD())) {
        LOG_ERR("上下文不正确");
        return -1;
    }

    if (!(pDoc = (XMLDOC *)(pCtx->preBuf.ptr))) {
        LOG_ERR("XML尚未初始化");
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
        LOG_ERR("参数个数不正确");
        return -1;
    }
}

/***************************************************************
 * 设置XML操作字符集
 **************************************************************/
static int tp_CHRSET(int argc, TPVAR argv[], char *result, int size)
{
    TPCTX *pCtx;
    XMLDOC *pDoc;

    if (argc != 1) {
        LOG_ERR("参数个数不正确");
        return -1;
    }

    if (!(pCtx = tpCtxGetTSD())) {
        LOG_ERR("上下文不正确");
        return -1;
    }

    if (!(pDoc = (XMLDOC *)(pCtx->preBuf.ptr))) {
        LOG_ERR("XML尚未初始化");
        return -1;
    }

	strncpy(pDoc->encoding_char, argv[0].buf, sizeof(pDoc->encoding_char)-1);
    return 0;
}

/***************************************************************
 * 设置XML编码字符集
 **************************************************************/
static int tp_XMLENC(int argc, TPVAR argv[], char *result, int size)
{
    TPCTX *pCtx;
    XMLDOC *pDoc;

    if (argc != 1) {
        LOG_ERR("参数个数不正确");
        return -1;
    }

    if (!(pCtx = tpCtxGetTSD())) {
        LOG_ERR("上下文不正确");
        return -1;
    }

    if (!(pDoc = (XMLDOC *)(pCtx->preBuf.ptr))) {
        LOG_ERR("XML尚未初始化");
        return -1;
    }

	strncpy(pDoc->encoding_dump, argv[0].buf, sizeof(pDoc->encoding_dump)-1);
    return 0;
}

/***************************************************************
 * 初始化XML文档
 **************************************************************/
static int tp_XMLINIT(int argc, TPVAR argv[], char *result, int size)
{
    TPCTX *pCtx;
    XMLDOC *pDoc;

    /* argv[0]: 根节点名称 */

    if (argc < 1 || argc > 2) {
        LOG_ERR("参数个数不正确");
        return -1;
    }

    if (!(pCtx = tpCtxGetTSD())) {
        LOG_ERR("上下文不正确");
        return -1;
    }
    if (pCtx->preBuf.ptr) {
        LOG_WRN("XML根节点已存在");
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
 * 新建XML节点
 **************************************************************/
static int tp_XMLNODE(int argc, TPVAR argv[], char *result, int size)
{
    TPCTX *pCtx;
    XMLDOC *pDoc;

    /* argv[0]: 父节点(xpath) */
    /* argv[1]: 新节点名称 */
    /* argv[2]: 新节点文本 */

    if (!(pCtx = tpCtxGetTSD())) {
        LOG_ERR("上下文不正确");
        return -1;
    }

    if (!(pDoc = (XMLDOC *)(pCtx->preBuf.ptr))) {
        LOG_WRN("XML尚未初始化");
        return -1;
    }

    if (argc == 2) {
        return newXMLNode(pDoc, argv[0].buf, argv[1].buf, NULL);
    } else if (argc == 3) {
        return newXMLNode(pDoc, argv[0].buf, argv[1].buf, argv[2].buf);
    } else {
        LOG_ERR("参数个数不正确");
        return -1;
    }
}

/***************************************************************
 * 新建XML节点(no encoding)
 **************************************************************/
static int tp_XMLNODE_Noencoding(int argc, TPVAR argv[], char *result, int size)
{
    TPCTX *pCtx;
    XMLDOC *pDoc;

    /* argv[0]: 父节点(xpath) */
    /* argv[1]: 新节点名称 */
    /* argv[2]: 新节点文本 */

    if (!(pCtx = tpCtxGetTSD())) {
        LOG_ERR("上下文不正确");
        return -1;
    }

    if (!(pDoc = (XMLDOC *)(pCtx->preBuf.ptr))) {
        LOG_WRN("XML尚未初始化");
        return -1;
    }

    if (argc == 2) {
        return newXMLNode_Noencoding(pDoc, argv[0].buf, argv[1].buf, NULL);
    } else if (argc == 3) {
        return newXMLNode_Noencoding(pDoc, argv[0].buf, argv[1].buf, argv[2].buf);
    } else {
        LOG_ERR("参数个数不正确");
        return -1;
    }
}

/***************************************************************
 * 设置XML节点文本
 **************************************************************/
static int tp_XMLTEXT(int argc, TPVAR argv[], char *result, int size)
{
    TPCTX *pCtx;
    XMLDOC *pDoc;

    /* argv[0]: 节点(xpath) */
    /* argv[1]: 文本值 */

    if (argc != 2) {
        LOG_ERR("参数个数不正确");
        return -1;
    }

    if (!(pCtx = tpCtxGetTSD())) {
        LOG_ERR("上下文不正确");
        return -1;
    }

    if (!(pDoc = (XMLDOC *)(pCtx->preBuf.ptr))) {
        LOG_WRN("XML尚未初始化");
        return -1;
    }
    return setXMLText(pDoc, argv[0].buf, argv[1].buf);
}

/***************************************************************
 * 设置XML节点文本(no encoding)
 **************************************************************/
static int tp_XMLTEXT_Noencoding(int argc, TPVAR argv[], char *result, int size)
{
    TPCTX *pCtx;
    XMLDOC *pDoc;

    /* argv[0]: 节点(xpath) */
    /* argv[1]: 文本值 */

    if (argc != 2) {
        LOG_ERR("参数个数不正确");
        return -1;
    }

    if (!(pCtx = tpCtxGetTSD())) {
        LOG_ERR("上下文不正确");
        return -1;
    }

    if (!(pDoc = (XMLDOC *)(pCtx->preBuf.ptr))) {
        LOG_WRN("XML尚未初始化");
        return -1;
    }
    return setXMLText_Noencoding(pDoc, argv[0].buf, argv[1].buf);
}

/***************************************************************
 * 设置XML节点属性
 **************************************************************/
static int tp_XMLPROP(int argc, TPVAR argv[], char *result, int size)
{
    TPCTX *pCtx;
    XMLDOC *pDoc;

    /* argv[0]: 节点(xpath) */
    /* argv[1]: 属性名 */
    /* argv[2]: 属性值 */

    if (argc != 3) {
        LOG_ERR("参数个数不正确");
        return -1;
    }

    if (!(pCtx = tpCtxGetTSD())) {
        LOG_ERR("上下文不正确");
        return -1;
    }

    if (!(pDoc = (XMLDOC *)(pCtx->preBuf.ptr))) {
        LOG_WRN("XML尚未初始化");
        return -1;
    }
    return setXMLProp(pDoc, argv[0].buf, argv[1].buf, argv[2].buf);
}

/***************************************************************
 * 设置XML节点属性
 **************************************************************/
static int tp_XMLPROP_Noencoding(int argc, TPVAR argv[], char *result, int size)
{
    TPCTX *pCtx;
    XMLDOC *pDoc;

    /* argv[0]: 节点(xpath) */
    /* argv[1]: 属性名 */
    /* argv[2]: 属性值 */

    if (argc != 3) {
        LOG_ERR("参数个数不正确");
        return -1;
    }

    if (!(pCtx = tpCtxGetTSD())) {
        LOG_ERR("上下文不正确");
        return -1;
    }

    if (!(pDoc = (XMLDOC *)(pCtx->preBuf.ptr))) {
        LOG_WRN("XML尚未初始化");
        return -1;
    }
    return setXMLProp_Noencoding(pDoc, argv[0].buf, argv[1].buf, argv[2].buf);
}


/***************************************************************
 * 取ISO8583报文域
 **************************************************************/
static int tp_ISO(int argc, TPVAR argv[], char *result, int size)
{
    int nId,len;
    TPCTX *pCtx;

    if (argc != 1) {
        LOG_ERR("参数个数不正确");
        return -1;
    }

    pCtx = tpCtxGetTSD();
    if (!pCtx || !pCtx->preBuf.ptr) {
        LOG_ERR("上下文不正确");
        return -1;
    }
    nId = atoi(argv[0].buf);
    if (nId <= 0 || nId > NMAXBITS) {
        LOG_ERR("域ID值[%d]不正确", nId);
        return -1;
    }
    if ((len = tpMsgCtxGet(pCtx->preBuf.ptr, nId-1, result, size)) < 0) {
        LOG_ERR("tpMsgCtxGet(%d) error", nId-1);
        return -1;
    }
    return len;
}

/***************************************************************
 * 取报文头域
 **************************************************************/
static int tp_HDR(int argc, TPVAR argv[], char *result, int size)
{
    TPCTX  *pCtx;
    TPHEAD *ptpHead;

    if (argc != 1) {
        LOG_ERR("参数个数不正确");
        return -1;
    }

    pCtx = tpCtxGetTSD();
    if (!pCtx) {
        LOG_ERR("上下文不正确");
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
        LOG_ERR("无报文头域[%s]", argv[0].buf);
        return -1;
    }
    return strlen(result);
}

/***************************************************************
 * 设置前置报文头长度
 **************************************************************/
static int tp_PREHDR_LEN(int argc, TPVAR argv[], char *result, int size)
{
    TPCTX *pCtx;

    if (argc != 1) {
        LOG_ERR("参数个数不正确");
        return -1;
    }

    pCtx = tpCtxGetTSD();
    if (!pCtx) {
        LOG_ERR("上下文不正确");
        return -1;
    }

    pCtx->tpMsg.head.bodyOffset = atoi(argv[0].buf);
    return 0;
}

/***************************************************************
 * 设置后置报文体长度
 **************************************************************/
static int tp_PREHDR_SET(int argc, TPVAR argv[], char *result, int size)
{
    TPCTX *pCtx;
    char buf[21];

    /*argv[0]: 偏移量*/
    /*argv[1]: 域长度*/
    /*argv[2]: 前置头长度*/

    if (argc != 3) {
        LOG_ERR("参数个数不正确");
        return -1;
    }

    pCtx = tpCtxGetTSD();
    if (!pCtx) {
        LOG_ERR("上下文不正确");
        return -1;
    }

    sprintf(buf, "%0*d", atoi(argv[1].buf), pCtx->tpMsg.head.bodyLen + atoi(argv[2].buf));
    memcpy(pCtx->tpMsg.body+atoi(argv[0].buf), buf, atoi(argv[1].buf));
    return 0;
}

/***************************************************************
 * 返回一个或多个相同字符组成的串
 **************************************************************/
static int tp_CHR(int argc, TPVAR argv[], char *result, int size)
{
    int c,len;

    if (argc < 1 || argc > 2) {
        LOG_ERR("参数个数不正确");
        return -1;    
    }
   
    c = atoi(argv[0].buf);
    if (c<0 || c>255) { /*十进制表示的ASCII码*/
        LOG_ERR("第1个参数值不正确");
        return -1;
    }

    len = (argc < 2)? 1:atoi(argv[1].buf);
    if (len <= 0) {
        LOG_ERR("第2个参数值不正确");
        return -1;
    }

    memset(result, c, len); 
    return len;
}

/***************************************************************
 * 返回一个或多个不同字符组成的串
 **************************************************************/
static int tp_STR(int argc, TPVAR argv[], char *result, int size)
{
    int i,c,len;

    if (argc < 1) {
        LOG_ERR("参数个数不正确");
        return -1;    
    }

    for (i=0,len=0; i<argc; i++) {
        if (argv[i].len <= 0) continue;
        c = atoi(argv[i].buf);
        if (c<0 || c>255) { /*十进制表示的ASCII码*/
            LOG_ERR("第%d个参数值不正确", i+1);
            return -1;
        }
        result[len++] = c;
    }
    return len;
}

/***************************************************************
 * 取参数值长度
 **************************************************************/
static int tp_LEN(int argc, TPVAR argv[], char *result, int size)
{
    if (argc != 1) {
        LOG_ERR("参数个数不正确");
        return -1;    
    }
    snprintf(result, size, "%d", argv[0].len);
    return strlen(result);
}

/***************************************************************
 * 去除左右空格
 **************************************************************/
static int tp_TRIM(int argc, TPVAR argv[], char *result, int size)
{
    int len;

    if (argc != 1) {
        LOG_ERR("参数个数不正确");
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
 * 去除左边空格
 **************************************************************/
static int tp_TRIML(int argc, TPVAR argv[], char *result, int size)
{
    int len;

    if (argc != 1) {
        LOG_ERR("参数个数不正确");
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
 * 去除右边空格
 **************************************************************/
static int tp_TRIMR(int argc, TPVAR argv[], char *result, int size)
{
    int len;

    if (argc != 1) {
        LOG_ERR("参数个数不正确");
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
 * 填充字符串
 **************************************************************/
static int tp_FILL(int argc, TPVAR argv[], char *result, int size)
{
    int len,tmpLen;

    /* argv[0]: 待填充串 */
    /* argv[1]: 填充字符 */
    /* argv[2]: 填充方式(L左填/R右填) */
    /* argv[3]: 填充后长度 */

    if (argc < 4) {
        LOG_ERR("参数个数不正确");
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
        LOG_ERR("第3个参数不正确");
        return -1;        
    }
    return len;
}

/***************************************************************
 * 连接多个字符串
 **************************************************************/
static int tp_CAT(int argc, TPVAR argv[], char *result, int size)
{
    int i,len,tmpLen;

    if (argc < 2) {
        LOG_ERR("参数个数不正确");
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
 * 取子串
 **************************************************************/
static int tp_MID(int argc, TPVAR argv[], char *result, int size)
{
    int len,offset;

    /* argv[0]: 原始串 */
    /* argv[1]: 偏移量 */
    /* argv[2]: 取多长 */

    if (argc != 3) {
        LOG_ERR("参数个数不正确");
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
 * 取左边子串
 **************************************************************/
static int tp_LEFT(int argc, TPVAR argv[], char *result, int size)
{
    /* argv[0]: 原始串 */
    /* argv[1]: 取多长 */

    int len;

    if (argc != 2) {
        LOG_ERR("参数个数不正确");
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
 * 取右边子串
 **************************************************************/
static int tp_RIGHT(int argc, TPVAR argv[], char *result, int size)
{
    int len,offset;

    /* argv[0]: 原始串 */
    /* argv[1]: 取多长 */

    if (argc != 2) {
        LOG_ERR("参数个数不正确");
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
 * 逻辑与
 **************************************************************/
static int tp_AND(int argc, TPVAR argv[], char *result, int size)
{
    int i;

    if (argc < 2) {
        LOG_ERR("参数个数不正确");
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
 * 逻辑或
 **************************************************************/
static int tp_OR(int argc, TPVAR argv[], char *result, int size)
{
    int i;

    if (argc < 2) {
        LOG_ERR("参数个数不正确");
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
 * 逻辑非
 **************************************************************/
static int tp_NOT(int argc, TPVAR argv[], char *result, int size)
{
    if (argc != 1) {
        LOG_ERR("参数个数不正确");
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
 * 比较两个数值是否相等
 **************************************************************/
static int tp_EQ(int argc, TPVAR argv[], char *result, int size)
{
    if (argc != 2) {
        LOG_ERR("参数个数不正确");
        return -1;    
    }

    result[0] = (atof(argv[0].buf) == atof(argv[1].buf))? 1:0;
    return 1;
}

/***************************************************************
 * 比较两个字符串是否相等
 **************************************************************/
static int tp_SEQ(int argc, TPVAR argv[], char *result, int size)
{
    if (argc != 2) {
        LOG_ERR("参数个数不正确");
        return -1;    
    }

    result[0] = (argv[0].len != argv[1].len || memcmp(argv[0].buf, argv[1].buf, argv[1].len))? 0:1;
    return 1;
}

/***************************************************************
 * 判断是否大于
 **************************************************************/
static int tp_GT(int argc, TPVAR argv[], char *result, int size)
{
    if (argc != 2) {
        LOG_ERR("参数个数不正确");
        return -1;    
    }

    result[0] = (atof(argv[0].buf) > atof(argv[1].buf))? 1:0;
    return 1;
}

/***************************************************************
 * 判断是否大于等于
 **************************************************************/
static int tp_GE(int argc, TPVAR argv[], char *result, int size)
{
    if (argc != 2) {
        LOG_ERR("参数个数不正确");
        return -1;    
    }

    result[0] = (atof(argv[0].buf) >= atof(argv[1].buf))? 1:0;   
    return 1;
}

/***************************************************************
 * 判断是否小于
 **************************************************************/
static int tp_LT(int argc, TPVAR argv[], char *result, int size)
{
    if (argc != 2) {
        LOG_ERR("参数个数不正确");
        return -1;    
    }

    result[0] = (atof(argv[0].buf) < atof(argv[1].buf))? 1:0;
    return 1;
}

/***************************************************************
 * 判断是否小于等于
 **************************************************************/
static int tp_LE(int argc, TPVAR argv[], char *result, int size)
{
    if (argc != 2) {
        LOG_ERR("参数个数不正确");
        return -1;    
    }

    result[0] = (atof(argv[0].buf) <= atof(argv[1].buf))? 1:0;
    return 1;
}

/***************************************************************
 * 两个数值相加
 **************************************************************/
static int tp_ADD(int argc, TPVAR argv[], char *result, int size)
{
    double f;

    /* argv[0]: */
    /* argv[1]: */
    /* argv[2]: 保留的小数位数(可选)*/

    if (argc < 2 || argc > 3) {
        LOG_ERR("参数个数不正确");
        return -1;    
    }

    f = atof(argv[0].buf) + atof(argv[1].buf);
    snprintf(result, size, "%.*lf", atoi(argv[2].buf), f);
    return strlen(result);
}

/***************************************************************
 * 两个数值相减
 **************************************************************/
static int tp_SUB(int argc, TPVAR argv[], char *result, int size)
{
    double f;

    /* argv[0]: */
    /* argv[1]: */
    /* argv[2]: 保留的小数位数(可选)*/

    if (argc < 2 || argc > 3) {
        LOG_ERR("参数个数不正确");
        return -1;    
    }

    f = atof(argv[0].buf) - atof(argv[1].buf);
    snprintf(result, size, "%.*lf", atoi(argv[2].buf), f);
    return strlen(result);
}

/***************************************************************
 * 两个数值相乘
 **************************************************************/
static int tp_MUL(int argc, TPVAR argv[], char *result, int size)
{
    double f;

    /* argv[0]: */
    /* argv[1]: */
    /* argv[2]: 保留的小数位数(可选)*/

    if (argc < 2 || argc > 3) {
        LOG_ERR("参数个数不正确");
        return -1;    
    }

    f = atof(argv[0].buf) * atof(argv[1].buf);
    snprintf(result, size, "%.*lf", atoi(argv[2].buf), f);
    return strlen(result);
}

/***************************************************************
 * 两个数值相除
 **************************************************************/
static int tp_DIV(int argc, TPVAR argv[], char *result, int size)
{
    double f;

    /* argv[0]: */
    /* argv[1]: */
    /* argv[2]: 保留的小数位数(可选)*/

    if (argc < 2 || argc > 3) {
        LOG_ERR("参数个数不正确");
        return -1;    
    }

    if (atof(argv[1].buf) == 0) {
        LOG_ERR("第2个参数值不正确");
        return -1;
    }

    f = atof(argv[0].buf) / atof(argv[1].buf);
    snprintf(result, size, "%.*lf", atoi(argv[2].buf), f);
    return strlen(result);
}

/***************************************************************
 * 条件赋值
 **************************************************************/
static int tp_IF(int argc, TPVAR argv[], char *result, int size)
{
    int len;

    if (argc != 3) {
        LOG_ERR("参数个数不正确");
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
 * 正则匹配
 **************************************************************/
static int tp_REG(int argc, TPVAR argv[], char *result, int size)
{
    /* argv[0]: 匹配字符串*/
    /* argv[1]: 正则表达式*/

    if (argc != 2) {
        LOG_ERR("参数个数不正确");
        return -1;    
    }

    result[0] = (regMatch(argv[0].buf, argv[1].buf))? 0:1;
    return 1;
}    

/***************************************************************
 * 取系统当前日期和时间
 **************************************************************/
static int tp_DATETIME(int argc, TPVAR argv[], char *result, int size)
{
    time_t t;

    /* argv[0]: 指定格式(可选) */

    if (argc > 1) {
        LOG_ERR("参数个数不正确");
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
 * 返回空值
 **************************************************************/
static int tp_NULL(int argc, TPVAR argv[], char *result, int size)
{
    return 0;
}

static int tp_KEYVAL_SET(int argc, TPVAR argv[], char *result, int size)
{
    TPCTX *pCtx;
    char buf[21];

    /*argv[0]: 偏移量*/
    /*argv[1]: KEY值*/

    if (argc != 2) {
        LOG_ERR("参数个数不正确");
        return -1;
    }

    pCtx = tpCtxGetTSD();
    if (!pCtx) {
        LOG_ERR("上下文不正确");
        return -1;
    }
/*
    sprintf(buf, "%0*d", atoi(argv[1].buf), pCtx->tpMsg.head.bodyLen + atoi(argv[2].buf));
*/
    memcpy(pCtx->tpMsg.body+atoi(argv[0].buf), argv[1].buf,argv[1].len);
    return 0;
}

