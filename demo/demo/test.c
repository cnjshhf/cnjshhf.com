#include <unistd.h>
#include <sys/types.h>

#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>

#include <pthread.h>

int g_Count = 0;

int 	nNum, nLoop;

//定义锁
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

//定义条件并初始化
pthread_cond_t my_condition=PTHREAD_COND_INITIALIZER;

#define CUSTOM_COUNT 2
#define PRODUCT_COUNT 4

void *consume(void* arg)
{
	
	int inum = 0;
	inum = (int)arg;
	while(1)
	{
		pthread_mutex_lock(&mutex);
		printf("consum:%d\n", inum);
		while (g_Count == 0)  //while 醒来以后需要重新判断 条件g_Count是否满足，如果不满足，再次wait
		{
			printf("consum:%d 开始等待\n", inum);
			pthread_cond_wait(&my_condition, &mutex); //api做了三件事情 //pthread_cond_wait假醒
			printf("consum:%d 醒来\n", inum);
		}
	
		printf("consum:%d 消费产品begin\n", inum);
		g_Count--; //消费产品
		printf("consum:%d 消费产品end\n", inum);
		
		pthread_mutex_unlock(&mutex);
	
		sleep(1);
	}

	pthread_exit(0);

} 

//生产者线程
void *produce(void* arg)
{
	int inum = 0;
	inum = (int)arg;
	
	while(1)
	{
	
		pthread_mutex_lock(&mutex);
		printf("产品数量:%d\n", g_Count);
		printf("produce:%d 生产产品begin\n", inum);
		g_Count++;
		//只要我生产出一个产品，就告诉消费者去消费
		printf("produce:%d 生产产品end\n", inum);
		
		printf("produce:%d 发条件signal begin\n", inum);
		pthread_cond_signal(&my_condition); //通知，在条件上等待的线程 
		printf("produce:%d 发条件signal end\n", inum);
		
		pthread_mutex_unlock(&mutex);
		sleep(1);
	}
	
	pthread_exit(0);

} 

int handle()
{
        FILE *fp;
        char tmp[1024*10] = {0};
        char cmd[1024] = {0};

        snprintf(cmd, sizeof(cmd), "curl http://192.168.59.148:80/test.do");
        if ((fp = popen(cmd, "r")) != NULL){
                while(fgets(tmp, sizeof(tmp), fp)!=NULL){
                        printf("[%s]",tmp);
                }
        }
        pclose(fp);
        return 0;
}


int main()
{
	int		i =0;
	pthread_t	tidArray[CUSTOM_COUNT+PRODUCT_COUNT+10];
	
	//创建消费者线程
	for (i=0; i<CUSTOM_COUNT; i++)
	{
		pthread_create(&tidArray[i], NULL, consume, (void *)i);
	}
	
	sleep(1);
	//创建生产线程
	for (i=0; i<PRODUCT_COUNT; i++)
	{
		pthread_create(&tidArray[i+CUSTOM_COUNT], NULL, produce, (void*)i);
	}
	
	
	
	for (i=0; i<CUSTOM_COUNT+PRODUCT_COUNT; i++)
	{
		pthread_join(tidArray[i], NULL); //等待线程结束。。。
	}
	
	
	printf("进程也要结束1233\n");
	
	return 0;
}
