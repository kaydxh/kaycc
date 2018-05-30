#ifndef KAYCC_NET_TCPCLIENT_H 
#define KAYCC_NET_TCPCLIENT_H

#include <boost/noncopyable.hpp>

#include "../base/mutex.h"
#include "tcpconnection.h" 

namespace kaycc {
namespace net {

    class Connector;
    typedef boost::shared_ptr<Connector> ConnectorPtr;

    class TcpClient : boost::noncopyable {
    public:
        TcpClient(EventLoop* loop,
                  const InetAddress& serverAddr,
                  const std::string& name);

        ~TcpClient(); // force out-line dtor, for scoped_ptr members.

        void connect();
        void disconnect();
        void stop();

        TcpConnectionPtr connection() const {
            MutexLockGuard lock(mutex_);
            return connection_;
        }

        EventLoop* getLoop() const {
            return loop_;
        }

        bool retry() const {
            return retry_;
        }

        void enableRetry() {
            retry_ = true;
        }
 
        const std::string& name() const {
            return name_;
        }

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

    #if __cpluscplus >= 201103L
        void setConnectionCallback(const ConnectionCallback&& cb) {
            connectionCallback_ = std::move(cb);
        }

        void setMessageCallback(const MessageCallback&& cb) {
            messageCallback_ = std::move(cb);
        }

        void setWriteCompleteCallback(const WriteCompleteCallback&& cb) {
            writeCompleteCallback_ = std::move(cb);
        }
    #endif

    private:
        // 连接建立完毕会调用这个函数
        void newConnection(int sockfd);

        // 移除一个链接
        void removeConnection(const TcpConnectionPtr& conn);

        // 所属的Reactor  
        EventLoop* loop_;
        ConnectorPtr connector_; // avoid revealing Connector

        const std::string name_;
        ConnectionCallback connectionCallback_;
        MessageCallback messageCallback_;
        WriteCompleteCallback writeCompleteCallback_;

        bool retry_; // atomic   //是否重连，是指建立的连接成功后又断开是否重连。
        bool connect_; // atomic

        // always in loop thread
        int nextConnId_; //name_+nextConnid_用于标识一个连接

        mutable MutexLock mutex_;
        TcpConnectionPtr connection_;  //Connector连接成功后，得到一个TcpConnection
    };


} //end net
}

#endif