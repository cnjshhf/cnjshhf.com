#ifndef DSO_H
#define DSO_H 

#include <vector>
#include <dlfcn.h>
#include "xml.h"
using namespace std;


typedef struct Module{
//    int          version;
//    int          minor_version;
//    const char  *name;
    void (*init)(Module *);
    int (*handle)(void *);
} Module;
typedef struct Xml{
    void (*init)(Xml *);
    int (*convert)(iconv_t, unsigned char *, int *, unsigned char *, int *);
    XMLDOC *(*loadXMLFile)(const char *);
    int (*getXMLText)(XMLDOC *, const char *, char *, int );
    void (*freeXML)(XMLDOC *);
} Xml;


/* Dynamic load modules while spiderq is starting */
extern Module * dso_load(const char *path, const char *name);
extern Xml * dso_xml(const char *path, const char *name);

#endif
