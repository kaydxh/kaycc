#ifndef KAYCC_NET_INETADDRESS_H
#define KAYCC_NET_INETADDRESS_H

#include "../base/copyable.h"

#include <netinet/in.h>
#include <string>

/* 
 * 网络地址类 
 * 只是sockaddr_in的一个包装，实现了各种针对sockaddr_in的操作 
 */ 

//     /* Structure describing an Internet socket address.  */
//     struct sockaddr_in {
//         sa_family_t    sin_family; /* address family: AF_INET */
//         uint16_t       sin_port;   /* port in network byte order */
//         struct in_addr sin_addr;   /* internet address */
//     };

//     /* Internet address. */
//     typedef uint32_t in_addr_t;
//     struct in_addr {
//         in_addr_t       s_addr;     /* address in network byte order */
//     };

//     struct sockaddr_in6 {
//         sa_family_t     sin6_family;   /* address family: AF_INET6 */
//         uint16_t        sin6_port;     /* port in network byte order */
//         uint32_t        sin6_flowinfo; /* IPv6 flow information */
//         struct in6_addr sin6_addr;     /* IPv6 address */
//         uint32_t        sin6_scope_id; /* IPv6 scope-id */
//     };

namespace kaycc {
namespace net {

namespace sockets {
    const struct sockaddr* sockaddr_cast(const struct sockaddr_in6* addr);
} //sockets


class InetAddress : public kaycc::copyable {
public:

    // 使用这个构造函数可以构造回路地址 
    explicit InetAddress(uint16_t port = 0, bool loopbackOnly = false, bool ipv6 = false);

     // 从ip和端口构造一个地址
    InetAddress(const std::string& ip, uint16_t port, bool ipv6 = false);

    explicit InetAddress(const struct sockaddr_in& addr)
        : addr_(addr) {

        }

    explicit InetAddress(const struct sockaddr_in6& addr) 
        : addr6_(addr) {

        }

    sa_family_t family() const {
        return addr_.sin_family;
    }

    // 返回ip
    std::string toIp() const;
    // 返回ip和端口的字符串 
    std::string toIpPort() const;
     // 返回端口 
    uint16_t toPort() const;

    const struct sockaddr* getSockAddr() const {
        return sockets::sockaddr_cast(&addr6_);
    }

    void setSockAddrInet6(const struct sockaddr_in6& addr6) {
        addr6_ = addr6;
    }

    // 返回网络字节顺序的ip 
    uint32_t ipNetEndian() const;

    // 返回网络字节顺序的端口 
    uint16_t portNetEndian() const {
        return addr_.sin_port;
    }

    // 把主机名转换成ip地址，线程安全 
    static bool resolve(const std::string& hostname, InetAddress* out);

private:
    union {
        struct sockaddr_in addr_;
        struct sockaddr_in6 addr6_;
    };

};

} //net
}

#endif
