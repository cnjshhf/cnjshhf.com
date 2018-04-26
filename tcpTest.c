#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stddef.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <sys/time.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <net/if.h>
#include <sys/ioctl.h>

#define MAXBACKLOG    SOMAXCONN
#define SOSNDBUFSZ    1024*32  /* 32K */
#define SORCVBUFSZ    1024*32  /* 32K */

/****************************************************************
 * 非阻塞模式连接
 ****************************************************************/
static int connect_nonblock(int sockfd, const struct sockaddr *saddr, socklen_t saddr_len, int timeout)
{
    int ret,err,flags;
    fd_set rset,wset;
    struct timeval tval;
    socklen_t len;

    flags = fcntl(sockfd, F_GETFL, 0);
    fcntl(sockfd, F_SETFL, flags | O_NONBLOCK);

    ret = 0;
    err = 0;

    while (1) {
        if (connect(sockfd, (struct sockaddr *)saddr, saddr_len) == 0) {
            ret = 0;
            break;
        }
        if (errno != EINPROGRESS) {
            ret = -1;
            err = errno;
            break;
        }

        FD_ZERO(&rset);
        FD_SET(sockfd, &rset);
        wset = rset;
        tval.tv_sec = timeout;
        tval.tv_usec = 0;

        if (select(sockfd+1, &rset, &wset, NULL, (timeout ? &tval : NULL)) == 0) {
            ret = -1;
            errno = ETIMEDOUT;
            break;
        }

        if (FD_ISSET(sockfd, &rset) || FD_ISSET(sockfd, &wset)) {
            len = sizeof(err);
            if (getsockopt(sockfd, SOL_SOCKET, SO_ERROR, &err, &len) != 0) {
                ret = -1;
                err = errno;
                break;
            }
            if (err) { /* Solaris pending error */
                ret = -1;
                break;
            }
            break;
        } else {
            printf("select() ok, but fd is not set");
            ret = -1;
            break;
        }
    }
        
    fcntl(sockfd, F_SETFL, flags);
    errno = err;
    return ret;
}
/****************************************************************
 * 检查套接字是否可读
 ****************************************************************/
int readable(int sockfd, int timeout)
{
    int ret;
    fd_set rset;
    struct timeval tval;

    FD_ZERO(&rset);
    FD_SET(sockfd, &rset);
    tval.tv_sec = timeout;
    tval.tv_usec = 0;

    ret = select(sockfd+1, &rset, NULL, NULL, &tval);
    if (ret <= 0) return 0;
    return FD_ISSET(sockfd, &rset)? 1:0;
}

/****************************************************************
 * 检查套接字是否可写
 ****************************************************************/
int writeable(int sockfd, int timeout)
{
    int ret;
    fd_set wset;
    struct timeval tval;

    FD_ZERO(&wset);
    FD_SET(sockfd, &wset);
    tval.tv_sec = timeout;
    tval.tv_usec = 0;

    ret = select(sockfd+1, NULL, &wset, NULL, &tval);
    if (ret <= 0) return 0;
    return FD_ISSET(sockfd, &wset)? 1:0;
}

/****************************************************************
 * 初始化TCP连接
 ****************************************************************/
