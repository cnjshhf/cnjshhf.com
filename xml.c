/***************************************************************
 * XML解析函数(基于libxml2)
 ***************************************************************
 * initXML():            创建XML文档
 * loadXMLFile():        解析XML文件
 * loadXMLString():      解析XML文本
 * saveXMLFile():        输出XML文件
 * saveXMLString():      输出XML文本
 * freeXML():            释放XML文档
 * getXMLNode():         获取XML节点
 * getXMLNodeCount():    获取XML节点个数
 * newXMLNode():         新增XML节点
 * setXMLNameSpace():    设置XML命名空间
 * setXMLText():         设置XML节点文本
 * getXMLText():         获取XML节点文本
 * setXMLProp():         设置XML节点属性
 * getXMLProp():         获取XML节点属性
 * encoding():           码制转换
 * add by Yuan.y
 * newXMLNode_Noencoding  新增XML节点
 * setXMLText_Noencoding  设置XML节点文本
 * getXMLText_Noencoding  获取XML节点文本
 * setXMLProp_Noencoding  设置XML节点属性
 * getXMLProp_Noencoding  获取XML节点属性
 **************************************************************/
#include "xml.h"

/***************************************************************
 * 创建XML文档
 **************************************************************/
XMLDOC *initXML(const char *root, const char *encoding)
{
    XMLDOC    *xml;
    xmlNodePtr node;

    if (!root || !root[0]) {
        LOG_ERR("Invalid root element name");
        return NULL;
    }

    if (!(xml = malloc(sizeof(XMLDOC)))) {
        LOG_ERR("malloc() error: %s", strerror(errno));
        return NULL;
    }
    memset(xml, 0, sizeof(XMLDOC));

    if (!(xml->doc = xmlNewDoc((const xmlChar *)"1.0"))) {
        LOG_ERR("xmlNewDoc() error");
        free(xml);
        return NULL;
    }
    if (encoding) strncpy(xml->encoding_char, encoding, sizeof(xml->encoding_char)-1);

    if (!(node = xmlNewNode(NULL, (const xmlChar *)root))) {
        LOG_ERR("xmlNewNode(%s) error", root); 
        xmlFreeDoc(xml->doc);
        free(xml);
        return NULL;
    }
    xmlDocSetRootElement(xml->doc, node);
    xml->root = xmlDocGetRootElement(xml->doc);
    return xml;
}

/***************************************************************
 * 解析XML文件
 **************************************************************/
XMLDOC *loadXMLFile(const char *file)
{
    XMLDOC *xml;

    if (!(xml = malloc(sizeof(XMLDOC)))) {
        LOG_ERR("malloc() error: %s", strerror(errno));
        return NULL;
    }
    memset(xml, 0, sizeof(XMLDOC));

    if (!(xml->doc = xmlReadFile(file, NULL, XML_PARSE_NOBLANKS))) {
        LOG_ERR("xmlReadFile(%s) error", ((!file)? "":file));
        /*xmlCleanupParser();*/
        free(xml);
        return NULL;
    }

    if (!(xml->root = xmlDocGetRootElement(xml->doc))) {
        LOG_ERR("xmlDocGetRootElement() error");
        xmlFreeDoc(xml->doc);
        /*xmlCleanupParser();*/
        free(xml);
        return NULL;
    }
    return xml;
}

/***************************************************************
 * 解析XML文本
 **************************************************************/
XMLDOC *loadXMLString(const char *buf)
{
    XMLDOC *xml;

    if (!(xml = malloc(sizeof(XMLDOC)))) {
        LOG_ERR("malloc() error: %s", strerror(errno));
        return NULL;
    }
    memset(xml, 0, sizeof(XMLDOC));

    if (!(xml->doc = xmlParseDoc((const xmlChar *)buf))) {
        LOG_ERR("xmlParseDoc() error");
        /*xmlCleanupParser();*/
        free(xml);
        return NULL;
    }

    if (!(xml->root = xmlDocGetRootElement(xml->doc))) {
        LOG_ERR("xmlDocGetRootElement() error");
        xmlFreeDoc(xml->doc);
        /*xmlCleanupParser();*/
        free(xml);
        return NULL;
    }
    return xml;
}

