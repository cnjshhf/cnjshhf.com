/***************************************************************
 * ��ʽת����������
 **************************************************************/
#include "tpbus.h"

#define B2D(x)  (x/16*10+x%16)
#define D2B(x)  (x/10*16+x%10)

/***************************************************************
 * ��ձ��Ĳ�����
 **************************************************************/
int tpMsgCtxClear(MSGCTX *ctx)
{
    int i;
 
    for (i=0; i<MAXFLDNUM; i++) {
        ctx->flds[i].name[0] = 0;
        ctx->flds[i].offset = 0;
        ctx->flds[i].length = 0;
        ctx->flds[i].size = 0;
        ctx->flds[i].next = 0;
    }
    ctx->free = HASH_SIZE+1;
    ctx->tail = 0;
    return 0;
}

/***************************************************************
 * strcmp_IgnoreCase
 **************************************************************/
static int strcmp_IgnoreCase(char *s1, char*s2)
{
    int i,len; 

    if (strlen(s1) != (len = strlen(s2))) return -1;
    for (i=0; i<len; i++) {
        if (toupper(s1[i]) != toupper(s2[i])) return -1;
    }
    return 0;
}    

/***************************************************************
 * ��ȡ����������(���鷽ʽ)
 **************************************************************/
int tpMsgCtxGet(MSGCTX *ctx, int i, char *buf, int size)
{
    int len;

    len = (size < ctx->flds[i].length)? size : ctx->flds[i].length;
    memcpy(buf, ctx->buf+ctx->flds[i].offset, len);
    return len;
}

/***************************************************************
 * ��ȡ����������(HASH��ʽ)
 **************************************************************/
int tpMsgCtxHashGet(MSGCTX *ctx, char *key, char *buf, int size)
{
    int j;
    
    j = hashValue(key, HASH_SIZE)+1;
    while (j > 0 && strcmp_IgnoreCase(key, ctx->flds[j-1].name)) j = ctx->flds[j-1].next;
    if (j <= 0) return 0; /*Key not exist*/
    return tpMsgCtxGet(ctx, j-1, buf, size);
}

/***************************************************************
 * ���벢��������(���鷽ʽ)
 **************************************************************/
int tpMsgCtxPut(MSGCTX *ctx, int i, char *buf, int len)
{
    if (!buf) len = 0;
    if (ctx->flds[i].size > 0) {
        if (len <= ctx->flds[i].size) {
            ctx->flds[i].length = len;
            memcpy(ctx->buf+ctx->flds[i].offset, buf, len);
            return 0;
        }
    }
    if (MAXCTXLEN < ctx->tail+len) {
        LOG_ERR("��������ֵ�洢�ռ䲻��");
        return -1;
    }
    ctx->flds[i].size = len;
    ctx->flds[i].offset = ctx->tail;
    ctx->flds[i].length = len;
    memcpy(ctx->buf+ctx->flds[i].offset, buf, len);
    ctx->tail += len;
    return 0;
}

/***************************************************************
 * д�벢��������(HASH��ʽ)
 **************************************************************/
int tpMsgCtxHashPut(MSGCTX *ctx, char *key, char *buf, int len)
{
    int i,t;

    i = hashValue(key, HASH_SIZE)+1;
    if (ctx->flds[i-1].name[0]) {
        while (i > 0 && strcmp_IgnoreCase(key, ctx->flds[i-1].name)) { /*Find same key*/
            if (ctx->flds[i-1].next) {
                i = ctx->flds[i-1].next;
            } else { /*No same key be found*/
                t = i;
                i = ctx->free;
                while (ctx->flds[i-1].name[0]) {
                    if (++i > MAXFLDNUM) i = HASH_SIZE+1;
                    if (i == ctx->free) {
                        LOG_ERR("�����������ռ�����");
                        return -1;
                    }
                }
                ctx->flds[t-1].next = i;
                if ((ctx->free = i+1) > MAXFLDNUM) ctx->free = HASH_SIZE+1;
                break;
            }
        }
    }
    if (!ctx->flds[i-1].name[0]) { /*New key*/
        strncpy(ctx->flds[i-1].name, key, MAXKEYLEN);
        ctx->flds[i-1].next = 0;
    }
    if (!buf) len = 0;
    if (ctx->flds[i-1].size > 0) {
        if (len <= ctx->flds[i-1].size) {
            ctx->flds[i-1].length = len;
            memcpy(ctx->buf+ctx->flds[i-1].offset, buf, len);
            return 0;
        }
    }
    if (MAXCTXLEN < ctx->tail+len) {
        LOG_ERR("��������ֵ�洢�ռ䲻��");
        return -1;
    }
    ctx->flds[i-1].size = len;
    ctx->flds[i-1].offset = ctx->tail;
    ctx->flds[i-1].length = len;
    memcpy(ctx->buf+ctx->flds[i-1].offset, buf, len);
    ctx->tail += len;
    return 0;
}

/***************************************************************
 * ���䲢����ռ�(���鷽ʽ)
 **************************************************************/
int tpMsgCtxAlloc(MSGCTX *ctx, int i, int size)
{
    if (size < ctx->flds[i].size) return -1;
    if (MAXCTXLEN < ctx->tail+size) {
        LOG_ERR("��������ֵ�洢�ռ䲻��");
        return -1;
    }
    if (ctx->flds[i].length > 0) {
        memcpy(ctx->buf+ctx->tail, ctx->buf+ctx->flds[i].offset, ctx->flds[i].length); 
    }
    ctx->flds[i].size = size;
    ctx->flds[i].offset = ctx->tail;
    ctx->tail += size;
    return 0;
}

/***************************************************************
 * ���䲢����ռ�(HASH��ʽ)
 **************************************************************/
int tpMsgCtxHashAlloc(MSGCTX *ctx, char *key, int size)
{
    int i,t;

    i = hashValue(key, HASH_SIZE)+1;
    if (ctx->flds[i-1].name[0]) {
        while (i > 0 && strcmp_IgnoreCase(key, ctx->flds[i-1].name)) { /*Find same key*/
            if (ctx->flds[i-1].next) {
                i = ctx->flds[i-1].next;
            } else { /*No same key be found*/
                t = i;
                i = ctx->free;
                while (ctx->flds[i-1].name[0]) {
                    if (++i > MAXFLDNUM) i = HASH_SIZE+1;
                    if (i == ctx->free) {
                        LOG_ERR("�����������ռ�����");
                        return -1;
                    }
                }
                ctx->flds[t-1].next = i;
                if ((ctx->free = i+1) > MAXFLDNUM) ctx->free = HASH_SIZE+1;
                break;
            }
        }
    }
    if (!ctx->flds[i-1].name[0]) { /*New key*/
        strncpy(ctx->flds[i-1].name, key, MAXKEYLEN);
        ctx->flds[i-1].next = 0;
    }
    if (size < ctx->flds[i-1].size) return -1;
    if (MAXCTXLEN < ctx->tail+size) {
        LOG_ERR("��������ֵ�洢�ռ䲻��");
        return -1;
    }
    if (ctx->flds[i-1].length > 0) {
        memcpy(ctx->buf+ctx->tail, ctx->buf+ctx->flds[i-1].offset, ctx->flds[i-1].length); 
    }
    ctx->flds[i-1].size = size;
    ctx->flds[i-1].offset = ctx->tail;
    ctx->tail += size;
    return 0;
}

