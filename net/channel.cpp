#include "channel.h"
#include "eventloop.h"

#include <sstream>
#include <iostream>

#include <poll.h>
using namespace kaycc;
using namespace kaycc::net;

const int Channel::kNoneEvent = 0;

//POLLIN 　　　　有数据可读。
//POLLPRI　　　　有紧迫数据可读
const int Channel::kReadEvent = POLLIN | POLLPRI;  

//POLLOUT      写数据不会导致阻塞
const int Channel::kWriteEvent = POLLOUT;

Channel::Channel(EventLoop* loop, int fd)
    : loop_(loop),
      fd_(fd),
      events_(0),
      revents_(0),
      index_(0),
      tied_(false),
      eventHandling_(false),
      addedToLoop_(false) {

}

Channel::~Channel() {
    assert(!eventHandling_);
    assert(!addedToLoop_);

    if (loop_->isInLoopThread()) {
        assert(!loop_->hasChannel(this));
    }
}

void Channel::tie(const boost::shared_ptr<void>& obj) {
    tie_ = obj;
    tied_ = true; 
}

void Channel::update() {
    addedToLoop_ = true;
    loop_->updateChannel(this);
}

// 从Reactor(EventLoop)中移除它自己
void Channel::remove() {
    assert(isNoneEvent());
    addedToLoop_ = false;
    loop_->removeChannel(this);
}

void Channel::handleEvent(Timestamp receiveTime) {
    boost::shared_ptr<void> guard;
    if (tied_) {
        guard = tie_.lock();
        if (guard) {
            handleEventWithGuard(receiveTime);
        }
    } else {
        handleEventWithGuard(receiveTime);
    }

}

void Channel::handleEventWithGuard(Timestamp receiveTime) {
    eventHandling_ = true;

    //POLLHUP　　 指定的文件描述符挂起事件
    if ((revents_ & POLLHUP) && !(revents_ & POLLIN)) {
        if (closeCallback_) {
            closeCallback_();
        }
    }

    if (revents_ & POLLNVAL) {
        std::cout << "fd = " << fd_ << " Channel::handleEvent POLLNVAL" << std::endl;
    }

    //POLLERR，仅用于内核设置传出参数revents，表示设备发生错误
    //POLLNVAL，仅用于内核设置传出参数revents，表示非法请求文件描述符fd没有打开
    if (revents_ & (POLLERR | POLLNVAL)) {
        if (errorCallback_) {
            errorCallback_();
        }
    }

    //POLLIN 　　　　有数据可读。
    //POLLPRI　　　　有紧迫数据可读
    if (revents_ & (POLLIN | POLLPRI | POLLRDHUP)) {
        if (readCallback_) {
            readCallback_(receiveTime);
        }
    }

    if (revents_ & POLLOUT) {
        if (writeCallback_) {
            writeCallback_();
        }
    }

    eventHandling_ = false;

}

/*
events 和 revents能够设置的值都定义在<poll.h>头中，有以下几种可能

POLLIN ，读事件
POLLPRI，读事件，但表示紧急数据，例如tcp socket的带外数据
POLLRDNORM , 读事件，表示有普通数据可读　　　  
POLLRDBAND ,　读事件，表示有优先数据可读　　　　 
POLLOUT，写事件
POLLWRNORM , 写事件，表示有普通数据可写
POLLWRBAND ,　写事件，表示有优先数据可写　　　   　　　　  
POLLRDHUP (since Linux 2.6.17)，Stream socket的一端关闭了连接（注意是stream socket，我们知道还有raw socket,dgram socket），或者是写端关闭了连接，如果要使用这个事件，必须定义_GNU_SOURCE 宏。这个事件可以用来判断链路是否发生异常（当然更通用的方法是使用心跳机制）。要使用这个事件，得这样包含头文件：
　　#define _GNU_SOURCE  
　　#include <poll.h>
POLLERR，仅用于内核设置传出参数revents，表示设备发生错误
POLLHUP，仅用于内核设置传出参数revents，表示设备被挂起，如果poll监听的fd是socket，表示这个socket并没有在网络上建立连接，比如说只调用了socket()函数，但是没有进行connect。
POLLNVAL，仅用于内核设置传出参数revents，表示非法请求文件描述符fd没有打开
*/

std::string Channel::eventsToString(int fd, int ev) {
    std::ostringstream oss;
    oss << fd << ": ";
    if (ev & POLLIN) {
        oss << "IN ";
    }
    if (ev & POLLPRI) {
        oss << "PRI ";
    }
    if (ev & POLLOUT) {
        oss << "OUT ";
    }
    if (ev & POLLHUP) {
        oss << "HUP ";
    }
    if (ev & POLLRDHUP) {
        oss << "RDHUP ";
    }
    if (ev & POLLERR) {
        oss << "ERR ";
    }
    if (ev & POLLNVAL) {
        oss << "NVAL ";
    }

    return oss.str();
}

std::string Channel::reventsToString() const {
    return eventsToString(fd_, revents_);
}

std::string Channel::eventsToString() const {
    return eventsToString(fd_, events_);
}




