/***************************************************************
 * socket.c: 套接字编程函数库
 ***************************************************************
 * get_local_ip():          获取本地IP(v4)地址
 * uds_listen():            初始化域套接字侦听
 * uds_connect():           初始化域套接字连接
 * tcp_listen():            初始化TCP侦听
 * connect_nonblock():      非阻塞模式连接
 * tcp_connect():           初始化TCP连接
 * set_sock_reuse():        设置套接字选项(SO_REUSEADDR)
 * set_sock_linger():       设置套接字选项(SO_LINGER)
 * set_sock_sndbuf():       设置套接字选项(SO_SNDBUF)
 * set_sock_rcvbuf():       设置套接字选项(SO_RCVBUF)
 * set_sock_nodelay():      设置套接字无延迟发送数据
 * set_sock_keepalive():    设置套接字选项(SO_KEEPALIVE)    
 * tcp_accept():            接收客户端连接请求
 * readable():              检查套接字是否可读
 * writeable():             检查套接字是否可写
 * tcp_read():              接收指定长度数据
 * tcp_write():             接收指定长度数据
 * tcp_send():              发送消息报文(可指定预读长度)
 * tcp_recv():              接收消息报文(可指定预读长度)
***************************************************************/
#include "socket.h"

/***************************************************************
 * 获取本地IP(v4)地址
 ***************************************************************/
int get_local_ip(const char *eth, char *ip)
{
    int        sockfd;
    struct ifreq req;
    struct sockaddr_in *saddr;

    ip[0] = 0;
    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) == -1) {
        LOG_ERR("socket() error: %s", strerror(errno));
        return -1;
    }
    strcpy(req.ifr_name, eth);
    if (ioctl(sockfd, SIOCGIFADDR, &req) != 0) {
        LOG_ERR("ioctl(%s) error: %s", req.ifr_name, strerror(errno));
        close(sockfd);
        return -1;
    }
    saddr = (struct sockaddr_in*)&req.ifr_addr;
    strcpy(ip, inet_ntoa(saddr->sin_addr));
    close(sockfd);
    return 0;
}

/****************************************************************
 * 初始化域套接字侦听
 ****************************************************************/
int uds_listen(const char *path)
{
    int        sockfd,addrlen;
    struct sockaddr_un udsaddr;
        
    if ((sockfd = socket(AF_UNIX, SOCK_STREAM, 0)) == -1) {
        LOG_ERR("socket() error: %s", strerror(errno));
        return -1;
    }
        
    unlink(path);
    memset(&udsaddr, 0, sizeof(udsaddr));
    udsaddr.sun_family = AF_UNIX;
    strcpy(udsaddr.sun_path, path);
    addrlen = offsetof(struct sockaddr_un, sun_path) + strlen(path);
        
    if (bind(sockfd, (struct sockaddr *)&udsaddr, addrlen) != 0) {
        LOG_ERR("bind() error: %s", strerror(errno));
        unlink(path);
        close(sockfd);
        return -1;
    }
    if (listen(sockfd, MAXBACKLOG) != 0) {
        LOG_ERR("listen() error: %s", strerror(errno));
        unlink(path);
        close(sockfd);
        return -1;
    }
    return sockfd;
}

/****************************************************************
 * 初始化域套接字连接
 ****************************************************************/
int uds_connect(const char *svr_path, const char *cli_path)
{
    int        sockfd,addrlen;
    struct sockaddr_un udsaddr_svr;
    struct sockaddr_un udsaddr_cli;
        
    if ((sockfd = socket(AF_UNIX, SOCK_STREAM, 0)) == -1) {
        LOG_ERR("socket() error: %s", strerror(errno));
        return -1;
    }

    memset(&udsaddr_svr, 0, sizeof(udsaddr_svr));
    udsaddr_svr.sun_family = AF_UNIX;
    strcpy(udsaddr_svr.sun_path, svr_path);

    memset(&udsaddr_cli, 0, sizeof(udsaddr_cli));
    udsaddr_cli.sun_family = AF_UNIX;
    strcpy(udsaddr_cli.sun_path, cli_path);

    /* 绑定本地地址 */
    addrlen = offsetof(struct sockaddr_un, sun_path) + strlen(udsaddr_cli.sun_path);
    unlink(udsaddr_cli.sun_path);
    if (bind(sockfd, (struct sockaddr *)&udsaddr_cli, addrlen) != 0) {
        LOG_ERR("bind() error: %s", strerror(errno));
        unlink(udsaddr_cli.sun_path);
        close(sockfd);
        return -1;
    }
        
    /* 域套接字不采用非阻塞模式连接 */
    addrlen = offsetof(struct sockaddr_un, sun_path) + strlen(udsaddr_svr.sun_path);
    if (connect(sockfd, (struct sockaddr *)&udsaddr_svr, addrlen) != 0) {
        LOG_ERR("connect() error: %s", strerror(errno));
        unlink(udsaddr_cli.sun_path);
        close(sockfd);
        return -1;
    }
    return sockfd;
}