/***************************************************************
 * �����Ĵ��
 **************************************************************/
int tpMsgCtxPack(MSGCTX *ctx, char *msg, int size)
{
    int  i,j,len = 0;
    int  keyLen,tmpLen;
    char buf[MAXVALLEN];

    for (i=0; i<HASH_SIZE; i++) {
        if (!ctx->flds[i].name[0]) continue;
        if ((tmpLen = tpMsgCtxGet(ctx, i, buf, sizeof(buf))) > 0) {
            keyLen = strlen(ctx->flds[i].name);
            if (size < keyLen+tmpLen+3) return len;
            memcpy(msg+len, ctx->flds[i].name, keyLen);
            len += keyLen;
            msg[len++] = 0;
            msg[len++] = tmpLen / 255;
            msg[len++] = tmpLen % 255;
            memcpy(msg+len, buf, tmpLen);
            len += tmpLen;
        }
        j = ctx->flds[i].next;
        while (j>0) {
            if ((tmpLen = tpMsgCtxGet(ctx, j-1, buf, sizeof(buf))) > 0) {
                keyLen = strlen(ctx->flds[j-1].name);
                if (size < keyLen+tmpLen+3) return len;
                memcpy(msg+len, ctx->flds[j-1].name, keyLen);
                len += keyLen;
                msg[len++] = 0;
                msg[len++] = tmpLen / 255;
                msg[len++] = tmpLen % 255;
                memcpy(msg+len, buf, tmpLen);
                len += tmpLen;
            }
            j = ctx->flds[j-1].next;
        }
    }
    return len;
}

/***************************************************************
 * �����Ľ��
 **************************************************************/
int tpMsgCtxUnpack(MSGCTX *ctx, char *msg, int len)
{
    char *key;
    int tmpLen,offset=0;

    tpMsgCtxClear(ctx);
    while (offset < len) {
        key = msg+offset;
        while (offset <=len && msg[offset++]) {}
        if (len < offset+2) return -1;
        tmpLen = ((unsigned char)msg[offset])*255 + ((unsigned char)msg[offset+1]);
        offset += 2;
        if (tmpLen <= 0) continue;
        if (len < offset+tmpLen) return -1;
        if (tpMsgCtxHashPut(ctx, key, msg+offset, tmpLen) != 0) return -1;
        offset += tmpLen;
    }
    return 0;
}

/***************************************************************
 * �����ϣֵ
 **************************************************************/
unsigned long hashValue(const char *key, int size)
{
    char ch;
    unsigned long value = 0L;

    if (key) {
        value += 30 * toupper(*key);
        while ((ch = toupper(*key++)) != 0) value = value ^ ((value << 5) + (value >> 3) + (unsigned long)ch);
    }
    return (value % size);
}

/***************************************************************
 * ASCII��->BCD��
 **************************************************************/
void ASC2BCD(char *bbuf, char *abuf, int len)
{
    int ch,flag=0;

    if (len%2) flag = 1;
    for (; len>0; len--, abuf++) {
        ch = *abuf;
        if (!isxdigit(ch)) {
            memset(bbuf, 0, (len+1)/2);
            return;
        }
        if (ch > '9') ch += 9;
        if (flag) {
            *bbuf = (unsigned char)((*bbuf & 0xF0) | (ch & 0x0F));
            bbuf++;
        } else {
            *bbuf = (unsigned char)((*bbuf & 0x0F) | ((ch & 0x0F) << 4));
        }
        flag ^= 1;
    }
}

/***************************************************************
 * BCD��->ASCII��
 **************************************************************/
void BCD2ASC(char *abuf, char *bbuf, int len)
{
    int i;

    for (i=0; i<len; i++) {
        abuf[i*2] = (unsigned char)bbuf[i]/16 + '0';
        if (abuf[i*2] > '9') abuf[i*2] = (unsigned char)abuf[i*2] + 7;
        abuf[i*2+1] = (unsigned char)bbuf[i]%16 + '0';
        if (abuf[i*2+1] > '9') abuf[i*2+1] = (unsigned char)abuf[i*2+1] + 7;
    }
}

/***************************************************************
 * ��ȡTLV���
 **************************************************************/
int tlvGetTag(char *buf, int size, char *tag)
{
    unsigned char *p = (unsigned char *)buf;
   
    if (size < 1) return -1;
    tag[0] = p[0];
    if ((p[0] & 0x1F) != 0x1F) return 1;
    if (size < 2) return -1;
    tag[1] = p[1];
    return 2;
}

/***************************************************************
 * ����TLV���
 **************************************************************/
int tlvSetTag(char *buf, int size, char *tag)
{
    int nBytes = 0;
    unsigned char ch;
    unsigned char *pBuf = (unsigned char *)buf;
    unsigned char *pTag = (unsigned char *)tag;

    while (*pTag) {
        ch = 0;
        if (size < nBytes+1) return -1;

        if (*pTag >= '0' && *pTag <= '9') {
            ch = *pTag - '0';
        } else if (*pTag >= 'a' && *pTag <= 'f') {
            ch = *pTag - 'a' + 10;
        } else if (*pTag >= 'A' && *pTag <= 'F') {
            ch = *pTag - 'A' + 10;
        } else {
            return -1;
        }
        pBuf[nBytes] = (ch << 4);
        pTag++;
        if (!(*pTag)) return -1;

        if (*pTag >= '0' && *pTag <= '9') {
            ch = *pTag - '0';
        } else if (*pTag >= 'a' && *pTag <= 'f') {
            ch = *pTag - 'a' + 10;
        } else if (*pTag >= 'A' && *pTag <= 'F') {
            ch = *pTag - 'A' + 10;
        } else {
            return -1;
        }
        pBuf[nBytes] |= ch;
        pTag++;
        nBytes++;
    }
    return nBytes;
}

/***************************************************************
 * ��ȡTLV����
 **************************************************************/
