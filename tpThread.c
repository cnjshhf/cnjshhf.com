/***************************************************************
 * 线程函数库
 **************************************************************/
#include <pthread.h>

#define TPSTKSZ  1048*1024  /*1M*/

/***************************************************************
 * 初始化线程数据 
 **************************************************************/
int tpThreadInitTSD(pthread_key_t *key)
{
    int ret;

    if ((ret = pthread_key_create(key, NULL)) != 0) return ret;
    return pthread_setspecific(*key, NULL);
}

/***************************************************************
 * 创建线程
 **************************************************************/
int tpThreadCreate(void *(*start_routine)(void *), void *arg)
{
    pthread_t tid;
    pthread_attr_t attr;

    if (pthread_attr_init(&attr) != 0) return -1;
    if (pthread_attr_setstacksize(&attr, TPSTKSZ) != 0) return -1;
    if (pthread_create(&tid, &attr, start_routine, arg) != 0) return -1;
    pthread_attr_destroy(&attr);
    return 0;
}

/***************************************************************
 * 终止线程
 **************************************************************/
void tpThreadExit()
{
    pthread_detach(pthread_self());
    pthread_exit(NULL);
}
