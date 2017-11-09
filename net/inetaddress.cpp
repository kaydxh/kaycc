#include "inetaddress.h"

#include "../base/log.h"
#include "endian.h"
#include "socketsops.h"

#include <netdb.h>
#include <string.h> //bzero
#include <netinet/in.h>
#include <assert.h>

#include <boost/static_assert.hpp>


//#define INADDR_ANY      ((in_addr_t) 0x00000000)   //typedef uint32_t in_addr_t;
static in_addr_t kInaddrAny = INADDR_ANY; 

//#define IN_LOOPBACKNET      127
static in_addr_t kInaddrLoopback = INADDR_LOOPBACK;

using namespace kaycc;
using namespace kaycc::net;

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

//     struct in6_addr {  
//          uint8_t s6_addr[16];            // IPv6 address 
//     };



//#define offsetof(TYPE, MEMBER) ((size_t) &((TYPE *)0)->MEMBER)
//(TYPE *)0 让编译器相信0是TYPE类型的有效起始地址
//&((TYPE *)0)->MEMBER  没有访问MEMBER的内容，只是获取其地址
BOOST_STATIC_ASSERT(sizeof(InetAddress) == sizeof(struct sockaddr_in6));
BOOST_STATIC_ASSERT(offsetof(sockaddr_in, sin_family) == 0);
BOOST_STATIC_ASSERT(offsetof(sockaddr_in6, sin6_family) == 0);
BOOST_STATIC_ASSERT(offsetof(sockaddr_in, sin_port) == 2);
BOOST_STATIC_ASSERT(offsetof(sockaddr_in6, sin6_port) == 2);

//去除警告
// warning: invalid access to non-static data member ‘kaycc::net::InetAddress::<anonymous union>::addr6_’  of NULL object
/*
-Wno-invalid-offsetof (C++ and Objective-C++ only)
Suppress warnings from applying the `offsetof' macro to a non-POD type. According to the 1998 ISO C++ standard, applying `offsetof' to a non-POD type is undefined. In existing C++ implementations, however, `offsetof' typically gives meaningful results even when applied to certain kinds of non-POD types. (Such as a simple `struct' that fails to be a POD type only by virtue of having a constructor.) This flag is for users who are aware that they are writing nonportable code and who have deliberately chosen to ignore the warning about it.
The restrictions on `offsetof' may be relaxed in a future version of the C++ standard. 
*/
#pragma GCC diagnostic ignored "-Winvalid-offsetof"

InetAddress::InetAddress(uint16_t port, bool loopbackOnly, bool ipv6) {
    BOOST_STATIC_ASSERT(offsetof(InetAddress, addr6_) == 0);
    BOOST_STATIC_ASSERT(offsetof(InetAddress, addr_) == 0);

    if (ipv6) {
        bzero(&addr6_, sizeof(addr6_));
        addr6_.sin6_family = AF_INET6;

        //const struct in6_addr in6addr_loopback
        in6_addr ip = loopbackOnly ? in6addr_loopback : in6addr_any;
        addr6_.sin6_addr = ip;
        addr6_.sin6_port = sockets::hostToNetwork16(port);

    } else {
        bzero(&addr_, sizeof(addr_));
        addr_.sin_family = AF_INET;
        in_addr_t ip = loopbackOnly ? kInaddrLoopback : kInaddrAny;
        addr_.sin_addr.s_addr = sockets::hostToNetwork32(ip);
        addr_.sin_port = sockets::hostToNetwork16(port);
    }

}

InetAddress::InetAddress(const std::string& ip, uint16_t port, bool ipv6) {
    if (ipv6) {
        bzero(&addr6_, sizeof(addr6_));
        sockets::fromIpPort(ip.c_str(), port, &addr6_);
    } else {
        bzero(&addr_, sizeof(addr_));
        sockets::fromIpPort(ip.c_str(), port, &addr_);

    }

}

// 返回ip
std::string InetAddress::toIp() const {
    char buf[64] = "";
    sockets::toIp(buf, sizeof(buf), getSockAddr());
    return buf;
}

// 返回ip和端口的字符串 
std::string InetAddress::toIpPort() const {
    char buf[64] = "";
    sockets::toIpPort(buf, sizeof(buf), getSockAddr());
    return buf;
}

uint16_t InetAddress::toPort() const {
    return sockets::networkToHost16(portNetEndian());
}

// 返回网络字节顺序的ip 
uint32_t InetAddress::ipNetEndian() const {
    assert(family() == AF_INET);
    return addr_.sin_addr.s_addr;
}

// 线程安全的t_resolveBuffer，每个线程都有自己的一份，互不干涉
static __thread char t_resolveBuffer[64 * 1024];

/*
Unix/Linux下的gethostbyname函数常用来向DNS查询一个域名的IP地址。 由于DNS的递归查询，常常会发生gethostbyname函数在查询一个域名时严重超时。而该函数又不能像connect和read等函数那样通过setsockopt或者select函数那样设置超时时间，因此常常成为程序的瓶颈。

gethostbyname_r支持多线程，单机测试可以达到100次/s。

int gethostbyaddr_r(const void *addr, socklen_t len, int type,
        struct hostent *ret, char *buf, size_t buflen,
        struct hostent **result, int *h_errnop);

参数说明：name——是网页的host名称，如百度的host名是www.baidu.com
                  ret——成功的情况下存储结果用。
                  buf——这是一个临时的缓冲区，用来存储过程中的各种信息，一般8192大小就够了，可以申请一个数组char buf[8192]
                  buflen——是buf缓冲区的大小
                  result——如果成功，则这个hostent指针指向ret，也就是正确的结果；如果失败，则result为NULL
                  h_errnop——存储错误码
该函数成功返回0，失败返回一个非0的数
*/

//struct hostent {
//     char  *h_name;            /* official name of host */
//     char **h_aliases;         /* alias list */
//     int    h_addrtype;        /* host address type 到底是ipv4(AF_INET)，还是ipv6(AF_INET6)*/
//     int    h_length;          /* length of address */
//     char **h_addr_list;       /* list of addresses */
// };
//  #define h_addr h_addr_list[0] /* for backward compatibility */

// 把主机名转换成ip地址，线程安全 
bool InetAddress::resolve(const std::string& hostname, InetAddress* out) {
    assert(out != NULL);
    struct hostent hent;
    struct hostent* he = NULL;
    int herrno = 0;
    bzero(&hent, sizeof(hent));

    // 获取主机信息 
    int ret = gethostbyname_r(hostname.c_str(), &hent, t_resolveBuffer, sizeof(t_resolveBuffer), &he, &herrno);
    if (ret == 0 && he != NULL) {
        assert(he->h_addrtype == AF_INET && he->h_length == sizeof(uint32_t));
        out->addr_.sin_addr = *reinterpret_cast<struct in_addr*>(he->h_addr);

        return true;
    } else {
        if (ret) {
            LOG << "InetAddress::resolve failed: " << ret << std::endl;
        }

        return false;
    }
}