/***************************************************************
 * 输出XML文件
 **************************************************************/
int saveXMLFile(XMLDOC *xml, char *file, int fmt)
{
    int ret;

    if (!xml || !xml->doc || !xml->root) {
        LOG_ERR("Document can not be null");
        return -1;
    }

    if (!xml->encoding_dump[0]) {
        ret = xmlSaveFormatFileEnc(file, xml->doc, NULL, fmt);
    } else {
        ret = xmlSaveFormatFileEnc(file, xml->doc, xml->encoding_dump, fmt);
    }
    if (ret < 0) {
        LOG_ERR("xmlSaveFormatFileEnc() error");
        return -1;
    }
    return 0;
}

/***************************************************************
 * 输出XML文本
 **************************************************************/
int saveXMLString(XMLDOC *xml, char *buf, int size, int fmt)
{
    int dumplen;
    xmlChar *dumpbuf;

    if (!xml || !xml->doc || !xml->root) {
        LOG_ERR("Document can not be null");
        return -1;
    }

    if (!xml->encoding_dump[0]) {
        xmlDocDumpFormatMemoryEnc(xml->doc, &dumpbuf, &dumplen, NULL, fmt);
    } else {
        xmlDocDumpFormatMemoryEnc(xml->doc, &dumpbuf, &dumplen, xml->encoding_dump, fmt);
    }
    if (!dumpbuf || !dumplen) {
        LOG_ERR("xmlDocDumpFormatMemoryEnc() error");
        if (dumpbuf) xmlFree(dumpbuf);
        return -1;
    }

    if (size < (dumplen+1)) {
        LOG_ERR("Buffer size is not enough to dump");
        xmlFree(dumpbuf);
        return -1;
    }
    memcpy(buf, dumpbuf, dumplen);
    buf[dumplen] = 0;
    xmlFree(dumpbuf);
    return dumplen;
}

/***************************************************************
 * 释放XML文档
 **************************************************************/
void freeXML(XMLDOC *xml)
{
    if (xml) {
        if (xml->doc) {
            xmlFreeDoc(xml->doc);
            /*xmlCleanupParser();*/
        }
        free(xml);
    }
}

/***************************************************************
 * 获取XML节点
 **************************************************************/
