#include "socketsops.h"

#include "../base/types.h"
#include "../base/log.h"
#include "endian.h"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h> // snprintf
#include <string.h> //bzero
#include <unistd.h>
#include <assert.h>

using namespace kaycc;
using namespace kaycc::net;

//匿名空间，在编译单元内可见，内部会生成一个唯一标识
namespace {
//地址类型重新定义  
typedef struct sockaddr SA;

//系统不支持accept4函数
#if VALGRIND || defined (NO_ACCEPT4) 
void setNonBlockAndCloseOnExec(int sockfd) {
    // non-block
    int flags = ::fcntl(sockfd, F_GETFL, 0);
    flags |= O_NONBLOCK;
    int ret = ::fcntl(sockfd, F_SETFL, flags);
    // FIXME check

    flags = ::fcntl(sockfd, F_GETFL, 0);
    flags |= FD_CLOEXEC;
    ret = ::fcntl(sockfd, F_SETFL, flags);
    // FIXME check

    (void)ret;
}
#endif

}

const struct sockaddr* sockets::sockaddr_cast(const struct sockaddr_in6* addr) {
    return static_cast<const struct sockaddr*>(implicit_cast<const void*>(addr));
}

struct sockaddr*  sockets::sockaddr_cast(struct sockaddr_in6* addr) {
    return static_cast<struct sockaddr*>(implicit_cast<void*>(addr));
}

const struct sockaddr* sockets::sockaddr_cast(const struct sockaddr_in* addr) {
    return static_cast<const struct sockaddr*>(implicit_cast<const void*>(addr));
}

const struct sockaddr_in* sockets::sockaddr_in_cast(const struct sockaddr* addr) {
    return static_cast<const struct sockaddr_in*>(implicit_cast<const void*>(addr));
}

const struct sockaddr_in6* sockets::sockaddr_in6_cast(const struct sockaddr* addr) {
    return static_cast<const struct sockaddr_in6*>(implicit_cast<const void*>(addr));
}


int sockets::createNonblockingOrDie(sa_family_t family) {
    //IPPROTO_TCP改为0的话，那么系统会根据地址格式和套接字类别，自动选择一个适合的协议。
#if VALGRIND
    int sockfd = ::socket(family, SOCK_STREAM, IPPROTO_TCP);
    if (sockfd < 0) {
        LOG << "sockets::createNonblockingOrDie faild: " << sockfd << std::endl;
    }

    //设置非阻塞和close on exec 
    setNonBlockAndCloseOnExec(sockfd);

#else
    int sockfd = ::socket(family, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, IPPROTO_TCP); 
    if (sockfd < 0) {
        LOG << "sockets::createNonblockingOrDie faild: " << sockfd << std::endl;
    }
#endif

    return sockfd;
}

void sockets::bindOrDie(int sockfd, const struct sockaddr* addr) {
    int ret = ::bind(sockfd, addr, static_cast<socklen_t>(sizeof(struct sockaddr_in6)));
    if (ret < 0) {
        LOG << "sockets::bindOrDie failed: " << ret << std::endl;
    }

}

void sockets::listenOrDie(int sockfd) {
    int ret = ::listen(sockfd, SOMAXCONN); // #define SOMAXCONN   128
    if (ret < 0) {
        LOG << "sockets::listenOrDie failed: " << ret << std::endl;
    }
}

int sockets::accept(int sockfd, struct sockaddr_in6* addr) {
    socklen_t addrlen = static_cast<socklen_t>(sizeof(*addr));
#if VALGRIND || defined (NO_ACCEPT4)
    int connfd = ::accept(sockfd, sockaddr_cast(addr), &addrlen);
    setNonBlockAndCloseOnExec(connfd);
#else
    int connfd = ::accept4(sockfd, sockaddr_cast(addr), &addrlen, SOCK_NONBLOCK | SOCK_CLOEXEC);
#endif
    if (connfd < 0) {
        //若出现下面某些错误，系统可能会改变errno number，所以先把它保存起来。
        int savedErrno = errno;
        LOG << "accept" << std::endl;

        switch (savedErrno) {
            case EAGAIN:
            case ECONNABORTED:
            case EINTR:
            case EPROTO: // ???
            case EPERM:
            case EMFILE: // per-process lmit of open file desctiptor ???
                // expected errors
                errno = savedErrno;  //上述错误不致命，只保存起来就可以了 
                break;
            case EBADF:
            case EFAULT:
            case EINVAL:
            case ENFILE:
            case ENOBUFS:
            case ENOMEM:
            case ENOTSOCK:
            case EOPNOTSUPP:
                LOG << "unexpected error of ::accept " << savedErrno << std::endl; //致命错误直接FATAL  
                break;
            default:
                LOG << "unkonwn error of ::accept " << savedErrno << std::endl;
                break;
        }

    }

    return connfd;
}

int sockets::connect(int sockfd, const struct sockaddr* addr) {
    return ::connect(sockfd, addr, static_cast<socklen_t>(sizeof(struct sockaddr_in6)));
}

ssize_t sockets::read(int sockfd, void* buf, size_t count) {
    return ::read(sockfd, buf, count);
}

ssize_t sockets::readv(int sockfd, const struct iovec* iov, int iovcnt) {
    return ::readv(sockfd, iov, iovcnt);
}

ssize_t sockets::write(int sockfd, void* buf, size_t count) {
    return ::write(sockfd, buf, count);
}

// 关闭套接字 
void sockets::close(int sockfd) {
    if (::close(sockfd) < 0) {
        LOG << "sockets::close failed." << std::endl;
    }
}

// 关闭套接字的写端 
void sockets::shutdownWrite(int sockfd) {
    if (::shutdown(sockfd, SHUT_WR) < 0) {
        LOG << "sockets::shutdownWrite failed." << std::endl;
    }

}