int tlvGetLen(char *buf, int size, int *len)
{
    unsigned char *p = (unsigned char *)buf;

    if (size < 1) return -1;
    if (!(p[0] & 0x80)) {
        *len = p[0];
        return 1;
    }
    switch (p[0] & 0x7F) {
        case 1:
            if (size < 2) return -1;
            *len = p[1];
            return 2;
        case 2:
            if (size < 3) return -1;
            *len = p[1]*256+p[2];
            return 3;
        default:
            return -1;
    }
}

/***************************************************************
 * ����TLV����
 **************************************************************/
int tlvSetLen(char *buf, int size, int len)
{
    int nBytes = 0;
    unsigned char *p = (unsigned char *)buf;

    if (len < 128) {
        if (size < 1) return -1;
        p[nBytes++] = (unsigned char)len;
    } else if (len >= 128 && len <= 255) {
        if (size < 2) return -1;
        p[nBytes++] = 0x81;
        p[nBytes++] = (unsigned char)len;
    } else {
        if (size < 3) return -1;
        p[nBytes++] = 0x82;
        p[nBytes++] = (unsigned char)(len / 256);
        p[nBytes++] = (unsigned char)(len % 256);
    }
    return nBytes;
}

/***************************************************************
 * ȡTLV�����
 **************************************************************/
int tlvGet(char *buf, int len, char *tag, char *val, int size)
{
    char _tag[3];
    int ret,tmpLen,offset=0;

    while (offset < len) {
        if ((ret = tlvGetTag(buf+offset, len-offset, _tag)) == -1) {
            LOG_ERR("tlvGetTag() error");
            return -1;
        }
        offset += ret;
        _tag[ret] = 0;

        if ((ret = tlvGetLen(buf+offset, len-offset, &tmpLen)) == -1) {
            LOG_ERR("tlvGetLen() error");
            return -1;
        }
        offset += ret;

        if (tmpLen > len-offset) {
            LOG_ERR("TLV���[%s]��ֵ��ȫ", _tag);
            return -1;
        }

        if (strcmp(tag, _tag) == 0) {
            if (size < tmpLen) {
                LOG_ERR("TLV���[%s]��ֵ����", _tag);
                return -1;
            }
            memcpy(val, buf+offset, tmpLen);
            return tmpLen;
        }
	offset += tmpLen;
    }
    return 0; /*��ǲ�����*/
}

/***************************************************************
 * ׷��TLV�����
 **************************************************************/
int tlvCat(char *buf, int size, char *tag, char *val, int len)
{
    int ret,offset=0;

    if ((ret = tlvSetTag(buf+offset, size-offset, tag)) == -1) {
        LOG_ERR("tlvSetTag(%s) error", tag);
        return -1;
    }
    offset += ret;

    if ((ret = tlvSetLen(buf+offset, size-offset, len)) == -1) {
        LOG_ERR("tlvSetLen(%s,%d) error", tag, len);
        return -1;
    }
    offset += ret;

    if (size-offset < len) {
        LOG_ERR("TLV���ݹ���");
        return -1;
    }
    memcpy(buf+offset, val, len);
    offset += ret;
    return offset;
}

/***************************************************************
 * CSV���Ľ��
 **************************************************************/
int tpCSVUnpack(MSGCTX *ctx, char *msg, int len, FMTPROP *prop)
{
    int  offset=0,start=0;
    int  gid=0,rid=0,fid=0;
    int  tmpLen;
    char key[MAXKEYLEN+1];

    while (offset < len) {
        if (prop->csv.gsLen > 0 && memcmp(msg+offset, prop->csv.gsTag, prop->csv.gsLen) == 0) {
            tmpLen = offset - start;
            snprintf(key, sizeof(key), "%d.%d.%d", gid, rid, fid);
            if (tpMsgCtxHashPut(ctx, key, msg+start, tmpLen) != 0) {
                LOG_ERR("tpMsgCtxHashPut(%s) error", key);
                return -1;
            }
            offset += prop->csv.gsLen;
            start = offset;
            gid++;
            rid = 0;
            fid = 0;
        } else if (prop->csv.rsLen > 0 && memcmp(msg+offset, prop->csv.rsTag, prop->csv.rsLen) == 0) {
            tmpLen = offset - start;
            snprintf(key, sizeof(key), "%d.%d.%d", gid, rid, fid);
            if (tpMsgCtxHashPut(ctx, key, msg+start, tmpLen) != 0) {
                LOG_ERR("tpMsgCtxHashPut(%s) error", key);
                return -1;
            }
            offset += prop->csv.rsLen;
            start = offset;
            rid++;
            fid = 0;
        } else if (prop->csv.fsLen > 0 && memcmp(msg+offset, prop->csv.fsTag, prop->csv.fsLen) == 0) {
            tmpLen = offset - start;
            snprintf(key, sizeof(key), "%d.%d.%d", gid, rid, fid);
            if (tpMsgCtxHashPut(ctx, key, msg+start, tmpLen) != 0) {
                LOG_ERR("tpMsgCtxHashPut(%s) error", key);
                return -1;
            }
            offset += prop->csv.fsLen;
            start = offset;
            fid++;
        } else {
            offset++;
        }
    }
    if ((tmpLen = offset - start) > 0) {
        snprintf(key, sizeof(key), "%d.%d.%d", gid, rid, fid);
        if (tpMsgCtxHashPut(ctx, key, msg+start, tmpLen) != 0)
        {
            LOG_ERR("tpMsgCtxHashPut(%s) error", key);
            return -1;
        }
    }
    return 0;
}

/***************************************************************
 * XML���Ľ��
 **************************************************************/
int tpXMLUnpack(XMLDOC **ppDoc, char *msg, int len, FMTPROP *prop)
{
    msg[len] = 0;
    return ((*ppDoc = loadXMLString(msg)) != NULL)? 0:-1;
}

/***************************************************************
 * XML���Ĵ��
 **************************************************************/
int tpXMLPack(XMLDOC *pDoc, char *msg, int size, FMTPROP *prop)
{
    return saveXMLString(pDoc, msg, size, 0);
}

/***************************************************************
 * bitmapSet
 **************************************************************/
static void bitmapSet(int i, char *bitmap)
{
    unsigned char *p = (unsigned char *)bitmap;

    p[(i-1)/8] |= (0x01<<(7-(i-1)%8));
}

/***************************************************************
 * bitmapIsOn
 **************************************************************/
static int bitmapIsOn(int i, const char *bitmap)
{
    unsigned char *p = (unsigned char *)bitmap;

    return ((p[(i-1)/8]>>(7-(i-1)%8))&0x01);
}

