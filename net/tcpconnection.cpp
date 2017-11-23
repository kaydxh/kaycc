#include "tcpconnection.h"

#include "../base/weakcallback.h"
#include "channel.h"
#include "eventloop.h"
#include "socket.h"
#include "socketsops.h"
#include "../base/log.h"

#include <boost/bind.hpp>
#include <errno.h>

using namespace kaycc;
using namespace kaycc::net;


/* 
 * 默认的连接完成回调函数 
 */
void kaycc::net::defaultConnectionCallback(const TcpConnectionPtr& conn) {
    LOG << conn->localAddress().toIpPort() << " -> "
        << conn->peerAddress().toIpPort() << " is "
        << (conn->connected() ? "UP" : "DOWN") << std::endl;

    // do not call conn->forceClose(), because some users want to register message callback only.
}


/* 
 * 默认的数据到来的回调函数————丢弃！所以必须要设置自己的消息回调函数 
 */  
void kaycc::net::defaultMessageCallback(const TcpConnectionPtr& , Buffer* buffer, Timestamp) {
    LOG << "defaultMessageCallback in" << std::endl;
    buffer->retrieveAll();
}

TcpConnection::TcpConnection(EventLoop* loop,
                      const std::string& name,
                      int sockfd,
                      const InetAddress& localAddr,
                      const InetAddress& peerAddr)
    : loop_(loop),
      name_(name),
      state_(kConnecting),
      reading_(true),
      socket_(new Socket(sockfd)),
      channel_(new Channel(loop, sockfd)),
      localAddr_(localAddr),
      peerAddr_(peerAddr),
      highWaterMark_(64*1024*1024) { //64MB

    assert(loop_ != NULL);

    channel_->setReadCallback(
        boost::bind(&TcpConnection::handleRead, this, _1));

    channel_->setWriteCallback(
        boost::bind(&TcpConnection::handleWrite, this)); //设置写回调

    channel_->setCloseCallback(
        boost::bind(&TcpConnection::handleClose, this));

    channel_->setErrorCallback(
        boost::bind(&TcpConnection::handleError, this));

    LOG << "TcpConnection::ctor[" << name << "] at " << this
        << " fd=" << sockfd << std::endl;

    socket_->setKeepAlive(true);
}

TcpConnection::~TcpConnection() {
    LOG << "TcpConnection::dtor[" << name_ << "] at " << this
        << " fd=" << channel_->fd()
        << " state=" << stateToString() << std::endl;

    assert(state_ == kDisconnected);
}

// 获取tcp信息 
bool TcpConnection::getTcpInfo(struct tcp_info* tcpi) const {
    return socket_->getTcpInfo(tcpi);
}

// 获取tcp信息的字符串
std::string TcpConnection::getTcpInfoString() const {
    char buf[1024];
    buf[0]= '\0';
    socket_->getTcpInfoString(buf, sizeof(buf));

    return buf;
}

void TcpConnection::send(const void* message, int len) {
    send(std::string(static_cast<const char *>(message), len));
}

void TcpConnection::send(const std::string& message) {
    if (state_ == kConnected) {
        if (loop_->isInLoopThread()) { //loop_如果是所属的io线程，就调用sendInLoop
            sendInLoop(message);

        } else { //loop_如果不是所属的io线程，就转入到io线程发送（ 将该functon保存在队列中，并唤醒wakeupFd_，再在eventloop循环中调用）
            loop_->runInLoop(
                boost::bind(&TcpConnection::sendInLoop,
                            this,
                            message));
        }
    }

}

void TcpConnection::send(Buffer* buf) {
    if (state_ == kConnected) {
        if (loop_->isInLoopThread()) {
            sendInLoop(buf->peek(), buf->readableBytes());
            buf->retrieveAll();

        } else {
            loop_->runInLoop(
                boost::bind(&TcpConnection::sendInLoop,
                            this,
                            buf->retrieveAllAsString()));
        }

    }

}