XMLNODE *getXMLNode(XMLDOC *xml, const char *xpath)
{
    xmlNsPtr            ns;
    xmlNodePtr          node;
    xmlChar            *xpath_buf;
    xmlXPathContextPtr  xpath_ctx;
    xmlXPathObjectPtr   xpath_obj;

    if (!xml || !xml->doc || !xml->root) {
        LOG_ERR("Document can not be null");
        return NULL;
    }       
    if (!xpath || !xpath[0]) return xml->root;

    if (!(xpath_buf = xmlGetNodePath(xml->root))) {
        LOG_ERR("xmlGetNodePath() error");
        return NULL;
    }
    if (!(xpath_buf = xmlStrcat(xpath_buf, (const xmlChar *)"/"))) {
        LOG_ERR("xmlStrcat() error");
        return NULL;
    }
    if (!(xpath_buf = xmlStrcat(xpath_buf, (const xmlChar *)xpath))) {
        LOG_ERR("xmlStrcat() error");
        return NULL;
    }

    if (!(xpath_ctx = xmlXPathNewContext(xml->doc))) {
        LOG_ERR("xmlXPathNewContext() error");
        xmlFree(xpath_buf);
        return NULL;
    }

    ns = xml->root->ns;
    while (ns) {
        if (xmlXPathRegisterNs(xpath_ctx, ns->prefix, ns->href) != 0) {
            LOG_ERR("xmlXPathRegisterNs(%s,%s) error", ns->prefix, ns->href);
            xmlFree(xpath_buf);
            xmlXPathFreeContext(xpath_ctx);
            return NULL;
        }
        ns = ns->next;
    }

    if (!(xpath_obj = xmlXPathEval(xpath_buf, xpath_ctx))) {
        LOG_ERR("xmlXPathEval(%s) error", xpath_buf);
        xmlFree(xpath_buf);
        xmlXPathFreeContext(xpath_ctx);
        return NULL;
    }
    if (xpath_obj->type != XPATH_NODESET) {
        LOG_ERR("XPath(%s): node type is not NODESET", xpath_buf);
        xmlFree(xpath_buf);
        xmlXPathFreeContext(xpath_ctx);
        xmlXPathFreeObject(xpath_obj);
        return NULL;
    }
    if (!xpath_obj->nodesetval || !xpath_obj->nodesetval->nodeNr) {
        /* LOG_ERR("XPath(%s): none of nodes be found", xpath_buf); */  /*mod by dwxiong 2014-11-5 18:45:29*/
        LOG_WRN("XPath(%s): none of nodes be found", xpath_buf);
        xmlFree(xpath_buf);
        xmlXPathFreeContext(xpath_ctx);
        xmlXPathFreeObject(xpath_obj);
        return NULL;
    }
    node = xpath_obj->nodesetval->nodeTab[0];
    xmlFree(xpath_buf);
    xmlXPathFreeContext(xpath_ctx);
    xmlXPathFreeObject(xpath_obj);
    return node;
}

/***************************************************************
 * 获取XML节点个数
 **************************************************************/
int getXMLNodeCount(XMLDOC *xml, const char *xpath)
{
    xmlNsPtr            ns;
    xmlChar            *xpath_buf;
    xmlXPathContextPtr  xpath_ctx;
    xmlXPathObjectPtr   xpath_obj;
    
    if (!xml || !xml->doc || !xml->root) {
        LOG_ERR("Document can not be null");
        return -1;
    }
    if (!xpath || !xpath[0]) return 0;

    if (!(xpath_buf = xmlGetNodePath(xml->root))) {
        LOG_ERR("xmlGetNodePath() error");
        return -1;
    }
    if (!(xpath_buf = xmlStrcat(xpath_buf, (const xmlChar *)"/"))) {
        LOG_ERR("xmlStrcat() error");
        return -1;
    }
    if (!(xpath_buf = xmlStrcat(xpath_buf, (const xmlChar *)xpath))) {
        LOG_ERR("xmlStrcat() error");
        return -1;
    }

    if (!(xpath_ctx = xmlXPathNewContext(xml->doc))) {
        LOG_ERR("xmlXPathNewContext() error");
        xmlFree(xpath_buf);
        return -1;
    }

    ns = xml->root->ns;
    while (ns) {
        if (xmlXPathRegisterNs(xpath_ctx, ns->prefix, ns->href) != 0) {
            LOG_ERR("xmlXPathRegisterNs(%s,%s) error", ns->prefix, ns->href);
            xmlFree(xpath_buf);
            xmlXPathFreeContext(xpath_ctx);
            return -1;
        }
        ns = ns->next;
    }

    if ((xpath_obj = xmlXPathEval(xpath_buf, xpath_ctx)) == NULL) {
        LOG_ERR("xmlXPathEval(%s) error", xpath_buf);
        xmlFree(xpath_buf);
        xmlXPathFreeContext(xpath_ctx);
        return -1;
    }
    if (xpath_obj->type != XPATH_NODESET) {
        LOG_ERR("XPath(%s): type is not NODESET", xpath_buf);
        xmlFree(xpath_buf);
        xmlXPathFreeContext(xpath_ctx);
        xmlXPathFreeObject(xpath_obj);
        return -1;
    }
    if (!xpath_obj->nodesetval) return 0;
    return xpath_obj->nodesetval->nodeNr;
}

/***************************************************************
 * 新增XML节点
 **************************************************************/
