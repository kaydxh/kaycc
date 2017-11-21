#include "connector.h"

#include "channel.h"
#include "eventloop.h"
#include "socketsops.h"
#include "../base/log.h"

#include <boost/bind.hpp>

#include <errno.h>

using namespace kaycc;
using namespace kaycc::net;

const int Connector::kMaxRetryDelayMs;

Connector::Connector(EventLoop* loop, const InetAddress& serverAddr)
    : loop_(loop),
      serverAddr_(serverAddr),
      connect_(false),
      state_(kDisconnected),
      retryDelayMs_(kInitRetryDelayMs) {

    LOG << "ctor[" << this << "] Connector" << std::endl;

}

Connector::~Connector() {
    LOG << "dtor[" << this << "] Connector" << std::endl;
    assert(!channel_);
}

// 开始启动连接 // can be called in any thread
void Connector::start() {
    connect_ = true;
    loop_->runInLoop(boost::bind(&Connector::startInLoop, this)); // FIXME: unsafe
} 

// 启动连接
void Connector::startInLoop() {
    loop_->assertInLoopThread();
    assert(state_ == kDisconnected);
    if (connect_) { //调用前必须connect_为true，start()函数中会这么做  
        connect();
    } else {
        LOG << "do not connect" << std::endl;
    }
}

// 停止连接 can be called in any thread
void Connector::stop() {
    connect_ = false;
    loop_->queueInLoop(boost::bind(&Connector::stopInLoop, this));
} 

// 停止连接
void Connector::stopInLoop() {
    loop_->assertInLoopThread();
    if (state_ == kConnecting) {
        setState(kDisconnected);
        int sockfd = removeAndResetChannel();
        retry(sockfd);
    }
}

// 连接 
void Connector::connect() {
    int sockfd = sockets::createNonblockingOrDie(serverAddr_.family());
    int ret = sockets::connect(sockfd, serverAddr_.getSockAddr());
    int savedErrno = (ret == 0) ? 0 : errno;
    switch (savedErrno) {
        case 0:
        case EINPROGRESS: //非阻塞套接字，未连接成功返回码是EINPROGRESS表示正在连接  
        case EINTR:
        case EISCONN: //连接成功
            connecting(sockfd);
            break;

        //以 O_NONBLOCK的标志打开文件/socket/
        //FIFO，如果你连续做read操作而没有数据可读。此时程序不会阻塞起来等待数据准备就绪返回，read函数会返回一个错误EAGAIN，提示你的应用程序现在没有数据可读请稍后再试。
        //在linux进行非阻塞的socket接收数据时经常出现Resource temporarily unavailable，errno代码为11(EAGAIN)
        //这表明你在非阻塞模式下调用了阻塞操作，在该操作没有完成就返回这个错误，这个错误不会破坏socket的同步，不用管它，下次循环接着recv就可以。对非阻塞socket而言，EAGAIN不是一种错误
        case EAGAIN:
        case EADDRINUSE: //地址已经被使用

        //在调用TCP 
        //connect时，Linux内核会在某个范围内选择一个可用的端口作为本地端口去connect服务器，如果没有可用的端口可用，比如这个范围内的端口都处于如下状态中的一种：
        //1. bind使用的端口
        //2. 端口处于非TIME_WAIT状态
        //3. 端口处于TIME_WAIT状态，但是没有启用tcp_tw_reuse
        //那么就会返回EADDRNOTAVAIL错误。
        case EADDRNOTAVAIL:
        case ECONNREFUSED: //到达目的主机后，由于各种原因建立不了连接，主机返回RST（复位）响应，例如主机监听进程未启用，tcp取消连接等
        case ENETUNREACH: //客户机的SYN数据段导致某个路由器产生“目的地不可到达”类型的ICMP消息，函数以该错误返回
            retry(sockfd);
            break;

        case EACCES: //Permission denied
        case EPERM:  // Operation not permitted 
        case EAFNOSUPPORT: //Address family not supported by protocol
        case EALREADY: //  Operation already in progress
        case EBADF:  //错误的文件编号
        case EFAULT: //坏地址
        case ENOTSOCK: //在非socket上执行socket操作
            LOG << "connect error in Connector::startInLoop " << savedErrno << std::endl;
            sockets::close(sockfd);
            break;

        default:
            LOG << "Unexpected error in Connector::startInLoop " << savedErrno << std::endl;
            sockets::close(sockfd);
            break;
    }

}

