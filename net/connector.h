#ifndef KAYCC_NET_CONNECTOR_H 
#define KAYCC_NET_CONNECTOR_H 

//Connector可以说是连接器，负责客户端向服务器发起连接。实际上说白了就是封装了socket的connect操作

#include "inetaddress.h"

#include <boost/enable_shared_from_this.hpp>
#include <boost/function.hpp>
#include <boost/noncopyable.hpp>
#include <boost/scoped_ptr.hpp>

namespace kaycc {
namespace net {

    class Channel;
    class EventLoop;

    class Connector : boost::noncopyable,
                      public boost::enable_shared_from_this<Connector> {
    public:
        typedef boost::function<void (int sockfd)> NewConnectionCallback;

        Connector(EventLoop* loop, const InetAddress& serverAddr);
        ~Connector();

        // 设置连接完成的回调函数 
        void setNewConnectionCallback(const NewConnectionCallback& cb) {
            newConnectionCallback_ = cb;
        }

         // 开始启动连接
        void start(); // can be called in any thread

        // 重新连接
        void restart(); // must be called in loop thread 

        // 停止连接 
        void stop(); //can be called in any thread

        const InetAddress& serverAddress() const {
            return serverAddr_;
        }

    private:
        enum States {kDisconnected, kConnecting, kConnected};

        //// 最大超时重连间隔 
        static const int kMaxRetryDelayMs = 30*1000;

        // 默认的超时重连间隔
        static const int kInitRetryDelayMs = 500;

        // 设置状态
        void setState(States s) {
            state_ = s;
        }

        // 启动连接
        void startInLoop();

        // 停止连接
        void stopInLoop();

        // 连接 
        void connect();

        // 正在连接
        void connecting(int sockfd);

        // 处理写事件
        void handleWrite();

        // 处理错误
        void handleError();

        // 重试
        void retry(int sockfd);

        // 移除并重置事件通道
        int removeAndResetChannel();

        //重置事件通道
        void resetChannel();

        // 所属的Reactor
        EventLoop* loop_;

        // 服务器地址
        InetAddress serverAddr_;

        // 是否已经连接到服务器
        bool connect_; // atomic

        // 状态
        States state_; // FIXME: use atomic variable

        boost::scoped_ptr<Channel> channel_;

        // 连接完成之后的回调函数
        NewConnectionCallback newConnectionCallback_;

        //多少毫秒之后重试
        int retryDelayMs_;
    };

} //end net
}

#endif