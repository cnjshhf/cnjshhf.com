#include <sys/types.h>
#include <unistd.h>
#include <sys/wait.h>

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <signal.h>
#include <errno.h>
#include <signal.h>
#include <time.h>
#include <errno.h>
#include <stdarg.h>
#include <ctype.h>
#include <vector>
#include "qstring.h"
#include "confparser.h"
#include "dso.h"
#include "xml.h"

#include <iostream>

using namespace std;


char LogName[100];


void freeconf(Config * g_conf)
{
	if(g_conf->url != NULL)free(g_conf->url);
    if(g_conf->module_path != NULL)free(g_conf->module_path);
	vector<char *>::iterator it;	
	for (it = g_conf->vurl.begin();it < g_conf->vurl.end(); it++ )
        {
		if(*it != NULL){
			delete *it;
			*it = NULL;
		};
	}
	g_conf->vurl.clear();

    for (it = g_conf->load_modules.begin();it < g_conf->load_modules.end(); it++ )
        {   
        if(*it != NULL){
            delete *it;
            *it = NULL;
        };  
    }   
    g_conf->load_modules.clear();

	
}


int main(void )
{
	
    char log[] = "tool.log";
    Module *module = NULL;
    Xml *xml = NULL;
    void * a = NULL;
    XMLDOC *doc;
    char buf[1024] = {0};


	Config *g_conf = NULL;
    g_conf = initconfig();
    loadconfig(g_conf);
    //printf("[%s]",g_conf->module_path); 

    vector<char *>::iterator it = g_conf->load_modules.begin();
    for(; it != g_conf->load_modules.end(); it++) {
        module = dso_load(g_conf->module_path, *it); 
        if(module->handle)module->handle(a);
    } 
    
    vector<char *>::iterator its = g_conf->xml_modules.begin();
    for(; its!= g_conf->xml_modules.end(); its++) {
        xml = dso_xml(g_conf->module_path, *its); 
    } 

    doc = xml->loadXMLFile("1.xml");
    xml->getXMLText(doc, "script", buf, sizeof(buf));
    printf("[%s]",buf);
    xml->freeXML(doc);
    freeconf(g_conf);
	setLogName(log);

	
	return 0;
}