int newXMLNode(XMLDOC *xml, const char *xpath, const char *name, char *text)
{
    xmlChar   *buf;
    xmlNodePtr node;    
    xmlNodePtr parent;

    if (!(parent = getXMLNode(xml, xpath))) {
        LOG_ERR("getXMLNode(%s) error", ((!xpath)? "":xpath));
        return -1;
    }
    if (!(node = xmlNewChild(parent, NULL, (const xmlChar *)name, NULL))) {
        LOG_ERR("xmlNewChild(%s,%s) error", ((!xpath)? "":xpath), ((!name)? "":name));
        return -1;
    }
    if (!text) return 0;

    if (xml->encoding_char[0]) {
        buf = xmlStrdup((xmlChar *)text);
        if (encoding(xml->encoding_char, "UTF-8", (char **)&buf) != 0) {
            LOG_ERR("encoding(%s->UTF-8) error", xml->encoding_char);
            xmlFree(buf);
            return -1;
        }
        xmlNodeSetContent(node, buf);
        xmlFree(buf);
        return 0;
    } else if (xml->doc->encoding) {
        buf = xmlStrdup((xmlChar *)text);
        if (encoding((const char *)xml->doc->encoding, "UTF-8", (char **)&buf) != 0) {
            LOG_ERR("encoding(%s->UTF-8) error", xml->doc->encoding);
            xmlFree(buf);
            return -1;
        }
        xmlNodeSetContent(node, buf);
        xmlFree(buf);
        return 0;
    }
    xmlNodeSetContent(node, (xmlChar *)text);
    return 0;
}

/***************************************************************
 * 新增XML节点(no encoding)
 **************************************************************/
int newXMLNode_Noencoding(XMLDOC *xml, const char *xpath, const char *name, char *text)
{
    xmlChar   *buf;
    xmlNodePtr node;    
    xmlNodePtr parent;

    if (!(parent = getXMLNode(xml, xpath))) {
        LOG_ERR("getXMLNode(%s) error", ((!xpath)? "":xpath));
        return -1;
    }
    if (!(node = xmlNewChild(parent, NULL, (const xmlChar *)name, NULL))) {
        LOG_ERR("xmlNewChild(%s,%s) error", ((!xpath)? "":xpath), ((!name)? "":name));
        return -1;
    }
    if (!text) return 0;

/*
    if (xml->encoding_char[0]) {
        buf = xmlStrdup((xmlChar *)text);
        if (encoding(xml->encoding_char, "UTF-8", (char **)&buf) != 0) {
            LOG_ERR("encoding(%s->UTF-8) error", xml->encoding_char);
            xmlFree(buf);
            return -1;
        }
        xmlNodeSetContent(node, buf);
        xmlFree(buf);
        return 0;
    } else if (xml->doc->encoding) {
        buf = xmlStrdup((xmlChar *)text);
        if (encoding((const char *)xml->doc->encoding, "UTF-8", (char **)&buf) != 0) {
            LOG_ERR("encoding(%s->UTF-8) error", xml->doc->encoding);
            xmlFree(buf);
            return -1;
        }        
        xmlNodeSetContent(node, buf);
        xmlFree(buf);
        return 0;
    }
*/    
    xmlNodeSetContent(node, (xmlChar *)text);
    return 0;
}

/***************************************************************
 * 设置XML命名空间
 **************************************************************/
int setXMLNameSpace(XMLDOC *xml, const char *xpath, const char *prefix, char *href)
{
    xmlNodePtr node;

    if (!(node = getXMLNode(xml, xpath))) {
        LOG_ERR("getXMLNode(%s) error", ((!xpath)? "":xpath));
        return -1;
    }
    if (!xmlNewNs(node, (const xmlChar *)href, (const xmlChar *)prefix)) {
        LOG_ERR("xmlNewNs(%s=%s) error", ((!prefix)? "":prefix), ((!href)? "":href));
        return -1;
    }
    return 0;
}

