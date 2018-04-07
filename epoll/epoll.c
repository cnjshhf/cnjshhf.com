#include <deque>
#include <map>
#include <vector>
#include <pthread.h>
#include <semaphore.h>
#include <time.h>
#include <sys/time.h>
#include <sys/shm.h>
#include <errno.h>
#include <sys/types.h>
#include <fcntl.h>
#include <stdio.h>
#include <strings.h>

#include <string>
#include <cstdio>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <cstdlib>
#include <cctype>
#include <sstream>
#include <utility>
#include <stdexcept>

#include <sys/socket.h> 
#include <sys/epoll.h> 
#include <netinet/in.h> 
#include <arpa/inet.h> 
#include <iostream>
#include <signal.h>

using namespace std;

#define MAXLINE 5
#define LISTENQ 5
#define SERV_PORT 5000

bool bWrite = false;

void setnonblocking(int sock) 
{     
    int opts;     
    opts=fcntl(sock,F_GETFL);     
    if(opts<0)     
    {         
        perror("fcntl(sock,GETFL)");         
        exit(1);     
    }     
    opts = opts|O_NONBLOCK;     
    if(fcntl(sock,F_SETFL,opts)<0)     
    {         
        perror("fcntl(sock,SETFL,opts)");         
        exit(1);     
    }  
}

static void sig_pro(int signum)
{
    cout << "recv signal:" << signum << endl;
}

