#ifndef DSO_H
#define DSO_H 

#include <vector>
#include <map>
#include <string>
#include "xml.h"
using namespace std;

#define MODULE_OK  0
#define MODULE_ERR 1

#define MAGIC_MAJOR_NUMBER 20140814
#define MAGIC_MINOR_NUMBER 0



typedef struct Module{
    //int          version;
    //int          minor_version;
    //const char  *name;
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

extern vector<Module *> modules_pre_surl;
extern map<string, Module*> modules;

#define SPIDER_ADD_MODULE_PRE_SURL(module) do {\
    modules_pre_surl.push_back(module); \
} while(0)

/* Dynamic load modules while spiderq is starting */
extern Module * dso_load(const char *path, const char *name);
extern Xml * dso_xml(const char *path, const char *name);

#endif
