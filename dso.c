/*************************************************************************
	> File Name: dso.c
	> Author: hhf
	> Mail: hhf 
	> Created Time: 2018-03-19 17:16:47
 ************************************************************************/

#include "dso.h"
#include "qstring.h"
#include <stdio.h>

Module * dso_load(const char *path, const char *name)
{
    void *rv = NULL;
    void *handle = NULL;
    Module *module = NULL;

    char * npath = strcat2(3, path, name, ".so");

    //printf("[%s]",npath);
    if ((handle = dlopen(npath, RTLD_GLOBAL | RTLD_NOW)) == NULL) { 
    //if ((handle = dlopen(npath, RTLD_NOW)) == NULL){ 
        printf("Load module fail(dlopen): %s", dlerror());
    }
    //printf("[%s]",name);
    if ((rv = dlsym(handle, name)) == NULL) {
        printf("Load module fail(dlsym): %s", dlerror());
    }
    module = (Module *)rv;
    module->init(module);

    return module;
}

Xml * dso_xml(const char *path, const char *name)
{
    void *rv = NULL;
    void *handle = NULL;
    Xml *xml = NULL;
 
    char * npath = strcat2(3, path, name, ".so");
 
    //printf("[%s]",npath);
    if ((handle = dlopen(npath, RTLD_GLOBAL | RTLD_NOW)) == NULL) { 
    //if ((handle = dlopen(npath, RTLD_NOW)) == NULL){ 
        printf("Load xmlmod fail(dlopen): %s", dlerror());
    }   
    //printf("[%s]",name);
    if ((rv = dlsym(handle, name)) == NULL) {
        printf("Load xmlmod fail(dlsym): %s", dlerror());
    }   
    xml = (Xml *)rv;
    xml->init(xml);
 
    return xml;
}
