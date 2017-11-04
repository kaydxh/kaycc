#include "epollpoller.h"

#include "../channel.h"

//#include <boost/static_assert.hpp>

#include <assert.h>
#include <errno.h>
#include <sys/epoll.h>
#include <unistd.h>

using namespace kaycc;
using namespace kaycc::net;

namespace {
    const int kNew = -1;
    const int kAdded = 1;
    const int kDeleted = 2;
}


//当flag = EPOLL_CLOEXEC，创建的epfd会设置FD_CLOEXEC
//当flag = EPOLL_NONBLOCK，创建的epfd会设置为非阻塞
//FD_CLOEXEC是fd的一个标识说明，用来设置文件close-on-exec状态的。
//当close-on-exec状态为0时，调用exec时，fd不会被关闭；状态非零时则会被关闭，这样做可以防止fd泄露给执行exec后的进程。

EPollPoller::EPollPoller(EventLoop* loop)
    : Poller(loop),
      epollfd_(::epoll_create1(EPOLL_CLOEXEC)), 
      events_(kInitEventListSize) { //vector这样用时初始化kInitEventListSize个大小空间  

    if (epollfd_ < 0) {
        std::cout << "EPollPoller::EPollPoller failed." << std::endl;
    }

}

EPollPoller::~EPollPoller() {
    ::close(epollfd_);
}

Timestamp EPollPoller::poll(int timeoutMs, ChannelList* activeChannels) {
    std::cout << "fd total count " << channels_.size() << std::endl;

    int numEvents = ::epoll_wait(epollfd_,  ///使用epoll_wait()，等待事件返回,返回发生的事件数目  
                                 &*events_.begin(),
                                 static_cast<int>(events_.size()),
                                 timeoutMs);
    int savedErrno = errno;
    Timestamp now(Timestamp::now()); //得到时间戳  
    if (numEvents > 0) {
        std::cout << numEvents << " events happended." << std::endl;
        fillActiveChannels(numEvents, activeChannels); //调用fillActiveChannels，传入numEvents也就是发生的事件数目

        if (implicit_cast<size_t>(numEvents) == events.size()) { //如果返回的事件数目等于当前事件数组大小，就分配2倍空间  
            events_.resize(events.size()*2);
        }

    } else if (numEvents == 0) {
        std::cout << "nothing happended" << std::endl

    } else {

        if (savedErrno != EINTER) { //如果不是EINTR信号，就把错误号保存下来，并且输入到日志中  
            errno = savedErrno;
            std::cout << "EPollPoller::poll() error not EINTER:" << savedErrno << std::endl
        }

    }
 
}

void EPollPoller::fillActiveChannels(int numEvents, ChannelList* activeChannels) const {
    assert(implicit_cast<size_t>(numEvents) <= events_.size());

    for (int i = 0; i < numEvents; ++i) { //挨个处理发生的numEvents个事件，epoll模式返回的events_数组中都是已经发生的事件，这有别于select和poll  
        Channel* channel = static_cast<Channel*>(events_[i].data.ptr);

    #ifndef NDEBUG 
        int fd = channel->fd();
        ChannelMap::const_iterator it = channels_.find(fd);
        assert(it != channels_.end());
        assert(it->second == channel);
    #endif

        channel->set_revent(events_[i].events); //把已发生的事件传给channel,写到通道当中
        activeChannels->push_back(channel); //并且push_back进activeChannels
    }

}

//这个函数被调用是因为channel->enablereading()被调用，再调用channel->update()，再event_loop->updateChannel()，再->epoll或poll的updateChannel被调用
void EPollPoller::updateChannel(Channel* channel) {
    Poller::assertInLoopThread();
    const int index = channel->index();
    std::cout << "fd=" << channel->fd() << " events=" << channel->events() << " index=" << index;

    if (index == kNew || index == kDeleted) {
        int fd = channel->fd();
        if (index == kNew) { //a new one
            assert(channels_.find(fd) == channels_.end());
            channels_[fd] = channel;
        } else { // 已经删除的
            assert(channels_.find(fd) != channels_.end());
            assert(channels_[fd] == channel);
        }

        channel->set_index(kAdded);
        update(EPOLL_CTL_ADD, channel);

    } else { // update existing one with EPOLL_CTL_MOD/DEL
        int fd = channel->fd();
        (void)fd;

        assert(channels_.find(fd) != channels_.end());
        assert(channels_[fd] == channel);
        assert(index == kAdded);

        if (channel->isNoneEvent()) { //如果什么也没关注，就直接干掉
            update(EPOLL_CTL_DEL, channel);
            channel->set_index(kDeleted); //删除之后设为deleted，表示已经删除，只是从内核事件表中删除，在channels_这个通道数组中并没有删除  
        } else {
            update(EPOLL_CTL_MOD, channel); //有关注，那就只是更新。更新成什么样子channel中会决定。
        }

    }

}

void EPollPoller::removeChannel(Channel* channel) {
    Poller::assertInLoopThread();
    int fd = channel->fd();
    std::cout << "fd = " << fd << std::endl;
    assert(channels_.find(fd) != channels_.end());
    assert(channels_[fd] == channel);
    assert(channel->isNoneEvent());

    int index = channel->index(); 
    assert(index == kAdded || index == kDeleted);
    size_t n = channels_.erase(fd);
    (void)n;
    assert(n == 1);

    if (index == kAdded) {
        update(EPOLL_CTL_DEL, channel);
    }

    channel->set_index(kNew);
}

void EPollPoller::update(int operation, Channel* channel) {
    struct epoll_event event;
    bzero(&event, sizeof(event));
    event.events = channel->events();
    event.data.ptr = channel;

    int fd = channel->fd();

    std::cout << "epoll_ctl op = " << operationToString(operation)
        << " fd = " << fd << " event = { " << channel->eventsToString() << " }" << std::endl;

    if (::epoll_ctl(epollfd_, operation, fd, &event) < 0) {
        if (operation == EPOLL_CTL_DEL) {
            std::cout <<  "epoll_ctl op =" << operationToString(operation) << " fd=" << fd;
            
        } else {
            std::cout <<  "epoll_ctl op =" << operationToString(operation) << " fd=" << fd;
        }

    }

}

const char* EPollPoller::operationToString(int op) {
    switch (op) {
        case EPOLL_CTL_ADD:
            return "ADD";
        case EPOLL_CTL_DEL:
            return "DEL";
        case EPOLL_CTL_MOD:
            return "MOD";
        default:
            assert(false && "ERROR op")
            return "Unknown Operation";


    }

}