//用sockaddr_in对象构造ip与端口的字符串 ,如127.0.0.1:8080
void sockets::toIpPort(char* buf, size_t size, const struct sockaddr* addr) {
    toIp(buf, size, addr);
    size_t end = ::strlen(buf);
    const struct sockaddr_in* addr4 = sockaddr_in_cast(addr);
    uint16_t port = sockets::networkToHost16(addr4->sin_port); //网络字节序转换为本地字节序
    assert(size > end);
    snprintf(buf+end, size-end, ":%u", port);

}

// 从sockaddr_in中获取ip字符串 
void sockets::toIp(char* buf, size_t size, const struct sockaddr* addr) {
    if (addr->sa_family == AF_INET) {
        assert(size >= INET_ADDRSTRLEN); //#define INET_ADDRSTRLEN 16 ,如201.199.244.101 15个字节+NULL=16
        const struct sockaddr_in* addr4 =  sockaddr_in_cast(addr);
        ::inet_ntop(AF_INET, &addr4->sin_addr, buf, static_cast<socklen_t>(size));

    } else if (addr->sa_family == AF_INET6) {
        assert(size >= INET6_ADDRSTRLEN); //#define INET6_ADDRSTRLEN 46
        const struct sockaddr_in6* addr6 = sockaddr_in6_cast(addr);
        ::inet_ntop(AF_INET6, &addr6->sin6_addr, buf, static_cast<socklen_t>(size));
    }
 
}

// 从ip和端口构造一个sockaddr_in对象  
void sockets::fromIpPort(const char* ip, uint16_t port, struct sockaddr_in* addr) {
    addr->sin_family = AF_INET;
    addr->sin_port = hostToNetwork16(port);
    if (::inet_pton(AF_INET, ip, &addr->sin_addr) <= 0) {
        LOG << "sockets::fromIpPort failed." << std::endl;
    }

}

// 从ip和端口构造一个sockaddr_i6对象  
void sockets::fromIpPort(const char* ip, uint16_t port, struct sockaddr_in6* addr) {
    addr->sin6_family = AF_INET6;
    addr->sin6_port = hostToNetwork16(port);
    if (::inet_pton(AF_INET6, ip, &addr->sin6_addr) <= 0) {
        LOG << "sockets::fromIpPort failed." << std::endl;
    }

}

// 获取错误码

/*
level指定控制套接字的层次.可以取三种值: 
1)SOL_SOCKET:通用套接字选项. 
2)IPPROTO_IP:IP选项. 
3)IPPROTO_TCP:TCP选项.

optname指定控制的方式,列出部分：
SO_ERROR　　　　　　　　获得套接字错误　　　　　　　　　　　　　int 
SO_KEEPALIVE　　　　　　保持连接　　　　　　　　　　　　　　　　int 
SO_LINGER　　　　　　　 延迟关闭连接　　　　　　　　　　　　　　struct linger 
SO_OOBINLINE　　　　　　带外数据放入正常数据流　　　　　　　　　int 
SO_RCVBUF　　　　　　　 接收缓冲区大小　　　　　　　　　　　　　int 
SO_SNDBUF　　　　　　　 发送缓冲区大小　　　　　　　　　　　　　int 
*/
int sockets::getSocketError(int sockfd) {
    int optval;
    socklen_t optlen = static_cast<socklen_t>(sizeof(optval));

    if (::getsockopt(sockfd, SOL_SOCKET, SO_ERROR, &optval, &optlen) < 0) {
        return errno;
    } else {
        return optval;
    }

}


// 获取本地地址  getsockname
struct sockaddr_in6 sockets::getLocalAddr(int sockfd) {
    struct sockaddr_in6 localaddr;
    bzero(&localaddr, sizeof(localaddr));
    socklen_t addrlen = static_cast<socklen_t>(sizeof(localaddr));
    if (::getsockname(sockfd, sockaddr_cast(&localaddr), &addrlen) < 0) {
        LOG << "sockets::getLocalAddr failed." << std::endl;
    }

    return localaddr;
}

// 获取对方地址
struct sockaddr_in6 sockets::getPeerAddr(int sockfd) {
    struct sockaddr_in6 peeraddr;
    bzero(&peeraddr, sizeof(peeraddr));
    socklen_t addrlen = static_cast<socklen_t>(sizeof(peeraddr));
    if (::getpeername(sockfd, sockaddr_cast(&peeraddr), &addrlen) < 0) {
        LOG << "sockets::getPeerAddr failed." << std::endl;
    }

    return peeraddr;
}

// 是否为自己连接自己（环路地址），本地ip:port对端ip:port一致
bool sockets::isSelfConnect(int sockfd) {
    struct sockaddr_in6 localaddr = getLocalAddr(sockfd);
    struct sockaddr_in6 peeraddr = getPeerAddr(sockfd);
    if (localaddr.sin6_family == AF_INET) { //如果是IP4，那么把sockaddr_in6* 转化为sockaddr_in*，再进行端口和地址比较
        const struct sockaddr_in* laddr4 = reinterpret_cast<struct sockaddr_in*>(&localaddr); 
        const struct sockaddr_in* raddr4 = reinterpret_cast<struct sockaddr_in*>(&peeraddr);
        return laddr4->sin_port == raddr4->sin_port
            && laddr4->sin_addr.s_addr == raddr4->sin_addr.s_addr;

    } else if (localaddr.sin6_family == AF_INET6) {//如果是IP6，那么直接通过sockaddr_in6的port和sin6_addr比较
        return localaddr.sin6_port == peeraddr.sin6_port
            && memcmp(&localaddr.sin6_addr, &peeraddr.sin6_addr, sizeof(localaddr.sin6_addr)) == 0;
    } else {
        return false;
    }

}

