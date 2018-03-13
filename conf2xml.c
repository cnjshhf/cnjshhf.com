#include <sys/stat.h>
#include <sys/types.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <ctype.h>
#include <sys/types.h>
#include <regex.h>
#include <memory.h>
#include "findtask.h"
#include "findmapp.h"

char filepath[1024][1024];
char match[100];
int count;
FILE     *popen(const char *, const char *);


int handle(char *des)
{
        FILE *fp1;
        FILE *fp2;
        int i = 0, j = 0;
        char buf[1024][1024];
        char tmp[1024*10] = {0};
        char cmd[1024] = {0};
        char newdes[1024*10] = {0};

        snprintf(cmd, sizeof(cmd), "ls %s|grep .xml$",des);
        if ((fp1 = popen(cmd, "r")) != NULL){
                while(fgets(tmp, sizeof(tmp), fp1)!=NULL){
                        snprintf(buf[i], sizeof(buf[0]), "%s/%s",des,tmp);
                        buf[i][strlen(buf[i])-1]='\0';
                        //printf("result:[%s]",buf[ii]);
                        i++;
                }
                memset(tmp,0,sizeof(tmp));
                for(j=0;j<i;j++){
                        sprintf(filepath[count],"%s", buf[j]);
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
                        handle(newdes);
                }
        }
        return 0;

}

int readfile (char *propath,char **p) {
  FILE * pFile;
  long lSize;
  char * buffer;
  size_t result;

  pFile = fopen ( propath , "rb" );
  if (pFile==NULL) {fputs ("File error",stderr); exit (1);}

  fseek (pFile , 0 , SEEK_END);
  lSize = ftell (pFile);
  rewind (pFile);

  buffer = (char*) malloc (sizeof(char)*lSize);
  if (buffer == NULL) {fputs ("Memory error",stderr); exit (2);}

  result = fread (buffer,1,lSize,pFile);
  if (result != lSize) {fputs ("Reading error",stderr); exit (3);}

  fclose (pFile);
  *p = buffer;
  return 0;
}

int regxmatch(char *bematch,char *pattern){

    char errbuf[1024];
    regex_t reg;
    int err = 0,nm = 10;
    regmatch_t pmatch[nm];

    if(regcomp(&reg,pattern,REG_EXTENDED) < 0){
        regerror(err,&reg,errbuf,sizeof(errbuf));
        printf("err:%s\n",errbuf);
    }

    err = regexec(&reg,bematch,nm,pmatch,0);

    if(err == REG_NOMATCH){
        //printf("no match\n");
        printf("未设置端口\n");
        return -1;

    }else if(err){
        regerror(err,&reg,errbuf,sizeof(errbuf));
        printf("err:%s\n",errbuf);
        exit(-1);
    }

    for(int i=0;i<10 && pmatch[i].rm_so!=-1;i++){
        int len = pmatch[i].rm_eo-pmatch[i].rm_so;
        if(len){
            memset(match,'\0',sizeof(match));
            memcpy(match,bematch+pmatch[i].rm_so,len);
            //printf("%s\n",match);
        }
    }
    return 0;
}


int taskmoudle()
{

        char filedes[100] = {0};
	char *pattendpoint = "<endpoint id=\"([A-Za-z0-9_]*)\" ";
	char *data = NULL;
	int ret;
	int i = 0;

	if (NULL == getenv("PRO_PATH")){
                 printf("PRO_PATH未设置");
                 return -1;
         }
         snprintf(filedes, sizeof(filedes), "%s/config/endpoint", getenv("PRO_PATH"));


        ret = handle(filedes);
	while(filepath[i]!=NULL&&strlen(filepath[i])>0){
		//printf("%s\n",filepath[i]);
		ret = readfile(filepath[i],&data);
		if(ret!=0)printf("readfile err");
		ret = regxmatch(data,pattendpoint);
		if(ret!=0)printf("regxmatch err");
		//printf("[%s]\n",match);
		ret = findtask(match,NULL,NULL);
		if(ret!=0)printf("findtask err");
		memset(match,0,sizeof(match));
		if(data!=NULL)free(data);data= NULL;
		i++;
	}
        return 0;
}

int mappmoudle()
{

        char filedes[100] = {0};
	char *pattmappendpoint = "<mappings endpoint=\"([A-Za-z0-9_]*)\">";
	char *pattprocendpoint = "<procedures endpoint=\"([A-Za-z0-9_]*)\">";
        char *data = NULL;
        int ret;
        int i = 0;
	char typemapp[] = "consumer";
	char typeproc[] = "provider";


        if (NULL == getenv("PRO_PATH")){
                 printf("PRO_PATH未设置");
                 return -1;
         }
        snprintf(filedes, sizeof(filedes), "%s/config/mapping", getenv("PRO_PATH"));
        ret = handle(filedes);
        while(filepath[i]!=NULL&&strlen(filepath[i])>0){
                //printf("%s\n",filepath[i]);
                ret = readfile(filepath[i],&data);
                if(ret!=0)printf("readfile err");
                ret = regxmatch(data,pattmappendpoint);
                if(ret!=0)printf("regxmatch err");
                //printf("[%s]\n",match);
		ret = findmapp(match,filepath[i],typemapp);
		if(ret!=0)printf("findmapp err");
                memset(match,0,sizeof(match));
                if(data!=NULL)free(data);data= NULL;
                i++;
        }

	memset(filepath,0,sizeof(filepath));
        memset(match,0,sizeof(match));
	count =0;
	i =0;
	snprintf(filedes, sizeof(filedes), "%s/config/procedure", getenv("PRO_PATH"));
	ret = handle(filedes);
        while(filepath[i]!=NULL&&strlen(filepath[i])>0){
                //printf("%s\n",filepath[i]);
                ret = readfile(filepath[i],&data);
                if(ret!=0)printf("readfile err");
                ret = regxmatch(data,pattprocendpoint);
                if(ret!=0)printf("regxmatch err");
                //printf("[%s]\n",match);
                ret = findproc(match,filepath[i],typeproc);
                if(ret!=0)printf("findproc err");
                memset(match,0,sizeof(match));
                if(data!=NULL)free(data);data= NULL;
                i++;
        }
        return 0;
}

int main(int argc, char **argv){
	int ret;
	if (argc != 2) {
        	printf("Usage: %s [-t][-m]\n", argv[0]);
        	return -1;
	}

	if(strstr(argv[1],"-t")){
		printf("<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n");	
		printf("<mappstore>\n");
		memset(filepath,0,sizeof(filepath));
		memset(match,0,sizeof(match));
		count =0;
		ret =  taskmoudle();
		if(ret!=0)printf("taskmoudle err");
		printf("</mappstore>\n");
	}

	if(strstr(argv[1],"-m")){
		printf("<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n");
		printf("<mappstore>\n");
		memset(filepath,0,sizeof(filepath));
		memset(match,0,sizeof(match));
		count =0;
		ret = mappmoudle();
		if(ret!=0)printf("mappmoudle err");
		printf("</mappstore>\n");
	}
	
	return 0;
}
