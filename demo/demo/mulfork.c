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

//#define LOG1(...) LOG(strerror(errno),__FILE__,__LINE__,__VA_ARGS__)
#define LOG1(...) LOG(__FILE__,__LINE__,__VA_ARGS__)

char LogName[100];
int setLogName(char *str)
{
	memset(LogName,0,sizeof(LogName));
	strcpy(LogName,str);
	return 0;
}

void LOG(char *fl,int li,const char* ms, ...)
{
	char wzLog[1024] = { 0 };
	char buffer[1024] = { 0 };
	va_list args;
	va_start(args, ms);
	vsprintf(wzLog, ms, args);
	va_end(args);

	time_t now;
	time(&now);
	struct tm *local;
	local = localtime(&now);
	//printf("%04d-%02d-%02d %02d:%02d:%02d", local->tm_year + 1900, local->tm_mon,local->tm_mday, local->tm_hour, local->tm_min, local->tm_sec);
	//printf("|%d|%s",getpgid(0),wzLog);
	sprintf(buffer, "%04d-%02d-%02d %02d:%02d:%02d |%d|%s->%d%s", local->tm_year + 1900, local->tm_mon,
		local->tm_mday, local->tm_hour, local->tm_min, local->tm_sec,getpgid(0),fl,li,
		wzLog);
	//sprintf(buffer+strlen(buffer),"errinf=[%s]\n",err);
	//printf("%s",buffer);
	FILE* file = fopen(LogName, "a+");
	fwrite(buffer, 1, strlen(buffer), file);
	fclose(file);

	//  syslog(LOG_INFO,wzLog);  

}



void TestFunc(int loopnum)
{
        FILE *fp;
        char tmp[1024] = {0};
        char cmd[1024] = {0};

        snprintf(cmd, sizeof(cmd), "curl http://192.168.59.148:80/test.do");
        if ((fp = popen(cmd, "r")) != NULL){
                while(fgets(tmp, sizeof(tmp), fp)!=NULL){
                        LOG1("[%s]\n",tmp);
                }
        }
        pclose(fp);
	//LOG1("loopnum:%d\n", loopnum);
}

int main(void )
{
	
	int procnum = 10;
	int loopnum = 100;
	int mypid = 0;

	setLogName("test.log");
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
				TestFunc(j);
			}
			exit(0);
		}
	}

	while ( (mypid = waitpid(-1, NULL, WNOHANG)) > 0)
	{
		printf("子进程退出pid:%d \n", mypid);
	}
	
	return 0;
}