/***************************************************************
 * 设置XML节点文本
 **************************************************************/
int setXMLText(XMLDOC *xml, const char *xpath, char *text)
{
    xmlChar   *buf;
    xmlNodePtr node;

    if (!(node = getXMLNode(xml, xpath))) {
        LOG_ERR("getXMLNode(%s) error", ((!xpath)? "":xpath));
        return -1;
    }

    if (xml->encoding_char[0]) {
        buf = xmlStrdup((const xmlChar *)text);
        if (encoding(xml->encoding_char, "UTF-8", (char **)&buf) != 0) {
            LOG_ERR("encoding(%s->UTF-8) error", xml->encoding_char);
            xmlFree(buf);
            return -1;
        }
        xmlNodeSetContent(node, buf);
        xmlFree(buf);
        return 0;
    } else if (xml->doc->encoding) {
        buf = xmlStrdup((const xmlChar *)text);
        if (encoding((const char *)xml->doc->encoding, "UTF-8", (char **)&buf) != 0) {
            LOG_ERR("encoding(%s->UTF-8) error", xml->doc->encoding);
            xmlFree(buf);
            return -1;
        }
        xmlNodeSetContent(node, buf);
        xmlFree(buf);
        return 0;
    }
    xmlNodeSetContent(node, (xmlChar *)text);
    return 0;
}


/***************************************************************
 * 设置XML节点文本(no encoding)
 **************************************************************/
int setXMLText_Noencoding(XMLDOC *xml, const char *xpath, char *text)
{
    xmlChar   *buf;
    xmlNodePtr node;

    if (!(node = getXMLNode(xml, xpath))) {
        LOG_ERR("getXMLNode(%s) error", ((!xpath)? "":xpath));
        return -1;
    }
/*
    if (xml->encoding_char[0]) {
        buf = xmlStrdup((const xmlChar *)text);
        if (encoding(xml->encoding_char, "UTF-8", (char **)&buf) != 0) {
            LOG_ERR("encoding(%s->UTF-8) error", xml->encoding_char);
            xmlFree(buf);
            return -1;
        }
        xmlNodeSetContent(node, buf);
        xmlFree(buf);
        return 0;
    } else if (xml->doc->encoding) {
        buf = xmlStrdup((const xmlChar *)text);
        if (encoding((const char *)xml->doc->encoding, "UTF-8", (char **)&buf) != 0) {
            LOG_ERR("encoding(%s->UTF-8) error", xml->doc->encoding);
            xmlFree(buf);
            return -1;
        }
        xmlNodeSetContent(node, buf);
        xmlFree(buf);
        return 0;
    }
*/
    xmlNodeSetContent(node, (xmlChar *)text);
    return 0;
}

/***************************************************************
 * 获取XML节点文本
 **************************************************************/
int getXMLText(XMLDOC *xml, const char *xpath, char *text, int size)
{
    xmlChar   *buf;
    xmlNodePtr node;

    if (!(node = getXMLNode(xml, xpath))) {
        /* LOG_ERR("getXMLNode(%s) error", ((!xpath)? "":xpath)); */    /*mod by dwxiong 2014-11-5 18:47:03*/
        LOG_WRN("getXMLNode(%s) error", ((!xpath)? "":xpath));
        return -1;
    }
    if (!(buf = xmlNodeGetContent(node))) {
        LOG_ERR("xmlNodeGetContent(%s) error", ((!xpath)? "":xpath));
        return -1;
    }

    if (xml->encoding_char[0]) {
        if (encoding("UTF-8", xml->encoding_char, (char **)&buf) != 0) {
            LOG_ERR("encoding(UTF-8->%s) error", xml->encoding_char);
            xmlFree(buf);
            return -1;
        }
    } else if (xml->doc->encoding) {
        if (encoding("UTF-8", (const char *)xml->doc->encoding, (char **)&buf) != 0) {
            LOG_ERR("encoding(UTF-8->%s) error", xml->doc->encoding);
            xmlFree(buf);
            return -1;
        }
    }

    if (size < xmlStrlen(buf)+1) {
        LOG_ERR("Buffer is not enough");
        xmlFree(buf);
        return -1;
    }
    strcpy(text, (char *)buf);
    xmlFree(buf);
    return 0;
}

