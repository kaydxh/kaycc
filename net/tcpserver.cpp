#include "tcpserver.h"

#include "acceptor.h"
#include "eventloop.h"
#include "eventloopthreadpool.h"
#include "socketsops.h"
#include "../base/log.h"

#include <boost/bind.hpp>

#include <stdio.h> // snprintf

using namespace kaycc;
using namespace kaycc::net;


TcpServer::TcpServer(EventLoop* loop,
                    const InetAddress& listenAddr,
                    const std::string& name,
                    Option option)
    : loop_(loop),
      ipPort_(listenAddr.toIpPort()),
      name_(name),
      acceptor_(new Acceptor(loop, listenAddr, option == kReusePort)),
      threadPool_(new EventLoopThreadPool(loop, name_)),
      connectionCallback_(defaultConnectionCallback),
      messageCallback_(defaultMessageCallback),
      nextConnId_(1) {

    acceptor_->setNewConnectionCallback(
        boost::bind(&TcpServer::newConnection, this, _1, _2));

}

TcpServer::~TcpServer() {
    loop_->assertInLoopThread();
    LOG << "cpServer::~TcpServer [" << name_ << "] destructing" << std::endl;

     // 断开每一个连接 
    for (ConnectionMap::iterator it(connections_.begin()); 
        it != connections_.end(); ++it) {

        TcpConnectionPtr conn(it->second);
        it->second.reset();

        // 销毁连接
        conn->getLoop()->runInLoop(
            boost::bind(&TcpConnection::connectDestroyed, conn)); 
    }

}

void TcpServer::setThreadNum(int numThreads) {
    assert(0 <= numThreads);
    threadPool_->setThreadNum(numThreads);
}

void TcpServer::start() {
    if (started_.getAndSet(1) == 0) { //设置为1，返回之前的值,以后都为1就不会进入if语句  
        threadPool_->start(threadInitCallback_);

        assert(!acceptor_->listenning());
        loop_->runInLoop(
            boost::bind(&Acceptor::listen, get_pointer(acceptor_))); //get_pointer(acceptor_) 返回acceptor_.get()
    }

}

/*
实际上TcpConnection的功能已经很明显了，就是对已连接套接字的一个抽象。
TcpServer对客端而言就是一个服务器，客端使用TcpServer自然会在它上面设置各种自己网络程序的回调函数，对各种连接的处理。
所以TcpConnection就会在TcpServer中封装已连接套接字和回调函数，加入到round-robin所选择的Reactor之中，
然后该fd以后触发的所有可读可写事件都由该Reactor处理。这就是multi Reactor的思想。
*/

/// Not thread safe, but in loop
/// 新连接到来回调函数
void TcpServer::newConnection(int sockfd, const InetAddress& peerAddr) {
    loop_->assertInLoopThread();
    EventLoop* ioLoop = threadPool_->getNextLoop();

    char buf[64];
    snprintf(buf, sizeof(buf), "-%s#%d", ipPort_.c_str(), nextConnId_); ////端口+连接id 
    ++nextConnId_; //++之后就是下一个连接id 

    std::string connName = name_ + buf;
    LOG << "TcpServer::newConnection [" << name_
        << "] - new connection [" << connName
        << "] from " << peerAddr.toIpPort() << std::endl;

    InetAddress localAddr(sockets::getLocalAddr(sockfd));

    TcpConnectionPtr conn(new TcpConnection(ioLoop, // //创建一个连接对象，ioLoop是round-robin选择出来的  
                                            connName,
                                            sockfd,
                                            localAddr,
                                            peerAddr));

    //TcpConnection的引用计数此处为1，新建了一个Tcpconnection  
    connections_[connName] = conn;
    //TcpConnection的引用计数此处为2，因为加入到connections_中

    ////实际TcpServer的connectionCallback等回调函数是对conn的回调函数的封装，所以在这里设置过去 
    conn->setConnectionCallback(connectionCallback_);
    conn->setMessageCallback(messageCallback_);
    conn->setWriteCompleteCallback(writeCompleteCallback_);

    //将TcpServer的removeConnection设置了TcpConnection的关闭回调函数中
    conn->setCloseCallback(
        boost::bind(&TcpServer::removeConnection, this, _1)); // FIXME: unsafe 

    ioLoop->runInLoop(boost::bind(&TcpConnection::connectEstablished, conn));
    //调用TcpConenction:;connectEstablished函数内部会将引用计数加一然后减一，此处仍为2  
    //但是本函数介绍结束后conn对象会析构掉，所以引用计数为1，仅剩connections_列表中存活一个  
}

/// Thread safe.
/// 删除连接
void TcpServer::removeConnection(const TcpConnectionPtr& conn) {
    loop_->runInLoop(boost::bind(&TcpServer::removeConnectionInLoop, this, conn));
}


/// Not thread safe, but in loop
void TcpServer::removeConnectionInLoop(const TcpConnectionPtr& conn) {
    loop_->assertInLoopThread();

    LOG << "TcpServer::removeConnectionInLoop [" << name_
        << "] - connection " << conn->name() << std::endl;

    size_t n = connections_.erase(conn->name());
    (void)n;
    assert(n == 1);

    EventLoop* ioLoop = conn->getLoop();

    ioLoop->queueInLoop(
        boost::bind(&TcpConnection::connectDestroyed, conn));
}