void TcpConnection::sendInLoop(const std::string& message) {
    sendInLoop(static_cast<const void*>(message.c_str()), message.length());
}

/*
http://blog.csdn.net/freeelinux/article/details/53636117
函数有两个大情况：
    情况1：当我们调用send()时，如果没有关注可写事件并且缓冲区无缓存，那我们直接立即写入。这是为什么呢？
    因为这就是一般异步网络库的通用方法，缓存应用层传过来的数据，在I/O设备可写的情况下尽量写入数据。如果关注了POLLOUT事件，意味着我们要等待POLLOUT事件发生才能写入。
    如果缓冲区有数据，意味者我们必须先写完缓冲区数据，所以都不能立即写。
    写入之后，可能没有写完需要注册writeCompleteCallback事件（这个函数是用户注册的写完数据的回调函数）。如果写出错，且errno!=EWOULDBLOCK，也就是EAGAIN，那就表示出错了。

    writeComplateCallback()函数表示数据发送完毕回调函数，即所有的用户数据都已拷贝到内核缓冲区时调用该回调函数，Buffer被清空也要调用该回调函数，可以理解为lowWaterMarkCallback()函数。上层用户只管调用conn->send()函数发送数据，网络库负责将用户数据拷贝到内核缓冲区。通常情况下编写大流量的应用程序，才需要关注writeCompleteCallback()。大流量的应用程序不断生成数据，然后send()，如果对等方接收不及时，收到通告窗口的影响，发送缓冲区会不足这个时候，就会将用户数据添加到应用层的发送缓冲区，
    可能会撑爆outputBuffer。解决方法就是调整发送频率，只需要关注writeCompleteCallback，当所有用户数据都拷贝到内核缓冲区后，上层用户会得到通知，就可以继续发送数据。
    这需要上层用户的配合。对于低流量的应用程序，通常不需要关注writeCompleteCallback()。

    情况2：情况2与情况1并不能完全分割，如果我们没能够立即写入，即没有执行情况1的if语句，或者执行了但是没有写完，我们需要把剩余数据追加到outputBuffer中。如果我们追加数据，但是readable长度加上我们要追加的超度超过了高水位线，意味着我们逻辑可能有问题。不能总是一加数据就超标，高水位线或许不够高。所以需要让I/O线程执行highWaterMarkCallback()函数，做相应处理（这个函数是用户定义，由用户决定超高水位标是否进行修改）。然后把数据追加到outputBuffer，或许空间不够，但append()函数内部可以处理这个问题，不论是内部调整，还是再申请空间都可以。
    最后，如果没有注册POLLOUT事件，我们需要注册POLLOUT事件，但它发生时，我们的缓冲区数据就可以安心的发送了。
    highWaterMarkCallback()是高水位标回调函数，outputBuffer撑到一定程度时最好回调该函数。这意味着对等方接收不及时，导致outputBuffer不断增大，很可能因为我们没有关注completeWaterMarkCallback()。在这个回调函数中用户就可以断开这个连接，避免内存不足。

    讲到这里，不得不再提一下muduo库所谓的“三个半事件”：
    1.连接建立：服务器accept(被动)接收连接，客户端connect(主动)发起连接。
    2.连接断开：主动断开(close、shutdown)，被动断开(read返回0)
    3.消息到达：文件描述符可读
    4.消息发送完毕：这算半个事件。对于低流量的服务，可以不关心这个事件。这里的发送完毕是数据写入操作系统缓冲区，将由TCP协议栈法则数据的发送与重传，不代表对方已经接数据。
    高流量服务可能导致内核缓冲区满了，数据还会追加到应用层缓冲区。

    上面我们进行实际上就是最后的这“消息发送完毕”的半个事件直接写入套接字实际上就是直接写入操作系统缓冲区，然后写入数据有剩余的情况下，我们把数据追加到Buffer中。
    至于数据到底有没有发送到对端，这让TCP去负责，所以就是半个事件。

此外，还有一个函数，我们上面那个函数末尾进行了enableWriting，注册了关注POLLOUT事件，事件发生后会调用TcpConnection的handleWrite()函数：
*/