// 重新连接  must be called in loop thread 
void Connector::restart() {
    loop_->assertInLoopThread();
    setState(kDisconnected);
    retryDelayMs_ = kInitRetryDelayMs;
    connect_ = true;

    // 发起连接
    startInLoop();
}

 // 正在连接
void Connector::connecting(int sockfd) {
    setState(kConnecting);
    assert(!channel_);

    //Channel与sockfd关联
    channel_.reset(new Channel(loop_, sockfd));

    //设置可写回调函数，这时候如果socket没有错误，sockfd就处于可写状态
    channel_->setWriteCallback(
        boost::bind(&Connector::handleWrite, this)); //// FIXME: unsafe  

    channel_->setErrorCallback(
        boost::bind(&Connector::handleError, this)); //FIXME: unsafe  

    // channel_->tie(shared_from_this()); is not working,
    // as channel_ is not managed by shared_ptr
    channel_->enableWriting();
}

// 移除并重置事件通道
// 每次重连都会创建一个新的事件处理器，旧的事件处理器会被删除
int Connector::removeAndResetChannel() {
    channel_->disableAll();
    channel_->remove();
    int sockfd = channel_->fd();

    // 重置事件处理器
    loop_->queueInLoop(boost::bind(&Connector::resetChannel, this));
    return sockfd;
}

//重置事件通道
void Connector::resetChannel() {
    channel_.reset();
}

// 处理写事件
void Connector::handleWrite() {
    LOG << "Connector::handleWrite " << state_ << std::endl;

    // 如果状态是正在连接
    if (state_ == kConnecting) {
        // 移除并重置事件处理器
        int sockfd = removeAndResetChannel();
        int err = sockets::getSocketError(sockfd);
        if (err) {
            LOG << "Connector::handleWrite - SO_ERROR = " << err << std::endl;
            retry(sockfd);

        // 发生了自己对自己的连接，重连 
        } else if (sockets::isSelfConnect(sockfd)) {
            LOG << "Connector::handleWrite - Self connect" << std::endl;
            retry(sockfd);

        } else {
            setState(kConnected);
            if (connect_) {
                 // 调用用户的连接完成回调函数
                newConnectionCallback_(sockfd);
            } else {
                sockets::close(sockfd);
            }

        }

    } else {
        // what happened?
        assert(state_ == kDisconnected);
    }

}

// 处理错误
void Connector::handleError() {
    LOG << "Connector::handleError state=" << state_ << std::endl;

    // 如果是正在连接 
    if (state_ == kConnecting) {
        // 移除并重置事件处理器
        int sockfd = removeAndResetChannel();

        // 获取错误码
        int err = sockets::getSocketError(sockfd);
        LOG << "SO_ERROR = " << err << std::endl;
        retry(sockfd);
    }

}

// 重试
void Connector::retry(int sockfd) {
    sockets::close(sockfd);
    setState(kDisconnected);

    if (connect_) {
        LOG << "Connector::retry - Retry connecting to " << serverAddr_.toIpPort()
            << " in " << retryDelayMs_ << " milliseconds.";

        // 指定时间之后重试 
        loop_->runAfter(retryDelayMs_/1000.0, 
                        boost::bind(&Connector::startInLoop, shared_from_this()));

        // 重新计算重试间隔时间
        retryDelayMs_ = std::min(retryDelayMs_ * 2, kMaxRetryDelayMs);

    } else {
        LOG << "do not connect" << std::endl;
    }

}
