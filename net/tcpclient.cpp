#include "tcpclient.h"

#include "connector.h"
#include "eventloop.h"
#include "socketsops.h"
#include "../base/log.h"

#include <boost/bind.hpp>

#include <stdio.h> // snprintf

using namespace kaycc;
using namespace kaycc::net;

namespace kaycc {
namespace net {
namespace detail {

    // 移除连接
    void removeConnection(EventLoop* loop, const TcpConnectionPtr& conn) {
        loop->queueInLoop(boost::bind(&TcpConnection::connectDestroyed, conn));
    }

    void removeConnector(const ConnectorPtr& /*connector*/) {
        //connector->
    }

} //end detail
} //end net
}


TcpClient::TcpClient(EventLoop* loop,
                  const InetAddress& serverAddr,
                  const std::string& name)
    : loop_(loop),
      connector_(new Connector(loop, serverAddr)),
      name_(name),
      connectionCallback_(defaultConnectionCallback),
      messageCallback_(defaultMessageCallback),
      retry_(false),
      connect_(true),
      nextConnId_(1) {

    //一旦连接建立连接，回调newConnection
    connector_->setNewConnectionCallback(
        boost::bind(&TcpClient::newConnection, this, _1));

    LOG << "TcpClient::TcpClient[" << name_
        << "] - connector " << get_pointer(connector_) << std::endl;

}

TcpClient::~TcpClient() {
    LOG << "TcpClient::~TcpClient[" << name_
        << "] - connector " << get_pointer(connector_) << std::endl;

    TcpConnectionPtr conn;
    bool unique = false;

    {
        MutexLockGuard lock(mutex_);
        unique = connection_.unique();
        conn = connection_;
    }

    if (conn) {
        assert(loop_ == conn->getLoop());
        // FIXME: not 100% safe, if we are in different thread
        // 移除TcpConnection对象
        CloseCallback cb = boost::bind(&detail::removeConnection, loop_, _1);

        // 调用连接关闭回调函数
        loop_->runInLoop(
            boost::bind(&TcpConnection::setCloseCallback, conn, cb));

        if (unique) {
            // 强制关闭
            conn->forceClose();
        }

    } else {
        // 停止连接器 
        connector_->stop();

        // 移除连接器 
        loop_->runAfter(1, 
            boost::bind(&detail::removeConnector, connector_));
    }
}

void TcpClient::connect() {
    LOG << "TcpClient::connect[" << name_ << "] - connecting to "
        << connector_->serverAddress().toIpPort() << std::endl;

    connect_ = true;
    connector_->start();
}

void TcpClient::disconnect() {
    connect_ = false;

    {
        MutexLockGuard lock(mutex_);
        if (connection_) {
            connection_->shutdown();
        }

    }
}

void TcpClient::stop() {
    connect_ = false;
    connector_->stop();
}
/*
该函数创建一个堆上局部TcpConnection对象，并用TcpClient的智能指针connection_保存起来，这样本函数中conn即便析构掉，connection_依然维护该连接。
然后设置各种回调函数。由于为了避免busy loop，在Connector中一旦连接成功，我们取消关注sockfd的可写事件。并且本函数使用conn->connectEstablished()内部会关注可读事件
*/
// 连接建立完毕会调用这个函数
void TcpClient::newConnection(int sockfd) {
    loop_->assertInLoopThread();
    InetAddress peerAddr(sockets::getPeerAddr(sockfd));

    char buf[32];
    snprintf(buf, sizeof(buf), ":%s#%d", peerAddr.toIpPort().c_str(), nextConnId_);
    ++nextConnId_;

    std::string connName = name_ + buf;

    InetAddress localAddr(sockets::getLocalAddr(sockfd));

    // FIXME poll with zero timeout to double confirm the new connection  
    // FIXME use make_shared if necessary  
    //创建一个TcpConnection对象，智能指针。  
    //根据Connector中的handleWrite()函数，连接建立后会把sockfd从poller中移除，以后不会再关注可写事件了  
    //否则会出现busy loop，因为已连接套接字一直处于可写状态  
    TcpConnectionPtr conn(new TcpConnection(loop_,
                                            connName,
                                            sockfd,
                                            localAddr,
                                            peerAddr));

    conn->setConnectionCallback(connectionCallback_);
    conn->setMessageCallback(messageCallback_);
    conn->setWriteCompleteCallback(writeCompleteCallback_);
    conn->setCloseCallback(
        boost::bind(&TcpClient::removeConnection, this, _1)); // FIXME: unsafe

    {
        MutexLockGuard lock(mutex_);
        connection_ = conn;
    }

    conn->connectEstablished();
}

// 移除一个链接
void TcpClient::removeConnection(const TcpConnectionPtr& conn) {
    loop_->assertInLoopThread();
    assert(loop_ == conn->getLoop());

    {
        MutexLockGuard lock(mutex_);
        assert(connection_ == conn);
        connection_.reset(); //重置 
    }

      //I/O线程中销毁
    loop_->queueInLoop(boost::bind(&TcpConnection::connectDestroyed, conn));

    if (retry_ && connect_) {
        LOG << "TcpClient::connect[" << name_ << "] - Reconnecting to "
            << connector_->serverAddress().toIpPort() << std::endl;

        //这里的重连是连接成功后断开的重连，所以实际上是重启 
        connector_->restart();
    }
}