int tcp_connect(const char *locaddr, const char *host, int port, int timeout)
{
    int  ret,sockfd;
    char service[10];
    struct addrinfo hints,*res;
    struct addrinfo hints_loc,*res_loc = NULL;
        
    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    snprintf(service, sizeof(service), "%d", port);	
    if ((ret = getaddrinfo(host, service, &hints, &res)) != 0) {
        printf("getaddrinfo(%s,%s) error: %s", host, service, gai_strerror(ret));
        return -1;
    }

    if (locaddr && strlen(locaddr) > 0) { 
    /* 绑定本地地址 */
        memset(&hints_loc, 0, sizeof(struct addrinfo));
        hints_loc.ai_family = AF_UNSPEC;
        hints_loc.ai_socktype = SOCK_STREAM;
        if ((ret = getaddrinfo(locaddr, NULL, &hints_loc, &res_loc)) != 0) {
            printf("getaddrinfo(%s,NULL) error: %s", locaddr, gai_strerror(ret));
            freeaddrinfo(res);
            return -1;
        }
        if (res_loc->ai_family == AF_INET) { 
        /* IPv4 */
            ((struct sockaddr_in *)res_loc->ai_addr)->sin_port = 0;
        } else if (res_loc->ai_family == AF_INET6) {
        /* IPv6 */
            ((struct sockaddr_in6 *)res_loc->ai_addr)->sin6_port = 0;
        } else {
            printf("Invalid ai_family value");
            freeaddrinfo(res);
            freeaddrinfo(res_loc);
            return -1;
        }
    }

    while (1) {
        if ((sockfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol)) == -1) {
            printf("socket() error: %s", strerror(errno));
            break;
        }

        if (res_loc) {
        /* 绑定本地地址 */
            if (bind(sockfd, res_loc->ai_addr, res_loc->ai_addrlen) != 0) {
                printf("bind() error: %s", strerror(errno));
                break;
            }
        }
        if (timeout <= 0) { 
        /* 阻塞模式 */
            if (connect(sockfd, res->ai_addr, res->ai_addrlen) != 0) {
                printf("connect() error: %s", strerror(errno));
                break;
            }
        } else { 
         /* 非阻塞模式 */
            if (connect_nonblock(sockfd, res->ai_addr, res->ai_addrlen, timeout) != 0) {
                printf("connect_nonblock() error: %s", strerror(errno));
                break;
            }
        }
        freeaddrinfo(res); 
        if (res_loc) freeaddrinfo(res_loc);
        return sockfd;
    }

    freeaddrinfo(res); 
    if (res_loc) freeaddrinfo(res_loc);
    if (sockfd != -1) close(sockfd);
    return -1;
}
/****************************************************************
 * 接收指定长度数据
 ****************************************************************/
int tcp_read(int sockfd, void *buf, int len, int timeout)
{
    int    nread,flags;
    int    nleft = len;
    int    nsecs = timeout;
    void *pbuf = buf;

    if (timeout <= 0) {
        while (nleft > 0) {
            if ((nread = read(sockfd, pbuf, nleft)) == -1) {
                if (errno == EINTR) continue;
                return -1;
            } else if (nread == 0) {
                errno = ECONNRESET;
                return -1;
            } else {
                nleft -= nread;
                pbuf  += nread;
            }
        }
    } else {
        flags = fcntl(sockfd, F_GETFL, 0);
        fcntl(sockfd, F_SETFL, flags | O_NONBLOCK); 
        /* 设置非阻塞模式 */
        while (nleft > 0 && nsecs > 0) {
            if (readable(sockfd, 1)) {
                if ((nread = read(sockfd, pbuf, nleft)) == -1) {
                    if (errno != EAGAIN) return -1;
                } else if (nread == 0) {
                    errno = ECONNRESET;
                    return -1;
                } else {
                    nleft -= nread;
                    pbuf  += nread;
                }
            }
            nsecs--;
        }
        fcntl(sockfd, F_SETFL, flags);
        if (nsecs == 0 && nleft > 0) {
            errno = ETIMEDOUT;
            return -1;
        }
    }
    return 0;
}

/****************************************************************
 * 接收指定长度数据
 ****************************************************************/
int tcp_write(int sockfd, const void *buf, int len, int timeout)
{
    int     nwrite,flags;
    int     nleft = len;
    int     nsecs = timeout;
    const void *pbuf = buf;

    if (timeout <= 0) {
        while (nleft > 0) {
            if ((nwrite = write(sockfd, pbuf, nleft)) == -1) {
                if (errno == EINTR) continue;
                return -1;
            } else if (nwrite == 0) {
                errno = ECONNRESET;
                return -1;
            } else {
                nleft -= nwrite;
                pbuf    += nwrite;
            }
        }
    } else {
        flags = fcntl(sockfd, F_GETFL, 0);
        fcntl(sockfd, F_SETFL, flags | O_NONBLOCK); 
        /* 设置非阻塞模式 */
        while (nleft > 0 && nsecs > 0) {
            if (writeable(sockfd, 1)) {
                if ((nwrite = write(sockfd, pbuf, nleft)) == -1) {
                    return -1;
                } else if (nwrite == 0) {
                    errno = ECONNRESET;
                    return -1;
                } else {
                    nleft -= nwrite;
                    pbuf    += nwrite;
                }
            }
            nsecs--;
        }
        fcntl(sockfd, F_SETFL, flags);
        if (nsecs == 0 && nleft > 0) {
            errno = ETIMEDOUT;
            return -1;
        }
    }
    return 0;
}

/****************************************************************
 * 发送消息报文(可指定预读长度)
 ****************************************************************/
