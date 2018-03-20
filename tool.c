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
#include <map>
#include <list>

using namespace std;

char filepath[1024][1024];
int count;

int getxmlpath(char *des)
{  
        FILE *fp1;
        FILE *fp2;
        int i = 0, j = 0;
        char buf[1024][1024];
        char tmp[1024*10] = {0};
        char cmd[1024] = {0};
        char newdes[1024*10] = {0};
   
        snprintf(cmd, sizeof(cmd), "ls %s|grep [.]xml$",des);
    //printf("[%s]",cmd);
        if ((fp1 = popen(cmd, "r")) != NULL){
                while(fgets(tmp, sizeof(tmp), fp1)!=NULL){
                        snprintf(buf[i], sizeof(buf[0]), "%s/%s",des,tmp);
                        buf[i][strlen(buf[i])-1]='\0';
                        i++;
                }
                memset(tmp,0,sizeof(tmp));
                for(j=0;j<i;j++){
                    sprintf(filepath[count],"%s", buf[j]);
                    //printf("[%s]",filepath[count]);
                    memset(tmp,0,sizeof(tmp));
                    count ++;
                }
                //printf("[%d]\n",count);
        }
        memset(cmd,0,sizeof(cmd));
        sprintf(cmd,"ls -l %s|grep '^d'|awk '{print $NF}'",des);
        if ((fp2 = popen(cmd, "r")) != NULL){
                while(fgets(tmp, sizeof(tmp), fp2)!=NULL){
                        tmp[strlen(tmp)-1]='\0';
                        sprintf(newdes,"%s/%s",des,tmp);
                        //printf("[%s]\n",newdes);
                        getxmlpath(newdes);
                }
        }
        return 0;
 
}


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
void handle_normal(Xml *xml,char *path,char (*nodename)[50],int nodecount) 
{
    XMLDOC *doc;
    int i,j;
    char buf[1024] = {0};

    count =0;
    memset(filepath,0,sizeof(filepath));

    getxmlpath(path);
    for(i = 0;i <nodecount; i++){
        for(j = 0;j<count;j++){                                                          
            memset(buf,0,sizeof(buf));
            doc = xml->loadXMLFile(filepath[j]);
            xml->getXMLText(doc, nodename[i], buf, sizeof(buf));
            printf("[%s]",buf);
            xml->freeXML(doc);
            printf("[%d]",j);
        }   
    }
}

void handle_list(Xml *xml,char *path,list<string> mylist) 
{
    XMLDOC *doc;
    int i,j;
    char buf[1024] = {0};
 
    count =0; 
    memset(filepath,0,sizeof(filepath));
 
    getxmlpath(path);
    for(i = 0;i <(int) mylist.size() ; i++){
        for (j = 0;j<count;j++){
            memset(buf,0,sizeof(buf));
            doc = xml->loadXMLFile(filepath[j]);
            xml->getXMLText(doc, mylist, buf, sizeof(buf));
            printf("[%s]",buf);
            xml->freeXML(doc);
            printf("[%d]",j);
        }   
    }   
}


int main(void )
{
	
    char log[] = "tool.log";
    Module *module = NULL;
    Xml *xml = NULL;
    void * a = NULL;
    int i;
    char nodename[][50] = {"mappingKey","queue"};
    char str[1024];

    list<string> mylist;
    list<string>::iterator itlist;
    mylist.push_back("mappingKey");
    mylist.push_back("queue");



    

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

    //handle_normal(xml,"/home/tpbuswfgjj/demo01/xml/endpoint",nodename,2);
    handle_list(xml,"/home/tpbuswfgjj/demo01/xml/endpoint",mylist);


    map<string,string> mymap;
    map<string,string>::iterator itmap;
    mymap["aa"]="100";
    mymap["bb"]="200";
    mymap["cc"]="300";
    for(itmap =mymap.begin();itmap != mymap.end(); itmap++ )
        sprintf(str+strlen(str),"[%s]=[%s]&",(*itmap).first.c_str(),(*itmap).second.c_str());
    printf("%s",str);
    freeconf(g_conf);
	setLogName(log);

	
	return 0;
}