int main(int argc, char* argv[])
{
    int i, n, listenfd, connfd, nfds;
    char line[MAXLINE + 1];     
    socklen_t clilen;                     //声明epoll_event结构体的变量,ev用于注册事件,数组用于回传要处理的事件     
    struct epoll_event ev,events[20];     //生成用于处理accept的epoll专用的文件描述符     
    int epfd=epoll_create(256);     
    struct sockaddr_in clientaddr;     
    struct sockaddr_in serveraddr;     
    
    //为让应用程序不必对慢速系统调用的errno做EINTR检查,可以采取两种方式:1.屏蔽中断信号,2.处理中断信号
    //1.由signal()函数安装的信号处理程序，系统默认会自动重启动被中断的系统调用，而不是让它出错返回，
    //  所以应用程序不必对慢速系统调用的errno做EINTR检查，这就是自动重启动机制.
    //2.对sigaction()的默认动作是不自动重启动被中断的系统调用，
    //  因此如果我们在使用sigaction()时需要自动重启动被中断的系统调用，就需要使用sigaction的SA_RESTART选项

    //忽略信号  
    //sigset_t newmask;
    //sigemptyset(&newmask);
    //sigaddset(&newmask, SIGINT);
    //sigaddset(&newmask, SIGUSR1);
    //sigaddset(&newmask, SIGUSR2);
    //sigaddset(&newmask, SIGQUIT);
    //pthread_sigmask(SIG_BLOCK, &newmask, NULL);
    
    //处理信号
    //默认自动重启动被中断的系统调用,而不是让它出错返回,应用程序不必对慢速系统调用的errno做EINTR检查
    //signal(SIGINT, sig_pro);
    //signal(SIGUSR1, sig_pro);
    //signal(SIGUSR2, sig_pro);
    //signal(SIGQUIT, sig_pro);

    struct sigaction sa;
    sa.sa_flags = SA_RESTART;   //SA_RESART:自动重启动被中断的系统调用,0:默认不自动重启动被中断的系统调用
    sa.sa_handler = sig_pro;
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGUSR1, &sa, NULL);
    sigaction(SIGUSR2, &sa, NULL);
    sigaction(SIGQUIT, &sa, NULL);
    
    /*//系统调用被中断信号中断的测试验证
    char buf[1024];
    int nn;

    while(1) {
        if((nn = read(STDIN_FILENO, buf, 1024)) == -1) {
            if(errno == EINTR)
                printf("read is interrupted\n");
        }
        else {
            write(STDOUT_FILENO, buf, nn);
      }
    }
    
    return 0;*/
    
    listenfd = socket(AF_INET, SOCK_STREAM, 0);     
    //把socket设置为非阻塞方式     
    setnonblocking(listenfd);     
    //设置与要处理的事件相关的文件描述符     
    ev.data.fd=listenfd;     
    //设置要处理的事件类型     
    ev.events=EPOLLIN|EPOLLET;        
    //注册epoll事件     
    epoll_ctl(epfd,EPOLL_CTL_ADD,listenfd,&ev);     
    bzero(&serveraddr, sizeof(serveraddr));     
    serveraddr.sin_family = AF_INET;     
    serveraddr.sin_addr.s_addr = htonl(INADDR_ANY);   
    serveraddr.sin_port=htons(SERV_PORT);     
    bind(listenfd,(sockaddr *)&serveraddr, sizeof(serveraddr));     
    listen(listenfd, LISTENQ);

    for ( ; ; ) 
    {
        cout << "active" << endl;
        
        //等待epoll事件的发生         
        nfds=epoll_wait(epfd,events,20,500);         
        //处理所发生的所有事件             
        for (i = 0; i < nfds; ++i)         
        {
            if (events[i].data.fd < 0)
            {
                continue;
            }
      
            if (events[i].data.fd == listenfd)      //监听上的事件    
            {
                cout << "[conn] events=" << events[i].events << endl;
                
                if (events[i].events&EPOLLIN)   //有连接到来
                {
                    do
                    {
                        clilen = sizeof(struct sockaddr);               
                        connfd = accept(listenfd,(sockaddr *)&clientaddr, &clilen);                
                        if (connfd > 0)
                        {
                            cout << "[conn] peer=" << inet_ntoa(clientaddr.sin_addr) << ":" << ntohs(clientaddr.sin_port) << endl; 
                            
                            //把socket设置为非阻塞方式
                            setnonblocking(connfd);                            
                            //设置用于读操作的文件描述符                
                            ev.data.fd=connfd;                 
                            //设置用于注测的读操作事件                 
                            ev.events=EPOLLIN|EPOLLET;               
                            //注册ev                 
                            epoll_ctl(epfd,EPOLL_CTL_ADD,connfd,&ev);
                        }
                        else
                        {
                            cout << "[conn] errno=" << errno << endl;
                            
                            if (errno == EAGAIN)    //没有连接需要接收了
                            {
                                break;
                            }
                            else if (errno == EINTR)    //可能被中断信号打断,,经过验证对非阻塞socket并未收到此错误,应该可以省掉该步判断
                            {
                                ;
                            }
                            else    //其它情况可以认为该描述字出现错误,应该关闭后重新监听
                            {
                                cout << "[conn] close listen because accept fail and errno not equal eagain or eintr" << endl;
                                
                                //此时说明该描述字已经出错了,需要重新创建和监听
                                close(events[i].data.fd);                     
                                epoll_ctl(epfd,EPOLL_CTL_DEL,events[i].data.fd,&events[i]);
                                
                                //重新监听
                                listenfd = socket(AF_INET, SOCK_STREAM, 0);        
                                setnonblocking(listenfd);         
                                ev.data.fd=listenfd;         
                                ev.events=EPOLLIN|EPOLLET;            
                                epoll_ctl(epfd,EPOLL_CTL_ADD,listenfd,&ev);       
                                bind(listenfd,(sockaddr *)&serveraddr, sizeof(serveraddr));     
                                listen(listenfd, LISTENQ);
                                break;
                            }
                        }
                    } while (1);
                }
                else if (events[i].events&EPOLLERR || events[i].events&EPOLLHUP)    //有异常发生
                {
                    cout << "[conn] close listen because epollerr or epollhup" << errno << endl;
                    
                    close(events[i].data.fd);                     
                    epoll_ctl(epfd,EPOLL_CTL_DEL,events[i].data.fd,&events[i]);

                    //重新监听
                    listenfd = socket(AF_INET, SOCK_STREAM, 0);        
                    setnonblocking(listenfd);         
                    ev.data.fd=listenfd;         
                    ev.events=EPOLLIN|EPOLLET;            
                    epoll_ctl(epfd,EPOLL_CTL_ADD,listenfd,&ev);        
                    bind(listenfd,(sockaddr *)&serveraddr, sizeof(serveraddr));     
                    listen(listenfd, LISTENQ);
                }
            }
            else    //连接上的事件
            {
                cout << "[data] events=" << events[i].events << endl;
                
                if (events[i].events&EPOLLIN)   //有数据可读          
                {       
                    do
                    {
                        n = read(events[i].data.fd, line, MAXLINE);
                        if (n > 0)  //读到数据
                        {
                            line[n] = '\0';

                            //综合下面两种情况,在读到字节数大于0时必须继续读,不管读到字节数是否等于接收缓冲区大小,
                            //也不管错误代码是否为EAGAIN,否则要么导致关闭事件丢失,要么导致后续数据的丢失
                            if (n < MAXLINE)
                            {
                                //经过验证,如果对方发送完数据后就断开,即使判断是否错误代码为EAGAIN,也会导致close事件丢失,
                                //必须继续读,以保证断开事件的正常接收
                                cout << "[data] n > 0, read less recv buffer size, errno=" << errno << ",len=" << n << ", data=" << line << endl;
                            }
                            else
                            {
                                //经过验证,发送字节数大于等于接收缓冲区时,读到字节数为接收缓冲区大小,错误代码为EAGAIN,
                                //必须继续读,以保证正常接收后续数据
                                cout << "[data] n > 0, read equal recv buffer size, errno=" << errno << ",len=" << n << ", data=" << line << endl;
                            }
                        }
                        else if (n < 0) //读取失败
                        {
                            if (errno == EAGAIN)    //没有数据了
                            {
                                cout << "[data] n < 0, no data, errno=" << errno << endl;
                                
                                break;
                            }
                            else if(errno == EINTR)     //可能被内部中断信号打断,经过验证对非阻塞socket并未收到此错误,应该可以省掉该步判断
                            {
                                cout << "[data] n < 0, interrupt, errno=" << errno << endl;
                            }
                            else    //客户端主动关闭
                            {
                                cout << "[data] n < 0, peer close, errno=" << errno << endl;
                                
                                close(events[i].data.fd);                     
                                epoll_ctl(epfd,EPOLL_CTL_DEL,events[i].data.fd,&events[i]);
                                break;
                            }
                        }
                        else if (n == 0) //客户端主动关闭
                        {
                            cout << "[data] n = 0, peer close, errno=" << errno << endl;
                            
                            //同一连接可能会出现两个客户端主动关闭的事件,一个errno是EAGAIN(11),一个errno是EBADF(9),
                            //对错误的文件描述符EBADF(9)进行关闭操作不会有什么影响,故可以忽略,以减少errno判断的开销
                               
                            close(events[i].data.fd);                     
                            epoll_ctl(epfd,EPOLL_CTL_DEL,events[i].data.fd,&events[i]);
                            break;      
                        }
                    } while (1);
                }
                else if (events[i].events&EPOLLOUT)     //可以写数据
                {
                    cout << "[data] epollout" << endl;
                    
                    if (events[i].data.u64 >> 32 == 0x01)   //假定0x01代表关闭连接
                    {
                        //在需要主动断开连接时仅注册此事件不含可读事件,用来处理服务端主动关闭
                        close(events[i].data.fd);                     
                        epoll_ctl(epfd,EPOLL_CTL_DEL,events[i].data.fd,&events[i]);
                    }
                    else    //其它情况可以去设置该连接的可写标志
                    {
                        bWrite = true;
                    }
                }
                else if (events[i].events&EPOLLERR || events[i].events&EPOLLHUP)    //有异常发生
                {
                    cout << "[data] close peer because epollerr or epollhup" << endl;
                    
                    close(events[i].data.fd);                     
                    epoll_ctl(epfd,EPOLL_CTL_DEL,events[i].data.fd,&events[i]);
                }
            }
        }    
    }
    return 0;
}

ssize_t mysend(int socket, const void *buffer, size_t length, int flags)
{
    ssize_t tmp;
    size_t left = length;
    const char *p = (const char *)buffer;

    while (left > 0)
    {
        if (bWrite)     //判断该连接的可写标志
        {
            tmp = send(socket, p, left, flags);
            if (tmp < 0)
            {
                //当socket是非阻塞时,如返回此错误,表示写缓冲队列已满,
                if (errno == EAGAIN)
                {
                    //设置该连接的不可写标志
                    bWrite = false;
                    
                    usleep(20000);
                    continue;
                }
                else if (errno == EINTR)
                {
                    //被中断信号打断的情况可以忽略,经过验证对非阻塞socket并未收到此错误,应该可以省掉该步判断
                }
                else
                {
                    //其它情况下一般都是连接出现错误了,外部采取关闭措施
                    break;
                }
            }
            else if ((size_t)tmp == left)
            {
                break;
            }
            else
            {
                left -= tmp;
                p += tmp;
            }
        }
        else
        {
            usleep(20000);
        }
    }

    return (ssize_t)(length - left);
}