void TcpConnection::sendInLoop(const void* data, size_t len) {
    loop_->assertInLoopThread();
    ssize_t nwrote = 0;
    size_t remaining = len;
    bool faultError = false;
    if (state_ == kDisconnected) {
        LOG << "disconnected, give up writing" << std::endl;
        return;
    } 

    // if no thing in output queue, try writing directly
    /* 
     * 如果事件处理器禁用了写功能（或者说不处理写事件） 
     * 而且输出缓冲区中的数据已经全部发送完毕 
     * 那么就不通过缓冲区直接发送数据 
    */ 

    //outputBuffer_.readableBytes()表示需要发送的数据
    if (!channel_->isWriting() && outputBuffer_.readableBytes() == 0) {
        nwrote = sockets::write(channel_->fd(), data, len);

        if (nwrote >= 0) {
            remaining = len - nwrote;

            if (remaining == 0 && writeCompleteCallback_) {
                loop_->queueInLoop(boost::bind(writeCompleteCallback_, shared_from_this()));
            }

        } else { // nwrote < 0
            nwrote = 0;
            if (errno != EWOULDBLOCK) {
                LOG << "TcpConnection::sendInLoop" << std::endl;

                if (errno == EPIPE || errno == ECONNRESET) {
                    faultError = true;
                }
            }
        }

    }

    assert(remaining <= len);

    if (!faultError && remaining > 0) {
        size_t oldLen = outputBuffer_.readableBytes(); //缓存中有需要发送的老数据

        if (oldLen + remaining >= highWaterMark_ //缓存中的老数据 + 这次还剩下的发送数据 >= highWaterMark_
            && oldLen < highWaterMark_
            && highWaterMarkCallback_) {
            loop_->queueInLoop(boost::bind(highWaterMarkCallback_, shared_from_this(), oldLen + remaining));
        }

        // 把剩余数据追加到outputbuffer，并注册POLLOUT事件 
        outputBuffer_.append(static_cast<const char*>(data) + nwrote, remaining);

        if (!channel_->isWriting()) {
            channel_->enableWriting();
        }

    }

}

// NOT thread safe, no simultaneous calling
void TcpConnection::shutdown() {
    if (state_ == kConnected) {
        setState(kDisconnecting);
        loop_->runInLoop(boost::bind(&TcpConnection::shutdownInLoop, this));
    }
}

// 在循环中关闭写端
void TcpConnection::shutdownInLoop() {
    loop_->assertInLoopThread();

    if (!channel_->isWriting()) {
        socket_->shutdownWrite();
    }
}

// 强制关闭 
void TcpConnection::forceClose() {
    if (state_ == kConnected || state_ == kDisconnecting) {
        setState(kDisconnecting);
        loop_->queueInLoop(boost::bind(&TcpConnection::forceCloseInLoop, shared_from_this()));
    }

}

// 带延时的强制关闭 
void TcpConnection::forceCloseWithDelay(double seconds) {
    if (state_ == kConnected || state_ == kDisconnecting) {
        setState(kDisconnecting);
        loop_->runAfter(
            seconds,
            makeWeakCallback(shared_from_this(),
                             &TcpConnection::forceClose)); //// not forceCloseInLoop to avoid race condition
    }
}

// 强制退出循环
void TcpConnection::forceCloseInLoop() {
    loop_->assertInLoopThread();

    if (state_ == kConnected || state_ == kDisconnecting) {
        // as if we received 0 byte in handleRead();
        handleClose();
    }
}

// 将状态转换为字符串 
const char* TcpConnection::stateToString() const {
    switch (state_) {
        case kDisconnected:
            return "kDisconnected";
        case kConnecting:
            return "kConnecting";
        case kConnected:
            return "kConnected";
        case kDisconnecting:
            return "kDisconnecting";
        default:
            return "unknown state";
    }
}

// 关闭或开启Nagle算法  
void TcpConnection::setTcpNoDelay(bool on) {
    socket_->setTcpNoDelay(on);
}