/***************************************************************
 * 获取XML节点文本(no encoding)
 **************************************************************/
int getXMLText_Noencoding(XMLDOC *xml, const char *xpath, char *text, int size)
{
    xmlChar   *buf;
    xmlNodePtr node;

    if (!(node = getXMLNode(xml, xpath))) {
        /* LOG_ERR("getXMLNode(%s) error", ((!xpath)? "":xpath)); */    /*mod by dwxiong 2014-11-5 18:47:03*/
        LOG_WRN("getXMLNode(%s) error", ((!xpath)? "":xpath));
        return -1;
    }
    if (!(buf = xmlNodeGetContent(node))) {
        LOG_ERR("xmlNodeGetContent(%s) error", ((!xpath)? "":xpath));
        return -1;
    }
/*
    if (xml->encoding_char[0]) {
        if (encoding("UTF-8", xml->encoding_char, (char **)&buf) != 0) {
            LOG_ERR("encoding(UTF-8->%s) error", xml->encoding_char);
            xmlFree(buf);
            return -1;
        }
    } else if (xml->doc->encoding) {
        if (encoding("UTF-8", (const char *)xml->doc->encoding, (char **)&buf) != 0) {
            LOG_ERR("encoding(UTF-8->%s) error", xml->doc->encoding);
            xmlFree(buf);
            return -1;
        }
    }
*/

    if (size < xmlStrlen(buf)+1) {
        LOG_ERR("Buffer is not enough");
        xmlFree(buf);
        return -1;
    }
    strcpy(text, (char *)buf);
    xmlFree(buf);
    return 0;
}

/***************************************************************
 * 设置XML节点属性
 **************************************************************/
int setXMLProp(XMLDOC *xml, const char *xpath, const char *name, char *prop)
{
    xmlChar   *buf;
    xmlNodePtr node;

    if (!(node = getXMLNode(xml, xpath))) {
        LOG_ERR("getXMLNode(%s) error", ((!xpath)? "":xpath));
        return -1;
    }

    if (xml->encoding_char[0]) {
        buf = xmlStrdup((xmlChar *)prop);
        if (encoding(xml->encoding_char, "UTF-8", (char **)&buf) != 0) {
            LOG_ERR("encoding(%s->UTF-8) error", xml->encoding_char);
            xmlFree(buf);
            return -1;
        }
        xmlSetProp(node, (const xmlChar *)name, buf);
        xmlFree(buf);
        return 0;
    } else if (xml->doc->encoding) {
        buf = xmlStrdup((xmlChar *)prop);
        if (encoding((const char *)xml->doc->encoding, "UTF-8", (char **)&buf) != 0) {
            LOG_ERR("encoding(%s->UTF-8) error", xml->doc->encoding);
            xmlFree(buf);
            return -1;
        }
        xmlSetProp(node, (const xmlChar *)name, buf);
        xmlFree(buf);
        return 0;
    }
    xmlSetProp(node, (const xmlChar *)name, (const xmlChar *)prop);
    return 0;
}

/***************************************************************
 * 设置XML节点属性(no encoding)
 **************************************************************/