/***************************************************************
 * fillCopy
 **************************************************************/
static int fillCopy(char *msg, const char *buf, int len, int len2, int flag)
{
    switch (flag)
    {
        case LF_ZERO:
            memset(msg, '0', len2-len);
            memcpy(msg+len2-len, buf, len);
            break;
        case RF_ZERO:
            memcpy(msg, buf, len);
            memset(msg+len, '0', len2-len);
            break;
        case LF_SPACE:
            memset(msg, ' ', len2-len);
            memcpy(msg+len2-len, buf, len);
            break;
        case RF_SPACE:
            memcpy(msg, buf, len);
            memset(msg+len, ' ', len2-len);
            break;
        default:
            LOG_ERR("��䷽ʽ�޷�ʶ��");
            return -1;
    }
    return 0;
}

/***************************************************************
 * iso8583Get
 **************************************************************/
static int iso8583Get(char *msg, char *buf, int *pLen, int *pOffset, BITMAP_ITEM *bitmap)
{
    int nLVAR;

    switch (bitmap->fldType)
    {
        case Efb:
            *pLen = bitmap->fldLen/8;
            memcpy(buf, msg, *pLen);
            *pOffset = *pLen;
            break;

        case Efn:
            if (bitmap->bcdFlag)
            {
                *pLen = bitmap->fldLen;
                BCD2ASC(msg, buf, *pLen);
                *pOffset = (*pLen+1)/2;
            } else {
                *pLen = bitmap->fldLen;
                memcpy(buf, msg, *pLen);
                *pOffset = *pLen;
            }
            break;
    
        case Efa:
        case Efs:
        case Efan:
        case Efas:
        case Efns:
        case Efans:
        case Efx:
            *pLen = bitmap->fldLen;
            memcpy(buf, msg, *pLen);
            *pOffset = *pLen;
            break;

        case Eva:
        case Evn:
        case Evs:
        case Evan:
        case Evas:
        case Evns:
        case Evans:
        case Evz:
            if (BCDLEN == bitmap->lenType)
            {
                if (bitmap->lenLen < 3) /*LVAR,LLVAR*/
                {
                    *pLen = B2D(msg[0]);
                    nLVAR = 1;
                } else {                /*LLLVAR*/
                    *pLen = B2D(msg[0]);
                    *pLen = *pLen*100 + B2D(msg[1]);
                    nLVAR = 2;
                }
            }  else if (HEXLEN == bitmap->lenType) {
                if (bitmap->lenLen < 3) /*LVAR,LLVAR*/
                {
                    *pLen = msg[0];
                    nLVAR = 1;
                } else {                /*LLLVAR*/
                    *pLen = msg[0]*16*16 + msg[1];
                    nLVAR = 2;
                }
            } else { /*ASCLEN*/
                if (bitmap->lenLen < 2) /*LVAR*/
                {
                    *pLen = msg[0]-'0';
                    nLVAR = 1;
                } else if (bitmap->lenLen < 3) { /*LLVAR*/
                    *pLen = msg[0]-'0';
                    *pLen = *pLen*10 + msg[1]-'0';
                    nLVAR = 2;
                } else { /*LLLVAR*/
                    *pLen = msg[0]-'0';
                    *pLen = *pLen*10 + msg[1]-'0';
                    *pLen = *pLen*10 + msg[2]-'0';
                    nLVAR = 3;
                }
            }
            if (*pLen < 0 || *pLen > bitmap->fldLen)
            {
                LOG_ERR("��[%d]����[%d]����ȷ", bitmap->fldId, *pLen);
                return -1;
            }
            if (Evn == bitmap->fldType || Evz == bitmap->fldType)
            {
                if (bitmap->bcdFlag)
                {
                    BCD2ASC(msg+nLVAR, buf, *pLen);
                    *pOffset = nLVAR+(*pLen+1)/2;
                } else {
                    memcpy(buf, msg+nLVAR, *pLen);
                   *pOffset = nLVAR+*pLen;
                }
            } else {
                memcpy(buf, msg+nLVAR, *pLen);
                *pOffset = nLVAR+*pLen;
            }
            break;
    
        default:
            LOG_ERR("�����Ͷ��岻��ȷ");
            return -1;
    }
    return 0;
}

/***************************************************************
 * iso8583Set
 **************************************************************/
static int iso8583Set(char *msg, char *buf, int len, int *pOffset, BITMAP_ITEM *bitmap)
{
    int  nLVAR;
    char tmpBuf[MAXVALLEN+1];

    switch (bitmap->fldType)
    {
        case Efb:
            if (len*8 != bitmap->fldLen)
            {
                LOG_ERR("��[%d]����[%d]����ȷ", bitmap->fldId, len*8);
                return -1;
            }
            memcpy(msg, buf, len);
            *pOffset = len;
            break;

        case Efn:
            if (len < bitmap->fldLen)
            {
                fillCopy(tmpBuf, buf, len, bitmap->fldLen, bitmap->fillType);
                if (bitmap->bcdFlag)
                {
                    ASC2BCD(msg, tmpBuf, bitmap->fldLen);
                    *pOffset = (bitmap->fldLen+1)/2;
                } else {
                    memcpy(msg, tmpBuf, bitmap->fldLen);
                   *pOffset = bitmap->fldLen;
                }
            } else {
                if (bitmap->bcdFlag)
                {
                    ASC2BCD(msg, buf, bitmap->fldLen);
                    *pOffset = (bitmap->fldLen+1)/2;
                } else {
                    memcpy(msg, buf, bitmap->fldLen);
                    *pOffset = bitmap->fldLen;
                }
            }
            break;
    
        case Efa:
        case Efs:
        case Efan:
        case Efas:
        case Efns:
        case Efans:
        case Efx:
            if (len < bitmap->fldLen) 
            {
               fillCopy(msg, buf, len, bitmap->fldLen, bitmap->fillType);
            } else {
               memcpy(msg, buf, bitmap->fldLen);
            }
            *pOffset = bitmap->fldLen;
            break;

        case Eva:
        case Evn:        
        case Evs:
        case Evan:
        case Evas:
        case Evns:
        case Evans:
        case Evz:
            if (len > bitmap->fldLen) len = bitmap->fldLen;
            if (BCDLEN == bitmap->lenType)
            {
                if (bitmap->lenLen < 3) /*LVAR,LLVAR*/
                {
                    msg[0] = D2B(len);
                    nLVAR = 1;
                } else {                /*LLLVAR*/
                    msg[0] = D2B(len/100);
                    msg[1] = D2B(len%100);
                    nLVAR = 2;
                }
            }  else if (HEXLEN == bitmap->lenType) {
                if (bitmap->lenLen < 3) /*LVAR,LLVAR*/
                {
                    msg[0] = len;
                    nLVAR = 1;
                } else {                /*LLLVAR*/
                    msg[0] = len/(16*16);
                    msg[1] = len%(16*16);
                    nLVAR = 2;
                }
            } else { /*ASCLEN*/
                if (bitmap->lenLen < 2) /*LVAR*/
                {
                    msg[0] = len+'0';
                    nLVAR = 1;
                } else if (bitmap->lenLen < 3) { /*LLVAR*/
                    msg[0] = len/10+'0';
                    msg[1] = len%10+'0';
                    nLVAR = 2;
                } else { /*LLLVAR*/
                    msg[0] = len/100+'0';
                    msg[1] = (len-len/100*100)/10+'0';
                    msg[2] = len%10+'0';
                    nLVAR = 3;
                }
            }
            
            if (Evn == bitmap->fldType || Evz == bitmap->fldType)
            {
                if (bitmap->bcdFlag)
                {
                    ASC2BCD(msg+nLVAR, buf, len);
                    *pOffset = nLVAR+(len+1)/2;
                } else {
                    memcpy(msg+nLVAR, buf, len);
                    *pOffset = nLVAR+len;
                }
            } else {
                memcpy(msg+nLVAR, buf, len);
                *pOffset = nLVAR+len;
            }
            break;

        default:
            LOG_ERR("�����Ͷ��岻��ȷ");
            return -1;
    }
    return 0;
}

