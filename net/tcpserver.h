#ifndef KAYCC_NET_TCPSERVER_H 
#define KAYCC_NET_TCPSERVER_H 

/*
TcpServer的拥有一个Acceptor和EventLoopThreadPool。Acceptor是对accept连接一系列事件的封装，EventThreadPoll是I/O线程的池式结构。
这就是muduo库的思想，multi Reactor模型，主函数中一个main Reactor，I/O线程池中存放的都是有用Reactor的线程。来一个给分配一个。
所以，现在可以看出TcpServer具备了接受客户端连接，分配I/O线程给客户端的功能
*/

#include "../base/atomic.h"
#include "tcpconnection.h"

#include <map>
#include <boost/noncopyable.hpp>
#include <boost/scoped_ptr.hpp>
#include <boost/shared_ptr.hpp>


namespace kaycc {
namespace net {

    class Acceptor;
    class EventLoop;
    class EventLoopThreadPool;

    ///
    /// TCP server, supports single-threaded and thread-pool models.
    ///
    /// This is an interface class, so don't expose too much details.

    class TcpServer : boost::noncopyable {
    public:
        typedef boost::function<void (EventLoop*)> ThreadInitCallback;
        enum Option {
            // 不复用端口 
            kNoReusePort,

            // 复用端口 
            kReusePort,
        };

        TcpServer(EventLoop* loop,
                  const InetAddress& listenAddr,
                  const std::string& name,
                  Option option = kNoReusePort);

        ~TcpServer();  // force out-line dtor, for scoped_ptr members.

        const std::string& ipPort() const {
            return ipPort_;
        }

        const std::string& name() const {
            return name_;
        }

        // 获取Acceptor的EventLoop
        EventLoop* getLoop() const {
            return loop_;
        }

        // 设置线程的数量  
        /* numThreads的值： 
        * =0 表示所有的io操作都在Acceptor的EventLoop中进行 
        * =1 表示新的链接在Acceptor的EventLoop中进行，而其他的io操作在另一个线程中进行 
        * =N 表示新的链接Acceptor的EventLoop中进行，获取之后按照round-robin的方式分配给其中的一个线程处理其他的IO 
        */
        void setThreadNum(int numThreads);

        // 设置线程初始化回调函数 
        void setThreadInitCallback(const ThreadInitCallback& cb) {
            threadInitCallback_ = cb;
        }

        /// valid after calling start()
        boost::shared_ptr<EventLoopThreadPool> threadPool() {
            return threadPool_;
        }

        void start();

        /// Set connection callback.
        /// Not thread safe.
        void setConnectionCallback(const ConnectionCallback& cb) {
            connectionCallback_ = cb;
        }

        /// Set message callback.
        /// Not thread safe.
        void setMessageCallback(const MessageCallback& cb) {
            messageCallback_ = cb;
        }

        /// Set write complete callback.
        /// Not thread safe.
        void setWriteCompleteCallback(const WriteCompleteCallback& cb) {
            writeCompleteCallback_ = cb;
        }

    private:
        /// Not thread safe, but in loop
        /// 新连接到来回调函数
        void newConnection(int sockfd, const InetAddress& peerAddr);

        /// Thread safe.
        /// 删除连接
        void removeConnection(const TcpConnectionPtr& conn);

        /// Not thread safe, but in loop
        void removeConnectionInLoop(const TcpConnectionPtr& conn);

        typedef std::map<std::string, TcpConnectionPtr> ConnectionMap;

        EventLoop* loop_;  // the acceptor loop

        const std::string ipPort_;
        const std::string name_;
        boost::scoped_ptr<Acceptor> acceptor_;    // avoid revealing Acceptor
        boost::shared_ptr<EventLoopThreadPool> threadPool_;

        ConnectionCallback connectionCallback_;
        MessageCallback messageCallback_;
        WriteCompleteCallback writeCompleteCallback_;

        // 线程初始化回调函数 
        ThreadInitCallback threadInitCallback_;

        // 服务器是否已经启动
        AtomicInt32 started_;

        // 下一个连接的id 
        int nextConnId_;

        // 存放所有的连接 
        ConnectionMap connections_;

    };

} //end net
}

#endif