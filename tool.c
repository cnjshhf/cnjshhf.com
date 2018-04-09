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

char LogName[100];
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

    for (it = g_conf->xml_modules.begin();it < g_conf->xml_modules.end(); it++ )
        {   
        if(*it != NULL){
            delete *it;
            *it = NULL;
        };  
    }   
    g_conf->xml_modules.clear();

}

void handle_normal(Xml *xml,char *filename,char **nodename,int nodecount,char *curlparm) 
{
    XMLDOC *doc;
    int i,j;
    char buf[1024] = {0};
    char tmp[1024] = {0};
    char buf2[1024] ={0};

    for(i = 0;i <count; i++){
        memset(buf2,0,sizeof(buf2));
        strcpy(buf2,curlparm);
        doc = xml->loadXMLFile(filepath[i]);
        for(j = 0;j<nodecount;j++){                                                          
            memset(buf,0,sizeof(buf));
            //printf("[%s]",nodename[j]);
            xml->getXMLText(doc, nodename[j], buf, sizeof(buf));
            snprintf(tmp,sizeof(tmp),"{%s:%s}",filename,nodename[j]);
            strRepl(buf2,tmp,buf);
        }   
        xml->freeXML(doc);
        printf("[%s]\n",buf2);
    }
}


int getAngEle(char *line,char **buf,int *elecount)
{
    int len=0,i=0;
    char *pch =NULL;
    //printf("[%s]",line);
    while(sscanf (line+len,"%*[^{]{%[^}]",buf[i])!=EOF)
    {
        pch = strstr(line,buf[i]);
        len = pch-line+strlen(buf[i]);
        *elecount+=1;
        //buf[i][strlen(buf[i])]='\0';
        i++;
        //printf("%s",buf[i]);
    }
    return 0;
}

int main(void )
{

    char log[] = "tool.log";
    Module *module = NULL;
    Xml *xml = NULL;
    void * a = NULL;
    int i = 0;
    char **nodename = NULL;
    char nodepath[256] = {0};
    char filedes[1024] = {0};
    char **confele = NULL;
    char curlparm[1024];
    int argc;
    char **argv = NULL;
    int elecount =0;
    char tmp[1024*10] ={0};

    /*载入配置文件*/
	Config *g_conf = NULL;
    g_conf = initconfig();
    loadconfig(g_conf);
    
    //printf("[%s]",strRepl(*itvurl,"mappingKey","mappingkey"));

    /*建立路径映射*/
    map<string,string> route;
    map<string,string>::iterator itmap;
    route ["endpoint"]="/config/endpoint";
    route ["tasks"]="/spring";
    route ["macro"]="/config/macro";
    route ["mapping"]="/config/mapping";
    route ["message"]="/config/message";
    route ["pack"]="/config/pack";
    route ["procedure"]="/config/procedure";
    route ["route"]="/config/route";
    route ["service"]="/config/service";
    route ["unpack"]="/config/unpack";
    

    /*载入测试动态库文件*/
    vector<char *>::iterator it = g_conf->load_modules.begin();
    for(; it != g_conf->load_modules.end(); it++) {
        module = dso_load(g_conf->module_path, *it); 
        if(module->handle)module->handle(a);
    } 
    
    /*载入xml解析库文件*/
    vector<char *>::iterator its = g_conf->xml_modules.begin();
    for(; its!= g_conf->xml_modules.end(); its++) {
        xml = dso_xml(g_conf->module_path, *its); 
    } 

    //printf("%s",route.find("endpoint")->second.c_str()); 
    vector<char *>::iterator itvurl = g_conf->vurl.begin();


    confele = getMem(50);
    getAngEle(*itvurl,confele,&elecount);
    nodename =  getMem(elecount);

    for(i=0;i<elecount;i++)
    {
        argv = strsplit(confele[i], ':', &argc, 1);
        nodename[i]=strdup(argv[1]);
        //printf("[%s]",nodename[i]);
        free(argv);
        argv =NULL;
    }


    vector<char *>::iterator itss;
    for(itss = g_conf->vurl.begin();itss<g_conf->vurl.end();itss++)
    {
        count =0;
        memset(filepath,0,sizeof(filepath));        
        memset(tmp,0,sizeof(tmp));
        memset(filedes,0,sizeof(filedes));

        argv = strsplit(confele[0], ':', &argc, 1);
        snprintf(filedes, sizeof(filedes), "%s%s", getenv("PRO_PATH"),route.find(argv[0])->second.c_str());
        //printf("%s",filedes);
        getxmlpath(filedes);
        strcpy(tmp,*itss);
        handle_normal(xml,argv[0],nodename,elecount,tmp);
        free(argv);
        argv =NULL;
    }



    freeconf(g_conf);
    FreeMem(nodename,elecount);
    FreeMem(confele,elecount);
	setLogName(log);

	
	return 0;
}