/***************************************************************
 * ISO8583���Ľ��
 **************************************************************/
int tpISO8583Unpack(MSGCTX *ctx, char *msg, int len, FMTPROP *prop)
{
    int  i,offset,nBits;
    int  tmpLen;
    char tmpBuf[MAXVALLEN+1];
    char *mp,*p=msg;
    BITMAP *pBitmap;

    if (bitmapShmGet(prop->iso8583.bitmap, &pBitmap) <= 0) {
        LOG_ERR("ISO8583λͼ[id=%s]δ����", prop->iso8583.bitmap);
        return -1;
    }

    /*msgtype*/
    if (iso8583Get(p, tmpBuf, &tmpLen, &offset, &pBitmap->items[0]) != 0) {
        LOG_ERR("iso8583Get(msgtype) error");
        return -1;
    }
    p += offset;

    if (tpMsgCtxPut(ctx, 0, tmpBuf, tmpLen) != 0) {
        LOG_ERR("tpMsgCtxPut(0) error");
        return -1;
    }

    /*λͼ*/
    mp = p;
    nBits = 64;
    if (mp[0] & 0x80) {
        nBits += 64;
        if (pBitmap->nBits > 128 && (mp[8] & 0x80)) {
            nBits += 64;
            if (pBitmap->nBits > 192 && (mp[16] & 0x80)) nBits += 64;
        }
    }
    p += nBits/8;

    /*��λͼ��ȡ��*/
    for (i=2; i <= nBits; i++) {
        if (i==65  && nBits>128) continue;
        if (i==129 && nBits>192) continue;
        if (!bitmapIsOn(i, mp)) continue;

        if ((p-msg) > len) {
            LOG_ERR("ISO8583���Ĳ�����");
            return -1;
        }

        if (iso8583Get(p, tmpBuf, &tmpLen, &offset, &pBitmap->items[i-1]) != 0) {
            LOG_ERR("iso8583Get(��[%d]) error", i);
            return -1;
        }
        p += offset;

        if (tpMsgCtxPut(ctx, i-1, tmpBuf, tmpLen) != 0) {
            LOG_ERR("tpMsgCtxPut(%d) error", i-1);
            return -1;
        }
    }

    /*MACУ��*/
    if (prop->iso8583.macFun[0]) {
        if ((tmpLen = tpFunExec(prop->iso8583.macFun, tmpBuf, sizeof(tmpBuf))) < 0) {
            LOG_ERR("������ʽ����");
            return -1;
        }
        if (!tmpBuf[0]) {
            LOG_ERR("MACУ�����");
            return -1;
        }
    }
    return 0;
}

/***************************************************************
 * ISO8583���Ĵ��
 **************************************************************/
int tpISO8583Pack(MSGCTX *ctx, char *msg, int size, FMTPROP *prop)
{
    int  i,offset,nBits;
    int  tmpLen;
    char tmpBuf[MAXVALLEN+1];
    char *mp,*p=msg;
    BITMAP *pBitmap;

    if (bitmapShmGet(prop->iso8583.bitmap, &pBitmap) <= 0) {
        LOG_ERR("ISO8583λͼ[id=%s]δ����", prop->iso8583.bitmap);
        return -1;
    }

    /*msgtype*/
    if ((tmpLen = tpMsgCtxGet(ctx, 0, tmpBuf, sizeof(tmpBuf))) < 0) {
        LOG_ERR("tpMsgCtxGet(0) error");
        return -1;
    }
    if (iso8583Set(p, tmpBuf, tmpLen, &offset, &pBitmap->items[0]) != 0) {
        LOG_ERR("iso8583Set(msgtype) error");
        return -1;
    }
    p += offset;

    /*λͼ*/
    mp = p;
    memset(mp, 0, NMAXBITS/8);
    
    nBits = 64;
    if (ctx->free > 64) {
        nBits += 64;
        bitmapSet(1, mp);
    }
    if (ctx->free > 128) {
        nBits += 64;
        bitmapSet(65, mp);
    }
    if (ctx->free > 192) {
        nBits += 64;
        bitmapSet(129, mp);
    }
    p += nBits/8;

    /*��λͼ������*/
    for (i=2; i<=ctx->free; i++) {
        if (ctx->flds[i-1].length <= 0) continue;
        if ((tmpLen = tpMsgCtxGet(ctx, i-1, tmpBuf, sizeof(tmpBuf))) < 0) {
            LOG_ERR("tpMsgCtxGet(%d) error", i-1);
            return -1;
        }
        if (iso8583Set(p, tmpBuf, tmpLen, &offset, &pBitmap->items[i-1]) != 0) {
            LOG_ERR("iso8583Set(��[%d]) error", i);
            return -1;
        }
        bitmapSet(i, mp);
        p += offset;
    }
    return (p-msg);
}

/***************************************************************
 * ����Ԥ���
 **************************************************************/
