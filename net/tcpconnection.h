#ifndef KAYCC_NET_TCPCONNECTION_H 
#define KAYCC_NET_TCPCONNECTION_H 

#include "callbacks.h"
#include "buffer.h"
#include "inetaddress.h"

#include <boost/any.hpp>
#include <boost/enable_shared_from_this.hpp>
#include <boost/noncopyable.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/scoped_ptr.hpp>

/*
TcpConnection是使用shared_ptr来管理的类，因为它的生命周期模糊。TcpConnection表示已经建立或正在建立的连接，状态只有kConnecting、kConnected、kDisconnected、
kDisconnecting，它初始化时，构造函数的sockfd表示正在建立连接kConnecting。

建立连接后，用户只需处理收发数据，发送数据它会自动处理，收取数据后，会调用用户设置的MessageCallback函数来处理收到的数据。 
TcpConnection中封装了inputBuffer和outputBuffer，用来表示应用层的缓冲。在发送数据时，如果不能一次将呕吐Buffer中的数据发送完毕，它还会enable channel中的wirte事件，当sockfd可写时，会再次发送。

高水位回调和低水位回调： 
在发送数据时，如果发送过快会造成数据在本地积累。muduo解决这个问题的办法是用了高水位回调和低水位回调，分别用函数HighWaterMarkCallback和WriteCompleteCallback代表。
原理为：设置一个发送缓冲区的上限值，如果大于这个上限值，停止接收数据；WriteCompleteCallback函数为发送缓冲区为空时调用，在这个函数重启开启接收数据。

调用send时，可能不是TcpConnection所属的IO线程，这是通过loop_->runInLoop可以轮转到其所属的IO线程。因为TcpConnection中保存了其所属EventLoop的指针，可以通过EventLoop::runInLoop将所调用的函数添加到所属EventLoop的任务队列中。

断开连接： 
TcpConnection的断开是采用被动方式，即对方先关闭连接，本地read(2)返回0后，调用顺序如下： 
handleClose()->TcpServer::removeConnection->TcpConnection::connectDestroyed()。
*/

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

/* 
 * TCP客户端与TCP服务器之间的一个连接 
 * 客户端和服务器都使用这个类 
 * 对于服务器来说，每当一个客户到来的时候就建立一个TcpConnection对象 
 * 对于客户端来说，一个TcpConnection对象就表示客户端与服务器的一个连接 
 * 用户不应该直接使用这个类 
 */ 

namespace kaycc {
namespace net {
    class Channel;
    class EventLoop;
    class Socket;

    ///
    /// TCP connection, for both client and server usage.
    ///
    /// This is an interface class, so don't expose too much details.

    class TcpConnection : boost::noncopyable,
                          public boost::enable_shared_from_this<TcpConnection> {
    public:
        TcpConnection(EventLoop* loop,
                      const std::string& name,
                      int sockfd,
                      const InetAddress& localAddr,
                      const InetAddress& peerAddr);

        ~TcpConnection();

        EventLoop* getLoop() const {
            return loop_;
        }

        const std::string& name() const {
            return name_;
        }

        const InetAddress& localAddress() const {
            return localAddr_;
        }

        const InetAddress& peerAddress() const {
            return peerAddr_;
        }

        bool connected() const {
            return state_ == kConnected;
        }

        bool disconnected() const {
            return state_ == kDisconnected;
        }

        // 获取tcp信息 
        bool getTcpInfo(struct tcp_info*) const;

        // 获取tcp信息的字符串
        std::string getTcpInfoString() const;

        void send(const void* message, int len);

        void send(const std::string& message);

        void send(Buffer* message);

        void shutdown(); // NOT thread safe, no simultaneous calling

        // 强制关闭 
        void forceClose();

        // 带延时的强制关闭 
        void forceCloseWithDelay(double seconds);

        // 关闭或开启Nagle算法  
        void setTcpNoDelay(bool on);

        void startRead();

        void stopRead();

        // NOT thread safe, may race with start/stopReadInLoop
        bool isReading() const {
            return reading_;
        }

        void setContext(const boost::any& context) {
            context_ = context;
        }

        const boost::any& getContext() const {
            return context_;
        }

        // 获取可修改的上下文
        boost::any* getMutableContext() {
            return &context_;
        }

        void setConnectionCallback(const ConnectionCallback& cb) {
            connectionCallback_ = cb;
        }

        void setMessageCallback(const MessageCallback& cb) {
            messageCallback_ = cb;
        }

        void setWriteCompleteCallback(const WriteCompleteCallback& cb) {
            writeCompleteCallback_ = cb;
        }

        void setHighWaterMarkCallback(const HighWaterMarkCallback& cb, size_t highWaterMark) {
            highWaterMarkCallback_ = cb;
            highWaterMark_ = highWaterMark;
        }

        /// Advanced interface
        Buffer* inputBuffer() {
            return &inputBuffer_;
        }

        Buffer* outputBuffer() {
            return &outputBuffer_;
        }

        void setCloseCallback(const CloseCallback& cb) {
            closeCallback_ = cb;
        }

        // called when TcpServer accepts a new connection   should be called only once
        void connectEstablished();

        // called when TcpServer has removed me from its map  should be called only once
        void connectDestroyed();

    private:
        enum StateE {kDisconnected, kConnecting, kConnected, kDisconnecting};

        // 处理读
        void handleRead(Timestamp receiveTime);

        // 处理写
        void handleWrite();

        // 处理关闭
        void handleClose();

        // 处理错误 
        void handleError();

        void sendInLoop(const std::string& message);

        void sendInLoop(const void* message, size_t len);

        // 在循环中关闭连接
        void shutdownInLoop();

        // 强制退出循环
        void forceCloseInLoop();

        // 设置状态
        void setState(StateE s) {
            state_ = s;
        }

        // 将状态转换为字符串 
        const char* stateToString() const;

        void startReadInLoop();
        void stopReadInLoop();

        EventLoop* loop_;
        const std::string name_;

        StateE state_;
        bool reading_;

        boost::scoped_ptr<Socket> socket_;

        // 事件通道、tcp连接、还有套接字是一一对应的
        boost::scoped_ptr<Channel> channel_;
        const InetAddress localAddr_;
        const InetAddress peerAddr_;
        ConnectionCallback connectionCallback_;
        MessageCallback messageCallback_;
        WriteCompleteCallback  writeCompleteCallback_;

        // 达到高水位标记之后的回调函数
        HighWaterMarkCallback  highWaterMarkCallback_;

        // 关闭连接的回调函数
        CloseCallback closeCallback_;

        // 高水位标记 
        size_t highWaterMark_;

         // 输入缓冲区
        Buffer inputBuffer_;

        // 输出缓冲区
        Buffer outputBuffer_; // FIXME: use list<Buffer> as output buffer. 

        boost::any context_;

        // FIXME: creationTime_, lastReceiveTime_
        // bytesReceived_, bytesSent_ 

    };



} //end net
}

#endif