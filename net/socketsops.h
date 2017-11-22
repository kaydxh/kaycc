#ifndef KAYCC_NET_SOCKETSOPS_H
#define KAYCC_NET_SOCKETSOPS_H

#include <arpa/inet.h>

namespace kaycc {
namespace net {
namespace sockets{

    /// Creates a non-blocking socket file descriptor,
    /// abort if any error.

    //typedef unsigned short int sa_family_t;  address family, AF_xxx   
    int createNonblockingOrDie(sa_family_t family); 

    /*
    sizeof(sockaddr) = 16
    struct sockaddr {
        sa_family_t sa_family; //address family, AF_xxx 
        char        sa_data[14]; // 14 bytes of protocol address 
    }
    */

    int connect(int sockfd, const struct sockaddr* addr); 
    void bindOrDie(int sockfd, const struct sockaddr* addr);
    void listenOrDie(int sockfd);
    int accept(int sockfd, struct sockaddr_in6* addr);
    ssize_t read(int sockfd, void* buf, size_t count);

    //readv和write函数用于在一次函数调用中读，写多个非连续缓冲区。
    /*
    struct iovec{
        void *iov_base; //starting address of buffer
        size_t iov_len; //size of buffer
    }
    */

    ssize_t readv(int sockfd, const struct iovec* iov, int iovcnt);
    ssize_t write(int sockfd, const void* buf, size_t count);
    void close(int sockfd);

    // 关闭套接字的写端 
    void shutdownWrite(int sockfd);

    //用sockaddr_in对象构造ip与端口的字符串 ,如127.0.0.1:8080
    void toIpPort(char* buf, size_t size, const struct sockaddr* addr);

    // 从sockaddr_in中获取ip字符串 
    void toIp(char* buf, size_t size, const struct sockaddr* addr);

    /*
    sizeof(sockaddr_in) = 16
    struct sockaddr_in {  
      short int sin_family;            // Address family AF_INET 
      unsigned short int sin_port;    // Port number   
      struct in_addr sin_addr;        // Internet address  
      unsigned char sin_zero[8];     // Same size as struct sockaddr   
    };

    struct in_addr {  
      unsigned int s_addr;           // Internet address   
    }; 
    */  
    // 从ip和端口构造一个sockaddr_in对象  
    void fromIpPort(const char* ip, uint16_t port, struct sockaddr_in* addr);


    /*
    sizeof(sockaddr_in6) = 28
    struct sockaddr_in6 {  
      sa_family_t sin6_family;         // AF_INET6   
      in_port_t sin6_port;               // transport layer port   
      uint32_t sin6_flowinfo;           // IPv6 traffic class & flow info   
      struct in6_addr sin6_addr;    // IPv6 address   
      uint32_t sin6_scope_id;        // set of interfaces for a scope   
    };  
    struct in6_addr {  
        uint8_t s6_addr[16];            // IPv6 address 
    };
    */
    // 从ip和端口构造一个sockaddr_i6对象 
    void fromIpPort(const char* ip, uint16_t port, struct sockaddr_in6* addr);

    // 获取错误码 getsockopt
    int getSocketError(int sockfd);

    const struct sockaddr* sockaddr_cast(const struct sockaddr_in* addr);
    const struct sockaddr* sockaddr_cast(const struct sockaddr_in6* addr);
    struct sockaddr*  sockaddr_cast(struct sockaddr_in6* addr);

    const struct sockaddr_in* sockaddr_in_cast(const struct sockaddr* addr);
    const struct sockaddr_in6* sockaddr_in6_cast(const struct sockaddr* addr);

    // 获取本地地址  getsockname
    struct sockaddr_in6 getLocalAddr(int sockfd);

    // 获取对方地址
    struct sockaddr_in6 getPeerAddr(int sockfd);

    // 是否为自己连接自己（环路地址）
    bool isSelfConnect(int sockfd);



}
}
}

#endif