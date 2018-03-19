/*************************************************************************
	> File Name: xmltpbus.c
	> Author: hhf
	> Mail: hhf 
	> Created Time: 2018-03-17 00:00:09
 ************************************************************************/
#include "xml.h"
#include "dso.h"
int encoding(const char *from, const char *to, char **buf);


XMLNODE *getXMLNode(XMLDOC *xml, const char *xpath)
{
    xmlNsPtr            ns;
    xmlNodePtr          node;
    xmlChar            *xpath_buf;
    xmlXPathContextPtr  xpath_ctx;
    xmlXPathObjectPtr   xpath_obj;

    if (!xml || !xml->doc || !xml->root) {
        printf("Document can not be null");
        return NULL;
    }       
    if (!xpath || !xpath[0]) return xml->root;

    if (!(xpath_buf = xmlGetNodePath(xml->root))) {
        printf("xmlGetNodePath() error");
        return NULL;
    }
    if (!(xpath_buf = xmlStrcat(xpath_buf, (const xmlChar *)"/"))) {
        printf("xmlStrcat() error");
        return NULL;
    }
    if (!(xpath_buf = xmlStrcat(xpath_buf, (const xmlChar *)xpath))) {
        printf("xmlStrcat() error");
        return NULL;
    }

    if (!(xpath_ctx = xmlXPathNewContext(xml->doc))) {
        printf("xmlXPathNewContext() error");
        xmlFree(xpath_buf);
        return NULL;
    }

    ns = xml->root->ns;
    while (ns) {
        if (xmlXPathRegisterNs(xpath_ctx, ns->prefix, ns->href) != 0) {
            printf("xmlXPathRegisterNs(%s,%s) error", ns->prefix, ns->href);
            xmlFree(xpath_buf);
            xmlXPathFreeContext(xpath_ctx);
            return NULL;
        }
        ns = ns->next;
    }

    if (!(xpath_obj = xmlXPathEval(xpath_buf, xpath_ctx))) {
        printf("xmlXPathEval(%s) error", xpath_buf);
        xmlFree(xpath_buf);
        xmlXPathFreeContext(xpath_ctx);
        return NULL;
    }
    if (xpath_obj->type != XPATH_NODESET) {
        printf("XPath(%s): node type is not NODESET", xpath_buf);
        xmlFree(xpath_buf);
        xmlXPathFreeContext(xpath_ctx);
        xmlXPathFreeObject(xpath_obj);
        return NULL;
    }
    if (!xpath_obj->nodesetval || !xpath_obj->nodesetval->nodeNr) {
        printf("XPath(%s): none of nodes be found", xpath_buf);
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
 

XMLDOC *loadXMLFile(const char *file)
{
    XMLDOC *xml;

    if (!(xml = (XMLDOC * )malloc(sizeof(XMLDOC)))) {
        printf("malloc() error: %s", strerror(errno));
        return NULL;

    }
    memset(xml, 0, sizeof(XMLDOC));

    if (!(xml->doc = xmlReadFile(file, NULL, XML_PARSE_NOBLANKS))) {
        printf("xmlReadFile(%s) error", ((!file)? "":file));
        /*xmlCleanupParser();*/
        free(xml);
        return NULL;

    }

    if (!(xml->root = xmlDocGetRootElement(xml->doc))) {
        printf("xmlDocGetRootElement() error");
        xmlFreeDoc(xml->doc);
        /*xmlCleanupParser();*/
        free(xml);
        return NULL;

    }
    return xml;

}

int getXMLText(XMLDOC *xml, const char *xpath, char *text, int size)
{
    xmlChar   *buf;
    xmlNodePtr node;

    if (!(node = getXMLNode(xml, xpath))) {
        printf("getXMLNode(%s) error", ((!xpath)? "":xpath));
        return -1;

    }
    if (!(buf = xmlNodeGetContent(node))) {
        printf("xmlNodeGetContent(%s) error", ((!xpath)? "":xpath));
        return -1;

    }

    if (xml->encoding_char[0]) {
        if (encoding("UTF-8", xml->encoding_char, (char **)&buf) != 0) {
            printf("encoding(UTF-8->%s) error", xml->encoding_char);
            xmlFree(buf);
            return -1;

        }

    } else if (xml->doc->encoding) {
        if (encoding("UTF-8", (const char *)xml->doc->encoding, (char **)&buf) != 0) {
            printf("encoding(UTF-8->%s) error", xml->doc->encoding);
            xmlFree(buf);
            return -1;

        }

    }

    if (size < xmlStrlen(buf)+1) {
        printf("Buffer is not enough");
        xmlFree(buf);
        return -1;

    }
    strcpy(text, (char *)buf);
    xmlFree(buf);
    return 0;

}

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

int encoding(const char *from, const char *to, char **buf)
{
    int i = 0;
    iconv_t cd;
    xmlChar *out;
    int i_len,o_len;

    if (strlen(from) == strlen(to)) {
        //for (i=0; to[i]; i++) if (toupper(from[i]) != toupper(to[i])) break; 
        for (i=0; to[i]; i++) if (from[i] != to[i]) break;
    }
    if (!to[i]) return 0;

    if ((cd = iconv_open(to, from)) == (iconv_t)-1) {
        printf("iconv_open(%s->%s) error [%d]", from, to, errno);
        return -1;
    }

    i_len = strlen(*buf) + 1;
    o_len = i_len * 2;
    out = (xmlChar*)xmlMalloc(o_len);

    if (convert(cd, (unsigned char *)(*buf), &i_len, (unsigned char *)out, &o_len)) {
        xmlFree(out);
        return -1;
    }
    iconv_close(cd);
    xmlFree(*buf);
    *buf = (char *)out;
    return 0;
}
void freeXML(XMLDOC *xml)
{
    if (xml) {
        if (xml->doc) {
            xmlFreeDoc(xml->doc);
            /*xmlCleanupParser();*/

        }
        free(xml);
        printf("[xml释放完成]\n");

    }

}

void init(Xml *xml)
{
    return;
}
/*
int main()
{
    XMLDOC *doc;
    char buf[1024] = {0};
    doc = loadXMLFile("1.xml");
    getXMLText(doc, "script", buf, sizeof(buf));
    printf("[%s]",buf);
    freeXML(doc);
    return 0;
}
*/
Xml xml = {
    init,
    convert,
    loadXMLFile,
    getXMLText,
    freeXML
};