int tpPreUnpack(TPCTX *pCtx, FMTDEFS *fmtDefs)
{
    int  msgLen;
    char *pMsg,tmpBuf[MAXVALLEN+1];

    pCtx->preBuf.fmtType = fmtDefs->fmtType;

    if (fmtDefs->unpkFun[0]) {
        if (tpFunExec(fmtDefs->unpkFun, tmpBuf, sizeof(tmpBuf)) <0) {
            LOG_ERR("ִ�н��ǰ����������");
            return -1;
        }
    }
    if (pCtx->preBuf.fmtType == MSG_CUSTOM) {
        strcpy(pCtx->preBuf.freeFun, fmtDefs->freeFun);
        return 0;
    }

    pMsg = pCtx->tpMsg.body + pCtx->tpMsg.head.bodyOffset;
    msgLen = pCtx->tpMsg.head.bodyLen - pCtx->tpMsg.head.bodyOffset;

    switch (pCtx->preBuf.fmtType) {
        case MSG_NVL:
        case MSG_TLV:
            pCtx->preBuf.ptr = NULL;
            break;
        case MSG_CSV:
            tpMsgCtxClear(&pCtx->preBuf.buf);
            pCtx->preBuf.ptr = &pCtx->preBuf.buf;
            if (tpCSVUnpack(pCtx->preBuf.ptr, pMsg, msgLen, &fmtDefs->fmtProp) != 0)
            {
                LOG_ERR("tpCSVUnpack() error");
                return -1;
            }
            break;
        case MSG_FML:
            tpMsgCtxClear(&pCtx->preBuf.buf);
            pCtx->preBuf.ptr = &pCtx->preBuf.buf;
            if (tpMsgCtxUnpack(pCtx->preBuf.ptr, pMsg, msgLen) != 0)
            {
                LOG_ERR("tpMsgCtxUnpack() error");
                return -1;
            }
            break;
        case MSG_XML:
            if (tpXMLUnpack((XMLDOC **)&pCtx->preBuf.ptr, pMsg, msgLen, &fmtDefs->fmtProp) != 0)
            {
                LOG_ERR("tpXMLUnpack() error");
                return -1;
            }
            break;
        case MSG_ISO8583:
            tpMsgCtxClear(&pCtx->preBuf.buf);
            pCtx->preBuf.ptr = &pCtx->preBuf.buf;
            if (tpISO8583Unpack(pCtx->preBuf.ptr, pMsg, msgLen, &fmtDefs->fmtProp) != 0)
            {
                LOG_ERR("tpISO8583Unpack() error");
                return -1;
            }
            break;
        default:
            LOG_ERR("���ĸ�ʽ�޷�ʶ��");
            return -1;
    }
    return 0;
}

/***************************************************************
 * ��ʼ��Ԥ�ó�
 **************************************************************/
int tpPreBufInit(PREBUF *preBuf, FMTDEFS *fmtDefs)
{
    if (!fmtDefs) {
        preBuf->ptr = NULL;
        preBuf->offset = 0;
        preBuf->fmtType = 0;
        preBuf->freeFun[0] = 0;
        return 0;
    }

    preBuf->fmtType = fmtDefs->fmtType;
    switch (preBuf->fmtType) {
        case MSG_NVL:
        case MSG_CSV:
        case MSG_TLV:
        case MSG_XML:
            preBuf->ptr = NULL;
            break;
        case MSG_FML:
            tpMsgCtxClear(&preBuf->buf);
            preBuf->ptr = &preBuf->buf;
            break;
        case MSG_ISO8583:
            tpMsgCtxClear(&preBuf->buf);
            preBuf->buf.free = 0;
            preBuf->ptr = &preBuf->buf;
            break;
        case MSG_CUSTOM:
            preBuf->ptr = NULL;
            strcpy(preBuf->freeFun, fmtDefs->freeFun);
            break;
        default:
            LOG_ERR("���ĸ�ʽ�޷�ʶ��");
            preBuf->ptr = NULL;
            return -1;
    }
    return 0;
}

/***************************************************************
 * �ͷ�ΪԤ��/�óض�̬������ڴ�ռ�
 **************************************************************/
int tpPreBufFree(PREBUF *preBuf)
{
    char tmpBuf[MAXVALLEN+1];

    if (!preBuf->ptr) return 0;
    switch (preBuf->fmtType) {
        case MSG_NVL:
        case MSG_CSV:
        case MSG_TLV:
        case MSG_FML:
        case MSG_ISO8583:
            break;
        case MSG_XML:
            freeXML((XMLDOC *)preBuf->ptr);
            break;
        case MSG_CUSTOM:
            if (preBuf->freeFun[0]) tpFunExec(preBuf->freeFun, tmpBuf, sizeof(tmpBuf));
            break;
        default:
            LOG_ERR("���ĸ�ʽ�޷�ʶ��");
            return -1;
    }
    preBuf->ptr = NULL;
    return 0;
}

/***************************************************************
 * ����ѭ���������ʽ
 **************************************************************/
char *tpGetCompKey(char *express, char *result)
{
    int  i;
    char *ch,buf[MAXEXPLEN+1];
    char tmpBuf[MAXVALLEN+1];

    if (']' != express[strlen(express)-1]) return express;

    strcpy(buf, express);
    buf[strlen(buf)-1] = 0;

    if (!(ch = strchr(buf, '['))) {
        LOG_ERR("ѭ���������ʽ�﷨����");
	return NULL;
    }
    *ch = 0;

    if (tpFunExec(ch+1, tmpBuf, sizeof(tmpBuf)) < 0) return NULL;
    i = atoi(tmpBuf);
    if (i<0 || i>999) {
        LOG_ERR("ѭ�������[%d]����ȷ", i);
        return NULL;
    }
    snprintf(result, MAXKEYLEN+1, "%s#%d", strTrim(buf), i); 
    return result;
}

/***************************************************************
 * ���Ľ��
 **************************************************************/