/****************************************************************
 * 初始化TCP侦听
 ****************************************************************/
int tcp_listen(const char *host, int port)
{
    int  ret,sockfd;
    char service[10];
    struct addrinfo hints,*res;

    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_flags = AI_PASSIVE;
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    snprintf(service, sizeof(service), "%d", port);
    if ((ret = getaddrinfo(host, service, &hints, &res)) != 0) {
        LOG_ERR("getaddrinfo(%s,%s) error: %s", host, service, gai_strerror(ret));
        return -1;
    }
	
    while (1) {
        sockfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
        if (sockfd == -1) {
            LOG_ERR("socket() error: %s", strerror(errno));
            break;			
        }
        if (set_sock_reuse(sockfd, 1) != 0) break;

        if (bind(sockfd, res->ai_addr, res->ai_addrlen) == -1) {
            LOG_ERR("bind() error: %s", strerror(errno));
            break;
        } 
        if (listen(sockfd, MAXBACKLOG) != 0) {
            LOG_ERR("listen() error: %s", strerror(errno));
            break;
        }
        freeaddrinfo(res);
        return sockfd;
    }

    freeaddrinfo(res);
    if (sockfd != -1) close(sockfd);
    return -1;
}

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
            LOG_ERR("select() ok, but fd is not set");
            ret = -1;
            break;
        }
    }
        
    fcntl(sockfd, F_SETFL, flags);
    errno = err;
    return ret;
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
        LOG_ERR("getaddrinfo(%s,%s) error: %s", host, service, gai_strerror(ret));
        return -1;
    }

    if (locaddr && strlen(locaddr) > 0) { 
    /* 绑定本地地址 */
        memset(&hints_loc, 0, sizeof(struct addrinfo));
        hints_loc.ai_family = AF_UNSPEC;
        hints_loc.ai_socktype = SOCK_STREAM;
        if ((ret = getaddrinfo(locaddr, NULL, &hints_loc, &res_loc)) != 0) {
            LOG_ERR("getaddrinfo(%s,NULL) error: %s", locaddr, gai_strerror(ret));
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
            LOG_ERR("Invalid ai_family value");
            freeaddrinfo(res);
            freeaddrinfo(res_loc);
            return -1;
        }
    }

    while (1) {
        if ((sockfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol)) == -1) {
            LOG_ERR("socket() error: %s", strerror(errno));
            break;
        }
        if (set_sock_linger(sockfd, 1, 1) != 0) break;
        if (set_sock_sndbuf(sockfd, SOSNDBUFSZ) != 0) break;
        if (set_sock_rcvbuf(sockfd, SORCVBUFSZ) != 0) break;

        if (res_loc) {
        /* 绑定本地地址 */
            if (bind(sockfd, res_loc->ai_addr, res_loc->ai_addrlen) != 0) {
                LOG_ERR("bind() error: %s", strerror(errno));
                break;
            }
        }
        if (timeout <= 0) { 
        /* 阻塞模式 */
            if (connect(sockfd, res->ai_addr, res->ai_addrlen) != 0) {
                LOG_ERR("connect() error: %s", strerror(errno));
                break;
            }
        } else { 
         /* 非阻塞模式 */
            if (connect_nonblock(sockfd, res->ai_addr, res->ai_addrlen, timeout) != 0) {
                LOG_ERR("connect_nonblock() error: %s", strerror(errno));
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
 * 设置套接字选项(SO_REUSEADDR)
 ****************************************************************/
int set_sock_reuse(int sockfd, int onoff)
{
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &onoff, sizeof(int)) == -1 ) {
        LOG_ERR("setsockopt(SO_REUSEADDR) error: %s", strerror(errno));
        return -1;
    }
    return 0;
}

