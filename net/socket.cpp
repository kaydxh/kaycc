#include "socket.h"

#include "../base/log.h"
#include "inetaddress.h"
#include "socketsops.h"

#include <netinet/in.h>
#include <netinet/tcp.h>
#include <strings.h> // bzero
#include <stdio.h>

using namespace kaycc;
using namespace kaycc::net; 

/*
struct tcp_info
{
  u_int8_t  tcpi_state;                                  // TCP状态
  u_int8_t  tcpi_ca_state;                               //TCP拥塞状态
  u_int8_t  tcpi_retransmits;                            // 超时重传的次数
  u_int8_t  tcpi_probes;                                 //持续定时器或保活定时器发送且未确认的段数
  u_int8_t  tcpi_backoff;                                //退避指数
  u_int8_t  tcpi_options;                                //时间戳选项、SACK选项、窗口扩大选项、ECN选项是否启用
  u_int8_t  tcpi_snd_wscale : 4, tcpi_rcv_wscale : 4;    //发送、接收的窗口扩大因子

  u_int32_t tcpi_rto;                                    //超时时间，单位为微秒
  u_int32_t tcpi_ato;                                    //延时确认的估值，单位为微秒
  u_int32_t tcpi_snd_mss;                                //本端的MSS
  u_int32_t tcpi_rcv_mss;                                //对端的MSS

  u_int32_t tcpi_unacked;                                //未确认的数据段数，或者current listen backlog
  u_int32_t tcpi_sacked;                                 //SACKed的数据段数，或者listen backlog set in listen()
  u_int32_t tcpi_lost;                                   //丢失且未恢复的数据段数
  u_int32_t tcpi_retrans;                                //重传且未确认的数据段数
  u_int32_t tcpi_fackets;                                //FACKed的数据段数

  // Times. 单位为毫秒 
  u_int32_t tcpi_last_data_sent;                         //最近一次发送数据包在多久之前
  u_int32_t tcpi_last_ack_sent;                          //不能用 Not remembered, sorry. 
  u_int32_t tcpi_last_data_recv;                         //最近一次接收数据包在多久之前
  u_int32_t tcpi_last_ack_recv;                          //最近一次接收ACK包在多久之前

  // Metrics. 
  u_int32_t tcpi_pmtu;                                   //最后一次更新的路径MTU
  u_int32_t tcpi_rcv_ssthresh;                           //current window clamp，rcv_wnd的阈值
  u_int32_t tcpi_rtt;                                    //平滑的RTT，单位为微秒
  u_int32_t tcpi_rttvar;                                 //四分之一mdev，单位为微秒v 
  u_int32_t tcpi_snd_ssthresh;                           //慢启动阈值
  u_int32_t tcpi_snd_cwnd;                               //拥塞窗口
  u_int32_t tcpi_advmss;                                 //本端能接受的MSS上限，在建立连接时用来通告对端
  u_int32_t tcpi_reordering;                             //没有丢包时，可以重新排序的数据段数

  u_int32_t tcpi_rcv_rtt;                                //作为接收端，测出的RTT值，单位为微秒
  u_int32_t tcpi_rcv_space;                              //当前接收缓存的大小

  u_int32_t tcpi_total_retrans;                          //本连接的总重传个数
};
*/

Socket::~Socket() {
    sockets::close(sockfd_);
}

// 获取tcp信息
bool Socket::getTcpInfo(struct tcp_info* tcpi) const {
    socklen_t len = sizeof(*tcpi);
    bzero(tcpi, len);
    return ::getsockopt(sockfd_, SOL_TCP, TCP_INFO, tcpi, &len) == 0;
}

// 获取tcp的信息（字符串形式）
bool Socket::getTcpInfoString(char* buf, int len) const {
    struct tcp_info tcpi;
    bool ok = getTcpInfo(&tcpi);
    if (ok) {
        snprintf(buf, len, "unrecovered=%u "
            "rto=%u ato=%u snd_mss=%u rcv_mss=%u "
            "lost=%u retrans=%u rtt=%u rttvar=%u "
            "sshthresh=%u cwnd=%u total_retrans=%u",
            tcpi.tcpi_retransmits,
            tcpi.tcpi_rto,              //超时时间，单位为微秒
            tcpi.tcpi_ato,              //延时确认的估值，单位为微秒
            tcpi.tcpi_snd_mss,          //本端的MSS
            tcpi.tcpi_rcv_mss,          //对端的MSS
            tcpi.tcpi_lost,             //丢失且未恢复的数据段数
            tcpi.tcpi_retrans,          //重传且未确认的数据段数
            tcpi.tcpi_rtt,              //平滑的RTT，单位为微秒
            tcpi.tcpi_rttvar,           //四分之一mdev，单位为微秒v
            tcpi.tcpi_snd_ssthresh,    //慢启动阈值
            tcpi.tcpi_snd_cwnd,         //拥塞窗口
            tcpi.tcpi_total_retrans);   //本连接的总重传个数
    }

    return ok;
}

// 绑定地址
void Socket::bindAddress(const InetAddress& addr) {
    sockets::bindOrDie(sockfd_, addr.getSockAddr());
}

//监听
void Socket::listen() {
    sockets::listenOrDie(sockfd_);
}

// 接受一个连接，对方的地址存放在peeraddr中，返回一个文件描述符（出错则返回-1）
int Socket::accept(InetAddress* peeraddr) {
    struct sockaddr_in6 addr;
    bzero(&addr, sizeof(addr));
    int connfd = sockets::accept(sockfd_, &addr);
    if (connfd >= 0) {
        peeraddr->setSockAddrInet6(addr);
    }

    return connfd;
}

//关闭写端 
void Socket::shutdownWrite() {
    sockets::shutdownWrite(sockfd_);
}

///  
/// Enable/disable TCP_NODELAY (disable/enable Nagle's algorithm).  
///  
// 关闭或开启Nagle算法，该算法会先缓存小的数据片组成大的数据片再发送，可以有效减少网络上数据包的数量
void Socket::setTcpNoDelay(bool on) {
    int optval = on ? 1 : 0;
    ::setsockopt(sockfd_, IPPROTO_TCP, TCP_NODELAY,
                 &optval, static_cast<socklen_t>(sizeof(optval)));
}

///
/// Enable/disable SO_REUSEADDR
///
// 关闭或开启地址复用
void Socket::setReuseAddr(bool on) {
    int optval = on ? 1 : 0;
    ::setsockopt(sockfd_, SOL_SOCKET, SO_REUSEADDR,
                 &optval, static_cast<socklen_t>(sizeof(optval)));

}

// 关闭或开启端口复用
void Socket::setReusePort(bool on) {
#ifdef SO_REUSEPORT
    int optval = on ? 1 : 0;
    int ret = ::setsockopt(sockfd_, SOL_SOCKET, SO_REUSEPORT,
                           &optval, static_cast<socklen_t>(sizeof(optval)));

    if (ret < 0 && on) {
        LOG << "SO_REUSEPORT failed: " << ret << std::endl;
    }

#else 

    if (on) {
        LOG << "SO_REUSEPORT is not supported." << std::endl;
    }

#endif

}

// 关闭或开启保活机制 
void Socket::setKeepAlive(bool on) {
    int optval = on ? 1 : 0;
    ::setsockopt(sockfd_, SOL_SOCKET, SO_KEEPALIVE,
                 &optval, static_cast<socklen_t>(sizeof(optval)));
}