int _tpMsgConvUnpack(CONVGRP *pConvGrp, FMTDEFS *fmtDefs, TPCTX *pCtx, int nest_flag)
{
    int  i,tmpLen;
    char tmpBuf[MAXVALLEN+1];
    char *key,keyBuf[MAXKEYLEN+1];
    CONVGRP  *pConvGrp_ref;
    CONVSTMT *pConvStmt;

    for (i=0; i<pConvGrp->nStmts; ) {
        pConvStmt = &gShm.pConvStmt[i+pConvGrp->nStart];
        switch (pConvStmt->action) {
            case DO_NONE: /*do nothing*/
                break;

            case DO_JUMP: /*������ת*/
                if (!pConvStmt->express[0]) {
                    i = pConvStmt->jump - pConvGrp->nStart;
                    continue;
                } else {
                    if ((tmpLen = tpFunExec(pConvStmt->express, tmpBuf, sizeof(tmpBuf))) < 0) {
                        LOG_ERR("������ת�������ʽ����");
                        return -1;
                    }
                    if (tmpBuf[0] != 0 && tmpBuf[0] != '0') {
                        i = pConvStmt->jump - pConvGrp->nStart;
                        continue;
                    }
                }
                break;

            case DO_VAR: /*����ʱ����*/
                if ((tmpLen = tpFunExec(pConvStmt->express, tmpBuf, sizeof(tmpBuf))) < 0) {
                    LOG_ERR("������ʱ����ֵ����");
                    return -1;
                }
                if (tpMsgCtxHashPut(pCtx->msgCtx, pConvStmt->fldName, tmpBuf, tmpLen) != 0) {
                    LOG_ERR("����ʱ����[%s]����", pConvStmt->fldName);
                    return -1;
                }
                break;

            case DO_ALLOC: /*Ԥ����ռ�(�������ʱ����)*/
                if ((tmpLen = tpFunExec(pConvStmt->express, tmpBuf, sizeof(tmpBuf))) < 0) {
                    LOG_ERR("����Ԥ����ռ��С����");
                    return -1;
                }
                if (tpMsgCtxHashAlloc(pCtx->msgCtx, pConvStmt->fldName, atoi(tmpBuf)) != 0) {
                    LOG_ERR("Ϊ��ʱ����[%s]Ԥ����ռ�[%d]����", pConvStmt->fldName, atoi(tmpBuf));
                    return -1;
                }
                break;

            case DO_SET: /*����������*/
                if ((tmpLen = tpFunExec(pConvStmt->express, tmpBuf, sizeof(tmpBuf))) < 0) {
                    LOG_ERR("������������ֵ���ʽ����");
                    return -1;
                }
                if (!(key = tpGetCompKey(pConvStmt->fldName, keyBuf))) {
                    LOG_ERR("����ѭ���������ʽ����");
                    return -1;
                }
                if (tpMsgCtxHashPut(pCtx->msgCtx, key, tmpBuf, tmpLen) != 0) {
                    LOG_ERR("�������ı�����[%s]����", key);
                    return -1;
                }
                break;

            case DO_CONV: /*����ת����*/
                if (tpConvShmGet(atoi(pConvStmt->express), &pConvGrp_ref) <= 0) {
                    LOG_ERR("Ҫ���õ�ת����[%d]������", atoi(pConvStmt->express));
                    return -1;
                }
                if (_tpMsgConvUnpack(pConvGrp_ref, fmtDefs, pCtx, 1) != 0) {
                    LOG_ERR("ִ������ת����[%d]����", pConvGrp_ref->grpId);
                    return -1;
                }
                break;

            case DO_RETURN: /*�жϽű�*/
                break;

            default:
                LOG_WRN("ת�������޷�ʶ��");
                break;
        } /*switch end*/
        i++;
    } /*for end*/
    return 0;
}

/***************************************************************
 * ���Ĵ��
 **************************************************************/