/****************************************************************
 * 设置套接字选项(SO_LINGER)
 ****************************************************************/
int set_sock_linger(int sockfd, int onoff, int nsecs)
{
    struct linger lg;

    lg.l_onoff    = onoff;
    lg.l_linger = nsecs;
    
    if (setsockopt(sockfd, SOL_SOCKET, SO_LINGER, &lg, sizeof(lg)) == -1 ) {
        LOG_ERR("setsockopt(SO_LINGER) error: %s", strerror(errno));
        return -1;
    }
    return 0;
}

/****************************************************************
 * 设置套接字选项(SO_SNDBUF)
 ****************************************************************/
int set_sock_sndbuf(int sockfd, int size)
{
    if (setsockopt(sockfd, SOL_SOCKET, SO_SNDBUF, &size, sizeof(int)) == -1 ) {
        LOG_ERR("setsockopt(SO_SNDBUF) error: %s", strerror(errno));
        return -1;
    }
    return 0;
}
/****************************************************************
 * 设置套接字选项(SO_RCVBUF)
 ****************************************************************/
int set_sock_rcvbuf(int sockfd, int size)
{
    if (setsockopt(sockfd, SOL_SOCKET, SO_RCVBUF, &size, sizeof(int)) == -1 ) {
        LOG_ERR("setsockopt(SO_RCVBUF) error: %s", strerror(errno));
        return -1;
    }
    return 0;
}

/****************************************************************
 * 设置套接字无延迟发送数据
 ****************************************************************/
int set_sock_nodelay(int sockfd, int on)
{
    if (setsockopt(sockfd, IPPROTO_TCP, TCP_NODELAY, &on, sizeof(on)) == -1) {
        LOG_ERR("setsockopt(TCP_NODELAY) error: %s", strerror(errno));
        return -1;
    }
    return 0;
}

/****************************************************************
 * 设置套接字选项(SO_KEEPALIVE)
 ****************************************************************/
int set_sock_keepalive(int sockfd, int idleval, int interval, int cnt)
{
    int onoff = 1;

    if (setsockopt(sockfd, SOL_SOCKET, SO_KEEPALIVE, &onoff, sizeof(int)) == -1 ) {
        LOG_ERR("setsockopt(SO_KEEPALIVE) error: %s", strerror(errno));
        return -1;
    }
    if (setsockopt(sockfd, IPPROTO_TCP, TCP_KEEPIDLE, &idleval, sizeof(int)) == -1 ) {
        LOG_ERR("setsockopt(TCP_KEEPIDLE) error: %s", strerror(errno));
        return -1;
    }
    if (setsockopt(sockfd, IPPROTO_TCP, TCP_KEEPINTVL, &interval, sizeof(int)) == -1 ) {
        LOG_ERR("setsockopt(TCP_KEEPINTVL) error: %s", strerror(errno));
        return -1;
    }
    if (setsockopt(sockfd, IPPROTO_TCP, TCP_KEEPCNT, &cnt, sizeof(int)) == -1 ) {
        LOG_ERR("setsockopt(TCP_KEEPCNT) error: %s", strerror(errno));
        return -1;
    }
    return 0;
}

/****************************************************************
 * 接收客户端连接请求
 ****************************************************************/
