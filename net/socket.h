#ifndef KAYCC_NET_SOCKET_H 
#define KAYCC_NET_SOCKET_H 

#include <boost/noncopyable.hpp>


// struct tcp_info is in <netinet/tcp.h>
struct tcp_info;
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

namespace kaycc {
namespace net {

class InetAddress;

class Socket : boost::noncopyable {
public:

    // 用文件描述符构造 
    explicit Socket(int sockfd)
        : sockfd_(sockfd) {

        }

    ~Socket();

    // 获取文件描述符
    int fd() const {
        return sockfd_;
    }

    // 获取tcp信息
    bool getTcpInfo(struct tcp_info*) const;

    // 获取tcp的信息（字符串形式）
    bool getTcpInfoString(char* buf, int len) const;

     // 绑定地址
    void bindAddress(const InetAddress& localaddr);

    //监听
    void listen();

    // 接受一个连接，对方的地址存放在peeraddr中，返回一个文件描述符（出错则返回-1）
    int accept(InetAddress* peeraddr);

    //关闭写端 
    void shutdownWrite();

    ///  
    /// Enable/disable TCP_NODELAY (disable/enable Nagle's algorithm).  
    ///  
    // 关闭或开启Nagle算法，该算法会先缓存小的数据片组成大的数据片再发送，可以有效减少网络上数据包的数量
    void setTcpNoDelay(bool on);

    ///
    /// Enable/disable SO_REUSEADDR
    ///
    // 关闭或开启地址复用
    void setReuseAddr(bool on);

    // 关闭或开启端口复用
    void setReusePort(bool on);

    // 关闭或开启保活机制 
    void setKeepAlive(bool on);

private:
    const int sockfd_;

};

} //end net
}

#endif