void TcpConnection::startRead() {
    loop_->runInLoop(boost::bind(&TcpConnection::startReadInLoop, this));
}

void TcpConnection::startReadInLoop() {
    loop_->assertInLoopThread();
    if (!reading_ || !channel_->isReading()) {
        channel_->enableReading(); //关注读事件
        reading_ = true;
    }
}

void TcpConnection::stopRead() {
    loop_->runInLoop(boost::bind(&TcpConnection::stopReadInLoop, this));
}

void TcpConnection::stopReadInLoop() {
    loop_->assertInLoopThread();
    if (reading_ || channel_->isReading()) {
        channel_->disableReading();
        reading_ = false;
    }
}

// called when TcpServer accepts a new connection   should be called only once
void TcpConnection::connectEstablished() {
    loop_->assertInLoopThread();
    assert(state_ == kConnecting);

    setState(kConnected);
    channel_->tie(shared_from_this());
    channel_->enableReading();

    // 调用用户的连接回调函数（建立连接、断开链接都可以使用）
    connectionCallback_(shared_from_this());
}

// called when TcpServer has removed me from its map  should be called only once
void TcpConnection::connectDestroyed() {
    loop_->assertInLoopThread();

    if (state_ == kConnected) {
        setState(kDisconnected);
        channel_->disableAll();

        connectionCallback_(shared_from_this());
    }

    channel_->remove();
}

// 处理读
void TcpConnection::handleRead(Timestamp receiveTime) {
    loop_->assertInLoopThread();
    int savedErrno = 0;

    ssize_t n = inputBuffer_.readFd(channel_->fd(), &savedErrno);
    if (n > 0) {
         // 调用用户的数据到来回调函数
        messageCallback_(shared_from_this(), &inputBuffer_, receiveTime);
    } else if (n == 0) {
        handleClose(); //对方关闭socket，发送fin，关闭连接
    } else {
        errno = savedErrno;
        LOG << "TcpConnection::handleRead" << std::endl;
        handleError();
    }

}

// 处理写
void TcpConnection::handleWrite() {
    loop_->assertInLoopThread();
    if (channel_->isWriting()) {
        ssize_t n = sockets::write(channel_->fd(),
                                  outputBuffer_.peek(),
                                  outputBuffer_.readableBytes());

        // 正确写入
        if (n > 0) {
            outputBuffer_.retrieve(n);

            // 如果可读的数据量为0表示所有数据都被发送完毕了，即写完成了 
            if (outputBuffer_.readableBytes() == 0) {
                channel_->disableWriting();
                if (writeCompleteCallback_) {
                    // 调用用户的写完成回调函数
                    loop_->queueInLoop(boost::bind(writeCompleteCallback_, shared_from_this()));
                }

                // 如果当前状态是正在关闭连接  
                // 那么就调用shutdown来主动关闭连接 
                if (state_ == kDisconnecting) {
                    shutdownInLoop();
                }
            }

        } else { //n <= 0   写入错误
            LOG << "TcpConnection::handleWrite failed: " << n << std::endl;

        }

    } else {
        LOG << "Connection fd =" << channel_->fd()
            << " is down, no more writing" << std::endl;
    }
}

// 处理关闭
void TcpConnection::handleClose() {
    loop_->assertInLoopThread();
    LOG << "fd = " << channel_->fd() << " state = " << stateToString() << std::endl;

    assert(state_ == kConnected || state_ == kDisconnecting);
    // we don't close fd, leave it to dtor, so we can find leaks easily.
    setState(kDisconnected);
    channel_->disableAll();

    TcpConnectionPtr guardThis(shared_from_this());
    connectionCallback_(guardThis);

    closeCallback_(guardThis);
}

// 处理错误 
void TcpConnection::handleError() {
    int err = sockets::getSocketError(channel_->fd());
    LOG << "TcpConnection::handleError [" << name_
        << "] - SO_ERROR = " << err << std::endl;

}