int tcp_accept(int sockfd, char *addr, int *port)
{
    int ret;
    struct sockaddr saddr;
    socklen_t saddr_len = sizeof(saddr);

    memset(&saddr, 0, sizeof(saddr));
    while (1) {
        if ((ret = accept(sockfd, &saddr, &saddr_len)) == -1) {
            if (errno == EINTR) continue;
            LOG_ERR("accept() error: %s", strerror(errno));
            return -1;
        }
        break; 
    }

    if (addr) {
        if (saddr.sa_family == AF_INET) {
            inet_ntop(AF_INET, &(((struct sockaddr_in *)&saddr)->sin_addr), addr, INET_ADDRSTRLEN);
        } else if (saddr.sa_family == AF_INET6) {
            inet_ntop(AF_INET6, &(((struct sockaddr_in6 *)&saddr)->sin6_addr), addr, INET6_ADDRSTRLEN);
        } else {
            addr[0] = 0;
        }
    }
    if (port) {
        if (saddr.sa_family == AF_INET) {
            *port = ntohs(((struct sockaddr_in *)&saddr)->sin_port);
        } else if (saddr.sa_family == AF_INET6) {
            *port = ntohs(((struct sockaddr_in6 *)&saddr)->sin6_port);
        } else {
            *port = 0;
        }
    }
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

/****************************************************************
 * 初始化UDP侦听端口
 ****************************************************************/
int udp_init(const char *host, int port)
{
    int  ret,sockfd;
    char service[10];
    struct addrinfo hints,*res;

    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_flags = AI_PASSIVE;
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_DGRAM;
    snprintf(service, sizeof(service), "%d", port);
    if ((ret = getaddrinfo(host, service, &hints, &res)) != 0) {
        LOG_MSG("getaddrinfo(%s,%s) error: %s", host, service, gai_strerror(ret));
        return -1;
    }

    while (1) {
        sockfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
        if (sockfd == -1) {
            LOG_MSG("socket() error: %s", strerror(errno));
            break;			
        }
        if (set_sock_reuse(sockfd, 1) != 0) break;

        if (bind(sockfd, res->ai_addr, res->ai_addrlen) == -1) {
            LOG_MSG("bind() error: %s", strerror(errno));
            break;
        } 
        freeaddrinfo(res);
        return sockfd;
    }

    freeaddrinfo(res);
    if (sockfd != -1) close(sockfd);
    return -1;
}

/****************************************************************
 * 发送UDP报文
 ****************************************************************/
int udp_send(int sockfd, const char *buf, int len, const char *host, int port)
{
    int  ret;
    char service[10];
    struct sockaddr saddr;
    struct addrinfo hints,*res;

    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_flags = AI_PASSIVE;
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_DGRAM;
    snprintf(service, sizeof(service), "%d", port);
    if ((ret = getaddrinfo(host, service, &hints, &res)) != 0) {
        LOG_MSG("getaddrinfo(%s,%s) error: %s", host, service, gai_strerror(ret));
        return -1;
    }

    ret = sendto(sockfd, buf, len, 0, res->ai_addr, sizeof(saddr));
    if (ret == -1) {
        LOG_MSG("sendto() error: %s", strerror(errno));
            freeaddrinfo(res);
        return -1;
    }
    if (ret < len) {
        LOG_MSG("sendto(): some characters not be sent, [len=%d,ret=%d]");
            freeaddrinfo(res);
        return -1;
    }
            freeaddrinfo(res);
    return len;
}

/****************************************************************
 * 接收UDP报文
 ****************************************************************/
int udp_recv(int sockfd, char *buf, int len, char *addr, int *port)
{
    struct    sockaddr saddr;
    socklen_t saddr_len = sizeof(saddr);

    memset(&saddr, 0, sizeof(saddr));
    len = recvfrom(sockfd, buf, len, 0, &saddr, &saddr_len);
    if (len == -1) {
        LOG_ERR("recvfrom() error: %s", strerror(errno));
        return -1;
    }

    if (addr) {
        if (saddr.sa_family == AF_INET) {
            inet_ntop(AF_INET, &(((struct sockaddr_in *)&saddr)->sin_addr), addr, INET_ADDRSTRLEN);
        } else if (saddr.sa_family == AF_INET6) {
            inet_ntop(AF_INET6, &(((struct sockaddr_in6 *)&saddr)->sin6_addr), addr, INET6_ADDRSTRLEN);
        } else {
            addr[0] = 0;
        }
    }
    if (port) {
        if (saddr.sa_family == AF_INET) {
            *port = ntohs(((struct sockaddr_in *)&saddr)->sin_port);
        } else if (saddr.sa_family == AF_INET6) {
            *port = ntohs(((struct sockaddr_in6 *)&saddr)->sin6_port);
        } else {
            *port = 0;
        }
    }
    return len;
}