int setXMLProp_Noencoding(XMLDOC *xml, const char *xpath, const char *name, char *prop)
{
    xmlChar   *buf;
    xmlNodePtr node;

    if (!(node = getXMLNode(xml, xpath))) {
        LOG_ERR("getXMLNode(%s) error", ((!xpath)? "":xpath));
        return -1;
    }
/*
    if (xml->encoding_char[0]) {
        buf = xmlStrdup((xmlChar *)prop);
        if (encoding(xml->encoding_char, "UTF-8", (char **)&buf) != 0) {
            LOG_ERR("encoding(%s->UTF-8) error", xml->encoding_char);
            xmlFree(buf);
            return -1;
        }
        xmlSetProp(node, (const xmlChar *)name, buf);
        xmlFree(buf);
        return 0;
    } else if (xml->doc->encoding) {
        buf = xmlStrdup((xmlChar *)prop);
        if (encoding((const char *)xml->doc->encoding, "UTF-8", (char **)&buf) != 0) {
            LOG_ERR("encoding(%s->UTF-8) error", xml->doc->encoding);
            xmlFree(buf);
            return -1;
        }
        xmlSetProp(node, (const xmlChar *)name, buf);
        xmlFree(buf);
        return 0;
    }
*/
    xmlSetProp(node, (const xmlChar *)name, (const xmlChar *)prop);
    return 0;
}

/***************************************************************
 * 获取XML节点属性
 **************************************************************/
int getXMLProp(XMLDOC *xml, const char *xpath, const char *name, char *prop, int size)
{
    xmlChar   *buf;
    xmlNodePtr node;    

    if (!(node = getXMLNode(xml, xpath))) {
        LOG_ERR("getXMLNode(%s) error", ((!xpath)? "":xpath));
        return -1;
    }
    if (!(buf = xmlGetProp(node, (const xmlChar *)name))) {
        LOG_ERR("xmlGetProp(%s@%s) error", ((!xpath)? "":xpath), ((!name)? "":name));
        return -1;
    }

    if (xml->encoding_char[0]) {
        if (encoding("UTF-8", xml->encoding_char, (char **)&buf) != 0) {
            LOG_ERR("encoding(UTF-8->%s) error", xml->encoding_char);
            xmlFree(buf);
            return -1;
        }
    } else if (xml->doc->encoding) {
        if (encoding("UTF-8", (const char *)xml->doc->encoding, (char **)&buf) != 0) {
            LOG_ERR("encoding(UTF-8->%s) error", xml->doc->encoding);
            xmlFree(buf);
            return -1;
        }
    }

    if (size < xmlStrlen(buf)+1) {
        LOG_ERR("Buffer is not enough");
        xmlFree(buf);
        return -1;
    }
    strcpy(prop, (char *)buf);
    xmlFree(buf);
    return 0;
}

/***************************************************************
 * 获取XML节点属性(no encoding)
 **************************************************************/
int getXMLProp_Noencoding(XMLDOC *xml, const char *xpath, const char *name, char *prop, int size)
{
    xmlChar   *buf;
    xmlNodePtr node;    

    if (!(node = getXMLNode(xml, xpath))) {
        LOG_ERR("getXMLNode(%s) error", ((!xpath)? "":xpath));
        return -1;
    }
    if (!(buf = xmlGetProp(node, (const xmlChar *)name))) {
        LOG_ERR("xmlGetProp(%s@%s) error", ((!xpath)? "":xpath), ((!name)? "":name));
        return -1;
    }
/*
    if (xml->encoding_char[0]) {
        if (encoding("UTF-8", xml->encoding_char, (char **)&buf) != 0) {
            LOG_ERR("encoding(UTF-8->%s) error", xml->encoding_char);
            xmlFree(buf);
            return -1;
        }
    } else if (xml->doc->encoding) {
        if (encoding("UTF-8", (const char *)xml->doc->encoding, (char **)&buf) != 0) {
            LOG_ERR("encoding(UTF-8->%s) error", xml->doc->encoding);
            xmlFree(buf);
            return -1;
        }
    }
*/
    if (size < xmlStrlen(buf)+1) {
        LOG_ERR("Buffer is not enough");
        xmlFree(buf);
        return -1;
    }
    strcpy(prop, (char *)buf);
    xmlFree(buf);
    return 0;
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

/***************************************************************
 * 码制转换
 **************************************************************/
int encoding(const char *from, const char *to, char **buf)
{
    int i = 0;
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