int _tpMsgConvPack(CONVGRP *pConvGrp, FMTDEFS *fmtDefs, TPCTX *pCtx, int nest_flag)
{
    int  i,j,ret,tmpLen;
    int  size,offset,flag=0;
    char *key,keyBuf[MAXKEYLEN+1];
    char tmpBuf[MAXVALLEN+1];
    CONVGRP  *pConvGrp_ref;
    CONVSTMT *pConvStmt;
    TPMSG  *ptpMsg = &pCtx->tpMsg;
    TPHEAD *ptpHead = &ptpMsg->head;

    if (!nest_flag) {
        if (tpPreBufInit(&pCtx->preBuf, fmtDefs) != 0) {
            LOG_ERR("��ʼ��Ԥ�óس���");
            return -1;
        }
        ptpHead->bodyLen = 0;
        ptpHead->bodyOffset = 0;
        ptpHead->fmtType = fmtDefs->fmtType;
    }

    for (i=0; i<pConvGrp->nStmts; ) {
        pConvStmt = &gShm.pConvStmt[i+pConvGrp->nStart];
        switch (pConvStmt->action) {
            case DO_NONE: /*do nothing*/
                break;

            case DO_JUMP: /*������ת*/
                if (!pConvStmt->express[0]) {
                    i = pConvStmt->jump - pConvGrp->nStart;
                    continue;
                } else {
                    if ((tmpLen = tpFunExec(pConvStmt->express, tmpBuf, sizeof(tmpBuf))) < 0) {
                        LOG_ERR("������ת�������ʽ����");
                        goto ERR_RET;
                    }
                    if (tmpBuf[0] != 0 && tmpBuf[0] != '0') {
                        i = pConvStmt->jump - pConvGrp->nStart; 
                        continue;
                    }
                }
                break;

            case DO_VAR: /*����ʱ����*/
                if (!pCtx->msgCtx) {
                    LOG_ERR("�����������쳣��δ�ܷ���");
                    goto ERR_RET;
                }
                if ((tmpLen = tpFunExec(pConvStmt->express, tmpBuf, sizeof(tmpBuf))) < 0) {
                    LOG_ERR("������ʱ����ֵ����");
                    goto ERR_RET;
                }
                if (tpMsgCtxHashPut(pCtx->msgCtx, pConvStmt->fldName, tmpBuf, tmpLen) != 0) {
                    LOG_ERR("����ʱ����[%s]����", pConvStmt->fldName);
                    goto ERR_RET;
                }
                break;

            case DO_ALLOC: /*Ԥ����ռ�(��ʱ����)*/
                if (!pCtx->msgCtx) {
                    LOG_ERR("�����������쳣��δ�ܷ���");
                    goto ERR_RET;
                }
                if ((tmpLen = tpFunExec(pConvStmt->express, tmpBuf, sizeof(tmpBuf))) < 0) {
                    LOG_ERR("����Ԥ����ռ��С����");
                    goto ERR_RET;
                }
                if (tpMsgCtxHashAlloc(pCtx->msgCtx, pConvStmt->fldName, atoi(tmpBuf)) != 0) {
                    LOG_ERR("Ϊ��ʱ����[%s]Ԥ����ռ�[%d]����", pConvStmt->fldName, atoi(tmpBuf));
                    goto ERR_RET;
                }
                break;

            case DO_SET: /*��Ԥ�ó���*/
                if (!flag) flag = 1;
                if ((tmpLen = tpFunExec(pConvStmt->express, tmpBuf, sizeof(tmpBuf))) < 0) {
                    LOG_ERR("��������ֵ���ʽ����");
                    goto ERR_RET;
                }
                switch (pCtx->preBuf.fmtType) {
                    case MSG_NVL:
                        memcpy(ptpMsg->body+ptpHead->bodyLen, tmpBuf, tmpLen);
                        ptpHead->bodyLen += tmpLen;
                        break;
                    case MSG_CSV:
                        if ('*' == pConvStmt->fldName[0]) {
                            memcpy(ptpMsg->body+ptpHead->bodyOffset, tmpBuf, tmpLen);
                            ptpHead->bodyOffset += tmpLen;
                        } else {
                            memcpy(ptpMsg->body+ptpHead->bodyOffset+ptpHead->bodyLen, tmpBuf, tmpLen);
                            ptpHead->bodyLen += tmpLen;
                        }
                        break;
                    case MSG_TLV:
                        if (!pConvStmt->fldName[0]) {
                            memcpy(ptpMsg->body+ptpHead->bodyOffset, tmpBuf, tmpLen);
                            ptpHead->bodyOffset += tmpLen;
                        } else {
                            offset = ptpHead->bodyOffset+ptpHead->bodyLen;
                            size = MAXMSGLEN-sizeof(TPHEAD)-ptpHead->bodyOffset-ptpHead->bodyLen;
                            ret = tlvCat(ptpMsg->body+offset, size, pConvStmt->fldName, tmpBuf, tmpLen);
                            if (ret == -1) {
                                LOG_ERR("tlvCat(%s) error", pConvStmt->fldName);
                                goto ERR_RET;
                            }
                            ptpHead->bodyLen += ret;
                        }
                        break;
                    case MSG_FML:
                        if (!pConvStmt->fldName[0]) {
                            memcpy(ptpMsg->body+ptpHead->bodyOffset, tmpBuf, tmpLen);
                            ptpHead->bodyOffset += tmpLen;
                        } else {
                            if (!(key = tpGetCompKey(pConvStmt->fldName, keyBuf))) {
                                LOG_ERR("����ѭ���������ʽ����");
                                goto ERR_RET;
                            }
                            if (tpMsgCtxHashPut((MSGCTX *)pCtx->preBuf.ptr, key, tmpBuf, tmpLen) != 0) {
                                LOG_ERR("���FML������[%s]����", key);
                                goto ERR_RET;
                            }
                        }
                        break;
                    case MSG_ISO8583:
                        if (!pConvStmt->fldName[0]) {
                            memcpy(ptpMsg->body+ptpHead->bodyOffset, tmpBuf, tmpLen);
                            ptpHead->bodyOffset += tmpLen;
                        } else {
                            j = atoi(pConvStmt->fldName);
                            if (j<=0 || j>NMAXBITS) {
                                LOG_ERR("ISO8583��ID����ȷ");
                                goto ERR_RET;
                            }
                            if (tpMsgCtxPut((MSGCTX *)pCtx->preBuf.ptr, j-1, tmpBuf, tmpLen) != 0) {
                                LOG_ERR("���ISO8583������[%d]����", j);
                                goto ERR_RET;
                            }
                            if (j > ((MSGCTX *)pCtx->preBuf.ptr)->free) {
                                ((MSGCTX *)pCtx->preBuf.ptr)->free = j;
                            }
                        }
                        break;
                    case MSG_XML:
                        if ('*' == pConvStmt->fldName[0]) {
                            memcpy(ptpMsg->body+ptpHead->bodyOffset, tmpBuf, tmpLen);
                            ptpHead->bodyOffset += tmpLen;
                        }
                        break;
                    case MSG_CUSTOM:
                        break;
                    default:
                        LOG_ERR("���Ŀ�걨�ĸ�ʽ���Ͳ���ȷ");
                        goto ERR_RET;
                }
                break;

            case DO_CONV: /*����ת����*/
                if (tpConvShmGet(atoi(pConvStmt->express), &pConvGrp_ref) <= 0) {
                    LOG_ERR("Ҫ���õ�ת����[%d]������", atoi(pConvStmt->express));
                    return -1;
                }
                if (_tpMsgConvPack(pConvGrp_ref, fmtDefs, pCtx, 1) != 0) {
                    LOG_ERR("ִ������ת����[%d]����", pConvGrp_ref->grpId);
                    return -1;
                }
                break;

            case DO_RETURN: /*�жϽű�*/
                break;

            default:
                break;
        } /*switch end*/
        i++;
    } /*for end*/

    if (nest_flag) return 0; /*�ݹ���÷���*/
    if (!flag) return 0; /*�ձ���*/

    /*��Ԥ�ó����ɱ���*/
    switch (pCtx->preBuf.fmtType) {
        case MSG_FML:
            ptpHead->bodyLen = tpMsgCtxPack((MSGCTX *)pCtx->preBuf.ptr, ptpMsg->body+ptpHead->bodyOffset, MAXMSGLEN-sizeof(TPHEAD));
            if (ptpHead->bodyLen < 0) {
                LOG_ERR("tpMsgCtxPack() error");
                goto ERR_RET;
            }
            break;
        case MSG_XML:
            ptpHead->bodyLen = tpXMLPack((XMLDOC *)pCtx->preBuf.ptr, ptpMsg->body+ptpHead->bodyOffset, MAXMSGLEN-sizeof(TPHEAD), &fmtDefs->fmtProp);
            if (ptpHead->bodyLen < 0) {
                LOG_ERR("tpXMLPack() error");
                goto ERR_RET;
            }
            break;
        case MSG_ISO8583:
            ptpHead->bodyLen = tpISO8583Pack((MSGCTX *)pCtx->preBuf.ptr, ptpMsg->body+ptpHead->bodyOffset, MAXMSGLEN-sizeof(TPHEAD), &fmtDefs->fmtProp);
            if (ptpHead->bodyLen < 0) {
                LOG_ERR("tpISO8583Pack() error");
                goto ERR_RET;
            }
            break;
        default:
            break;
    }
#if 0
    if (fmtDefs->packLog[0]) {
        if (tpFunExec(fmtDefs->packLog, tmpBuf, sizeof(tmpBuf)) <0) {
            LOG_ERR("ִ�д������������");
            goto ERR_RET;
        }
    }
#endif        
/*add by dwxiong 2014-11-5 20:58:32*/    
    /*ִ�д��������*/
    if (fmtDefs->packFun[0]) {
        if (tpFunExec(fmtDefs->packFun, tmpBuf, sizeof(tmpBuf)) <0) {
            LOG_ERR("ִ�д������������");
            goto ERR_RET;
        }
    }
/* #endif */        /*del by dwxiong 2014-11-5 20:58:32*/
    if (pCtx->preBuf.fmtType != MSG_CUSTOM) ptpHead->bodyLen += ptpHead->bodyOffset;
    return 0;

ERR_RET:
    tpPreBufFree(&pCtx->preBuf);
    return -1;
}