int tcp_send(int sockfd, const void *buf, int len, int lenlen, int timeout)
{
    char lenbuf[11];

    if (lenlen > 0) {
        if (lenlen >= sizeof(lenbuf)) lenlen = sizeof(lenbuf) - 1;
        snprintf(lenbuf, sizeof(lenbuf), "%*.*d", lenlen, lenlen, len);
        if (tcp_write(sockfd, lenbuf, lenlen, timeout) != 0) return -1;
    }
    return tcp_write(sockfd, buf, len, timeout);
}

/****************************************************************
 * 接收消息报文(可指定预读长度)
 ****************************************************************/
int tcp_recv(int sockfd, void *buf, int len, int lenlen, int timeout)
{
    char lenbuf[11];
    int    nread,flags;
    int    nsecs = timeout;

    if (lenlen > 0) {
        if (lenlen >= sizeof(lenbuf)) lenlen = sizeof(lenbuf) - 1;
        if (tcp_read(sockfd, lenbuf, lenlen, timeout) != 0) return -1;
        lenbuf[lenlen] = 0;
        printf("lenbuf=[%d][%d][%d][%d]\n", lenbuf[0],lenbuf[1],lenbuf[2],lenbuf[3]);
        len = atoi(lenbuf);
        if (tcp_read(sockfd, buf, len, timeout) != 0) return -1;
        return len;
    } else {
        if (len > 0) {
            if (tcp_read(sockfd, buf, len, timeout) != 0) return -1;
            return len;
        }

        if (timeout <= 0) {
            while (1) {
                if ((nread = read(sockfd, buf, SORCVBUFSZ)) == -1) {
                    if (errno == EINTR) continue;
                    return -1;
                } else if (nread == 0) {
                    errno = ECONNRESET;
                    return -1;
                } else {
                    return nread;
                }
            }
        } else {
            flags = fcntl(sockfd, F_GETFL, 0);
            fcntl(sockfd, F_SETFL, flags | O_NONBLOCK); 
            /* 设置非阻塞模式 */
            while (nsecs > 0) {
                if (readable(sockfd, 1)) {
                    if ((nread = read(sockfd, buf, SORCVBUFSZ)) == -1) {
                        return -1;
                    } else if (nread == 0) {
                        errno = ECONNRESET;
                        return -1;
                    } else {
                        fcntl(sockfd, F_SETFL, flags);
                        return nread;
                    }
                }
                nsecs--;
            }
            fcntl(sockfd, F_SETFL, flags);
            errno = ETIMEDOUT;
            return -1;
        }
    }
}


int main(int argc, char *argv[])
{
    int i,len,sockfd;
    char buf[1024*64];
    char sendbuf[1024*64];
    time_t t;
	char sfilename[256];
	FILE *fp;

    if (argc != 4) {
        printf("Usage: %s <IP> <PORT> <FILENAME>\n", argv[0]);
        return -1;
    }

    if ((sockfd = tcp_connect(argv[1], argv[1], atoi(argv[2]), 0)) == -1) {
       printf("tcp_connect() error\n");
       return -1;
    }
            
    sprintf(sfilename, "%s", argv[3]);
    fp = fopen(sfilename, "r");
    if(fp == NULL){
        printf("fopen error [%s]\n", strerror(errno));
        return -1;
     }
     len = fread(buf, sizeof(char),1024*8,fp);
     buf[len] = 0x00;
     fclose(fp);
     sprintf(sendbuf, "%010d%s", strlen(buf), buf);
     //printf("sendbuf[%d]=[%s]\n",strlen(sendbuf), sendbuf);
     //sprintf(sendbuf, "%s",  buf);
     printf("sendbuf=[%s]\n", sendbuf);
            
            if (tcp_send(sockfd, sendbuf, strlen(buf)+10, 10, 0) == -1) {
                printf("tcp_send() error\n");
                close(sockfd);
                return -1;
            }
            printf("sendbuf ok: [%s]\n", sendbuf);
	
            //从服务器读数据
		char revbuf1[1024] = {0};
		int ret1 = read(sockfd, revbuf1, sizeof(revbuf1));	
		printf("%d",ret1);
		fputs(revbuf1, stdout); //从服务器收到数据，打印屏幕
		printf("调试");

            
            /*接收通讯报文*/
            if ((len = tcp_recv(sockfd, buf, len, 0, 0)) == -1) {
                printf("tcp_recv() error: %s\n", strerror(errno));
                close(sockfd);
                return -1;
            }
            buf[len] = 0;
            printf("recv: [%s]\n", buf);
            close(sockfd);
    return 0;
}

