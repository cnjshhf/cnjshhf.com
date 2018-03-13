#include "findtask.h"


int readfile_task (char **p) {
  FILE * pFile;
  long lSize;
  char * buffer;
  size_t result;
  char propath[50]= {0};

  if (NULL == getenv("PRO_PATH")){
      printf("PRO_PATH未设置");
      return -1;
  }
  snprintf(propath, sizeof(propath), "%s/spring/tasks.xml", getenv("PRO_PATH"));
  //printf("[%s]",propath);
  pFile = fopen ( propath , "rb" );
  if (pFile==NULL) {fputs ("File error",stderr); exit (1);}

  fseek (pFile , 0 , SEEK_END);
  lSize = ftell (pFile);
  rewind (pFile);

  //buffer = (char*) malloc (sizeof(char)*lSize);
  buffer = (char*) malloc (sizeof(char)*lSize);
  if (buffer == NULL) {fputs ("Memory error",stderr); exit (2);}

  result = fread (buffer,1,lSize,pFile);
  if (result != lSize) {fputs ("Reading error",stderr); exit (3);}

  fclose (pFile);
  *p = buffer;
  return 0;
}

int regxmatch_task(char *bematch,char *pattern,char **matchback){

    char errbuf[1024];
    char *match = NULL;
    regex_t reg;
    int err = 0,nm = 10;
    regmatch_t pmatch[nm];

    match = (char*) malloc (sizeof(char)*1024*10);
    memset(match,0,1024*10);

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


char *strslp (char *str)
{
  char * pch = NULL;
  pch = (char*) malloc (sizeof(char)*50);
  memset(pch,0,100);
  
  //printf ("Splitting string \"%s\" into tokens:\n",str);
  pch = strtok (str," ,.-");
  while (pch != NULL)
  {
    //printf ("%s\n",pch);
    pch = strtok (NULL, " ,.-");
  }
  printf ("%s\n",pch); 
  pch[strlen(pch)]='\0';
  return pch;
}



int findtask (const char * ep,char **task,char **port) {
  char *head = NULL;
  char *tail = NULL;
  char *tasknametail = NULL;
  char tmp[1024*10]= {0};
  char endpoint[50] = {0};
  char *matchback = NULL;
  int ret;

  char *bematch = NULL;
  char *pattport = "<entry key=\"port\" value=\"([0-9]*)\" />";
  char *patttaskid = "<property name=\"taskId\" value=\"([A-Za-z0-9_]*)\"></property>";
  char *pattadpater = "class=\"cn.congine.tpbus.adaptor.([A-Za-z]*).";
  char *patthost= "<entry key=\"host\" value=\"([A-Za-z0-9.]*)\"";
  char *patttaskname= "name=\"([^.\"$]+)";

  ret = readfile_task(&bematch);
  if(ret != 0) printf("readfile_task err");

  head = bematch;
  tail = bematch;
  sprintf(endpoint,"name=\"endpoint\" value=\"%s\"",ep);
  //printf("[%s]",endpoint);
  memset(tmp,'\0',sizeof(tmp));
  while(head != NULL)
  {
    //head = strstr(head,"<bean");
    head = strstr(head,"<bean");
    tail = strstr(tail,"</bean>");
    strncpy(tmp,head,tail-head);
    if(strstr(tmp,endpoint))
    {

      if(task!= NULL&&port!=NULL){
        ret = regxmatch_task(tmp,pattport,&matchback);
        if(ret != 0) printf("regxmatch_task err");
        *port = matchback;

	ret = regxmatch_task(tmp,patttaskid,&matchback);
        if(ret != 0) printf("regxmatch_task err");
        *task = matchback;

        return 0;
      }

      ret = regxmatch_task(tmp,patttaskid,&matchback);
      if(ret != 0) printf("regxmatch_task err");
      //printf("\t<macro id=\"%s_task_id\">%s</macro>\n",ep,matchback);
      printf("\t<task taskId=\"%s\">\n",matchback);
      if(matchback!=NULL){free(matchback);matchback = NULL;}
	
      ret = regxmatch_task(tmp,pattport,&matchback);
      if(ret != 0) printf("regxmatch_task err");
      //printf("\t<macro id=\"%s_port\">%s</macro>\n",ep,matchback);
      printf("\t\t<taskPort>%s</taskPort>\n",matchback);
      if(matchback!=NULL){free(matchback);matchback = NULL;}

      ret = regxmatch_task(tmp,patthost,&matchback);
      if(ret != 0) printf("regxmatch_task err");
      //printf("\t<macro id=\"%s_host\">%s</macro>\n",ep,matchback);
      printf("\t\t<taskHost>%s</taskHost>\n",matchback);
      if(matchback!=NULL){free(matchback);matchback = NULL;}

      //printf("[%s]",tmp);
      tasknametail = strstr(head,">");
      strncpy(tmp,head,tasknametail-head);
      tmp[tasknametail-head] = '\0';
      //printf("[%s]",tmp);
      ret = regxmatch_task(tmp,patttaskname,&matchback);
      if(ret != 0) printf("regxmatch_task err");
      //printf("\t<macro id=\"%s_task_name\">%s</macro>\n",ep,matchback);
      printf("\t\t<taskName>%s</taskName>\n",matchback);
      if(matchback!=NULL){free(matchback);matchback = NULL;}

      ret = regxmatch_task(tmp,pattadpater,&matchback);
      if(ret != 0) printf("regxmatch_task err");
      printf("\t\t<taskProtocol>%s</taskProtocol>\n",matchback);
      if(matchback!=NULL){free(matchback);matchback = NULL;}
     
      printf("\t\t<taskEndpoint>%s</taskEndpoint>\n",ep);
      printf("\t</task>\n");


    }
    memset(tmp,'\0',sizeof(tmp));
    head = tail ;
    tail++;
  }

  if(bematch!=NULL){free(bematch);bematch = NULL;}
  return 0;
}
