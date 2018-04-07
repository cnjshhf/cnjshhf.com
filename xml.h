#ifndef XML_H
#define XML_H 
#include <libxml/xmlmemory.h>
#include <libxml/parser.h>
#include <libxml/parserInternals.h>
#include <libxml/HTMLparser.h>
#include <libxml/HTMLtree.h>
#include <libxml/tree.h>
#include <libxml/xpath.h>
#include <libxml/xpathInternals.h>
#include <libxml/debugXML.h>
#include <libxml/xmlerror.h>
#include <libxml/xinclude.h>
#include <libxml/catalog.h>
#include <libxml/globals.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
    
#define XMLNODE  xmlNode
    
typedef struct {
    xmlDocPtr  doc;
    xmlNodePtr root;
    char encoding_char[21];
    char encoding_dump[21];
} XMLDOC;

#endif
