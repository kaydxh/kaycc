#include "acceptor.h"

#include "eventloop.h"
#include "inetaddress.h"
#include "socketsops.h"

#include <boost/bind.hpp>

#include <errno.h>
#include <fcntl.h> //open
#include <unistd.h>

#include "../base/log.h"

using namespace kaycc;
using namespace kaycc::net;



Acceptor::Acceptor(EventLoop* loop, const InetAddress& listenAddr, bool reuseport)
    : loop_(loop),
      acceptSocket_(sockets::createNonblockingOrDie(listenAddr.family())),
      acceptChannel_(loop, acceptSocket_.fd()),
      listenning_(false),
      idleFd_(::open("/dev/null", O_RDONLY | O_CLOEXEC)) { //返回一个文件描述符，成功返回大于等于0的数，出错返回-1

    assert(idleFd_ >= 0);

    // 地址复用 
    acceptSocket_.setReuseAddr(true);

    // 端口复用
    acceptSocket_.setReusePort(reuseport);

    // 绑定地址 
    acceptSocket_.bindAddress(listenAddr);

    // 设置读事件的回调函数
    acceptChannel_.setReadCallback(boost::bind(&Acceptor::handleRead, this));

}

Acceptor::~Acceptor() {
    acceptChannel_.disableAll();
    acceptChannel_.remove();
    ::close(idleFd_);
}

//监听
void Acceptor::listen() {
    loop_->assertInLoopThread();
    listenning_ = true;
    acceptSocket_.listen();

    // 启用事件处理器的读功能
    acceptChannel_.enableReading();
}

// 处理读事件，acceptChannel_的读事件回调函数 

/*
写服务器应用程序必须要考虑到服务器资源不足的情况，其中常见的一个是打开的文件数量（文件描述符数量）不能超过系统限制，当接受的连接太多时就会到达系统的限制，即表示打开的套接字文件描述符太多，从而导致accpet失败，返回EMFILE错误，但此时连接已经在系统内核建立好了，所以占用了系统的资源，我们不能让接受不了的连接继续占用系统资源，如果不处理这种错误就会有越来越多的内核连接建立，系统资源被占用也会越来越多，直到系统崩溃。

一个常见的处理方式就是，先打开一个文件，预留一个文件描述符，出现EMFILE错误的时候，把打开的文件关闭，此时就会空出一个可用的文件描述符，再次调用accept就会成功，接受到客户连接之后，我们马上把它关闭，这样这个连接在系统中占用的资源就会被释放。关闭之后又会有一个文件描述符空闲，我们再次打开一个文件，占用文件描述符，等待下一次的EMFILE错误
*/

void Acceptor::handleRead() {
    loop_->assertInLoopThread();
    InetAddress peerAddr;

    // 接受一个连接
    int connfd = acceptSocket_.accept(&peerAddr);
    if (connfd > 0) {
         // 调用新连接到来回调函数 
        if (newConnectionCallback_) {
            newConnectionCallback_(connfd, peerAddr);
        } else {
            sockets::close(connfd);
        }

    } else {
        LOG << "in Acceptor::handleRead failed." << connfd << std::endl;
        // 发生文件描述符不够用的情况 
        if (errno == EMFILE) {
            // 关闭预留的文件描述符
            ::close(idleFd_);

            // 然后立即打开，即可得到一个可用的文件描述符，马上接受新连接
            idleFd_ = ::accept(acceptSocket_.fd(), NULL, NULL);

            // 然后立即关闭这个连接，表示服务器不再提供服务，因为系统资源已经不足  
            // 服务器使用这个方法莱拒绝客户的连接
            ::close(idleFd_);

            //再次打开一个文件，占用文件描述符，等待下一次的EMFILE错误
            idleFd_ = ::open("/dev/null", O_RDONLY| O_CLOEXEC);
        }

    }

}