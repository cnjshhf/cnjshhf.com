#include "findmapp.h"
#include "findtask.h"


int readfile_mapp (char **p,char *propath) {
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

int regxmatch_mapp(char *bematch,char *pattern,char **matchback){

    char errbuf[1024];
    char *match = NULL;
    regex_t reg;
    int err = 0,nm = 10;
    regmatch_t pmatch[nm];

    match = (char*) malloc (sizeof(char)*1024);
    memset(match,0,1024);

    if(regcomp(&reg,pattern,REG_EXTENDED) < 0){
        regerror(err,&reg,errbuf,sizeof(errbuf));
        printf("err:%s\n",errbuf);
    }

    err = regexec(&reg,bematch,nm,pmatch,0);

    if(err == REG_NOMATCH){
        //printf("no match\n");
        //printf("未设置端口\n");
	strcpy(match,"空");
	*matchback = match;
        return 0;

    }else if(err){
        regerror(err,&reg,errbuf,sizeof(errbuf));
        printf("err:%s\n",errbuf);
        exit(-1);
    }

    for(int i=0;i<10 && pmatch[i].rm_so!=-1;i++){
        int len = pmatch[i].rm_eo-pmatch[i].rm_so;
        if(len){
            memset(match,'\0',strlen(match)+1);
            memcpy(match,bematch+pmatch[i].rm_so,len);
        }
    }
    //printf("[%s]",match);
    *matchback = match;
    return 0;
}




int findmapp (char * ep, char *filepath,char *type) {
  char *head = NULL;
  char *tail = NULL;
  char tmp[1024*10]= {0};
  char servcode[50] = {0};
  char *matchback = NULL;
  char *task =NULL;
  char *port =NULL;
  int ret;

  char *bematch = NULL;
  char *pattsevcode = "servCode=\"([A-Za-z0-9_]*)\"";
  char *patttrancode = "tranCode=\"([/.A-Za-z0-9?*_]*)\"";
  char *pattpack= "<pack>([A-Za-z0-9_]*)</pack>";
  char *pattunpack= "<unpack>([A-Za-z0-9_]*)</unpack>";
  char *pattmappname= "name=\"([^.\"$]+)";
  

  ret = readfile_mapp(&bematch,filepath);
  if(ret != 0) printf("readfile_task err");

  head = bematch;
  tail = bematch;
  memset(tmp,'\0',sizeof(tmp));
  while(head != NULL)
  {
    head = strstr(head,"<mapping");
    tail = strstr(tail,"</mapping>");
    strncpy(tmp,head,tail-head);
    //printf("[%s]",tmp);

    ret = regxmatch_mapp(tmp,pattsevcode,&matchback);
    if(ret != 0) printf("regxmatch_mapp err");
    //printf("[%s]",matchback);
    strcpy(servcode,matchback);
    if(strstr(servcode,"空"))break;
    if(matchback!=NULL){free(matchback);matchback = NULL;}

    ret = regxmatch_mapp(tmp,pattmappname,&matchback);
    if(ret != 0) printf("regxmatch_mapp err");
    printf("\t<mapp mappName=\"%s\">\n",matchback);
    if(matchback!=NULL){free(matchback);matchback = NULL;}

    ret = findtask(ep,&task,&port);
    if(ret!=0)printf("findtask err");
    if(task!=NULL){printf("\t\t<mappTask>%s</mappTask>\n",task);free(task);task = NULL;}
    if(port!=NULL){printf("\t\t<mappPort>%s</mappPort>\n",port);free(port);port = NULL;}

    printf("\t\t<mappServcode>%s</mappServcode>\n",servcode);

    ret = regxmatch_mapp(tmp,patttrancode,&matchback);
    if(ret != 0) printf("regxmatch_mapp err");
    //printf("\t<macro id=\"%s_%s_mapp_trancode\">%s</macro>\n",ep,servcode,matchback);
    printf("\t\t<mappTrancode>%s</mappTrancode>\n",matchback);
    if(matchback!=NULL){free(matchback);matchback = NULL;}
    
    ret = regxmatch_mapp(tmp,pattpack,&matchback);
    if(ret != 0) printf("regxmatch_mapp err");
    //printf("\t<macro id=\"%s_%s_mapp_pack\">%s</macro>\n",ep,servcode,matchback);
    printf("\t\t<mappPack>%s</mappPack>\n",matchback);
    if(matchback!=NULL){free(matchback);matchback = NULL;}

    ret = regxmatch_mapp(tmp,pattunpack,&matchback);
    if(ret != 0) printf("regxmatch_mapp err");
    //printf("\t<macro id=\"%s_%s_mapp_unpack\">%s</macro>\n",ep,servcode,matchback);
    printf("\t\t<mappUnpack>%s</mappUnpack>\n",matchback);
    if(matchback!=NULL){free(matchback);matchback = NULL;}
    
    //printf("\t<macro id=\"%s_%s_mapp_type\">%s</macro>\n",ep,servcode,type);
    printf("\t\t<mappType>%s</mappType>\n",type);
    printf("\t\t<mappEndpoint>%s</mappEndpoint>\n",ep);
    printf("\t</mapp>\n");

    
    memset(tmp,'\0',sizeof(tmp));
    memset(servcode,'\0',sizeof(servcode));
    head = tail ;
    tail++;
  }

  if(bematch!=NULL){free(bematch);bematch = NULL;}
  return 0;
}

int findproc(char * ep,char *filepath,char *type) {
  char *head = NULL;
  char *tail = NULL;
  char tmp[1024*10]= {0};
  char *matchback = NULL;
  char proccode[50]= {0};
  char *task = NULL;
  char *port = NULL;
  int ret;

  char *bematch = NULL;
  char *pattproccode = "code=\"([A-Za-z0-9_]*)\".*>";
  
  char *pattpack= "<pack>([A-Za-z0-9_]*)</pack>";
  char *pattunpack= "<unpack>([A-Za-z0-9_]*)</unpack>";
  char *pattmappname= "name=\"([^.\"$]+)";
  char *patttrancode = "tranCode=\"([/.A-Za-z0-9?*]*)\"";



  ret = readfile_mapp(&bematch,filepath);
  if(ret != 0) printf("readfile_mapp err");

  head = bematch;
  tail = bematch;
  memset(tmp,'\0',sizeof(tmp));
  while(head != NULL)
  {
    head = strstr(head,"<procedure ");
    tail = strstr(tail,"</procedure>");
    strncpy(tmp,head,tail-head);
    //printf("[%s]",tmp);

    ret = regxmatch_mapp(tmp,pattproccode,&matchback);
    if(ret != 0) printf("regxmatch_mapp err");
    //printf("[%s]",matchback);
    //printf("\t<macro id=\"%s_%s_mapp_servcode\">%s</macro>\n",ep,matchback,matchback);
    strcpy(proccode,matchback);
    if(strstr(proccode,"空"))break; 
    if(matchback!=NULL){free(matchback);matchback = NULL;}

    //printf("[%s]",tmp);
    ret = regxmatch_mapp(tmp,pattmappname,&matchback);
    if(ret != 0) printf("regxmatch_mapp err");
    printf("\t<mapp mappName=\"%s\">\n",matchback);
    if(matchback!=NULL){free(matchback);matchback = NULL;}

    ret = findtask(ep,&task,&port);
    if(ret!=0)printf("findtask err");
    if(task!=NULL){printf("\t\t<mappTask>%s</mappTask>\n",task);free(task);task = NULL;}
    if(port!=NULL){printf("\t\t<mappPort>%s</mappPort>\n",port);free(port);port = NULL;}

    printf("\t\t<mappServcode>%s</mappServcode>\n",proccode);
    if(matchback!=NULL){free(matchback);matchback = NULL;}

    ret = regxmatch_mapp(tmp,pattpack,&matchback);
    if(ret != 0) printf("regxmatch_mapp err");
    //printf("\t<macro id=\"%s_%s_mapp_pack\">%s</macro>\n",ep,proccode,matchback);
    printf("\t\t<mappPack>%s</mappPack>\n",matchback);
    if(matchback!=NULL){free(matchback);matchback = NULL;}

    ret = regxmatch_mapp(tmp,pattunpack,&matchback);
    if(ret != 0) printf("regxmatch_mapp err");
    //printf("\t<macro id=\"%s_%s_mapp_unpack\">%s</macro>\n",ep,proccode,matchback);
    printf("\t\t<mappUnpack>%s</mappUnpack>\n",matchback);
    if(matchback!=NULL){free(matchback);matchback = NULL;}

    ret = regxmatch_mapp(tmp,patttrancode,&matchback);
    if(ret != 0) printf("regxmatch_mapp err");
    //printf("\t<macro id=\"%s_%s_mapp_trancode\">%s</macro>\n",ep,servcode,matchback);
    printf("\t\t<mappTrancode>%s</mappTrancode>\n",matchback);
    if(matchback!=NULL){free(matchback);matchback = NULL;}

    //printf("\t<macro id=\"%s_%s_mapp_type\">%s</macro>\n",ep,proccode,type);
    printf("\t\t<mappEndpoint>%s</mappEndpoint>\n",ep);
    printf("\t\t<mappType>%s</mappType>\n",type);
    printf("\t</mapp>\n");


    memset(tmp,'\0',sizeof(tmp));
    head = tail ;
    tail++;
  }

  if(bematch!=NULL){free(bematch);bematch = NULL;}
  return 0;
}
