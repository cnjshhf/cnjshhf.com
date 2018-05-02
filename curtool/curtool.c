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
#include "qstring.h"
#include "confparser.h"
#include <vector>

using namespace std;


char LogName[100];

void TestFunc(int loopnum,Config *g_conf)
{
        FILE *fp;
        char tmp[1024] = {0};
        char cmd[1024] = {0};

	vector<char *>::iterator it;
	for (it = g_conf->vurl.begin();it < g_conf->vurl.end(); it++ )
	{
		snprintf(cmd, sizeof(cmd), "curl %s",*it);
		if ((fp = popen(cmd, "r")) != NULL){
			while(fgets(tmp, sizeof(tmp), fp)!=NULL){
				LOG1("[%s]\n",tmp);
			}
		}
	}
        pclose(fp);
	//LOG1("loopnum:%d\n", loopnum);
}

void freeconf(Config * g_conf)
{
	vector<char *>::iterator it;	
	for (it = g_conf->vurl.begin();it < g_conf->vurl.end(); it++ )
        {
		if(*it != NULL){
			delete *it;
			*it = NULL;
		};
	}
	g_conf->vurl.clear();
	
}

int main(void )
{
	
	int procnum = 10;
	int loopnum = 100;
	int mypid = 0;
	Config *g_conf = NULL;

        g_conf = initconfig();
        loadconfig(g_conf);
	//vector<char *>::iterator it = g_conf->vurl.begin(); 
    	//printf("[%s]",g_conf->vurl);
	



	setLogName("curtool.log");
	int i = 0, j = 0;
	printf("please enter you procNum : \n");
	scanf("%d", &procnum);
	
	printf("please enter you loopnum :\n");
	scanf("%d", &loopnum);
	
	pid_t pid;
	
	for (i=0; i<procnum; i++)
	{
		pid = fork();
		if (pid == 0)//子进程入口
		{
			for (j=0; j<loopnum; j ++)
			{
				TestFunc(j,g_conf);
			}
			freeconf(g_conf);
			exit(0);
		}
	}

	while ( (mypid = waitpid(-1, NULL, WNOHANG)) > 0)
	{
		printf("子进程退出pid:%d \n", mypid);
	}

	
	return 0;
}
