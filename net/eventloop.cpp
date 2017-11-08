#include "eventloop.h"

#include "../base/mutex.h"
#include "channel.h"
#include "poller.h"
#include "socketsops.h"
#include "timerqueue.h"

#include <boost/bind.hpp>

#include <signal.h>
#include <sys/eventfd.h>
#include <unistd.h>
#include <iostream>

using namespace kaycc;
using namespace kaycc::net;

namespace {
    __thread EventLoop* t_loopInThisThread = 0;

    const int kPollTimeMs = 10000;

/**
    eventfd()创建了一个"eventfd object"，能在用户态用做事件wait/notify机制，通过内核取唤醒用户态的事件。这个对象保存了一个内核维护的uint64_t类型的整型counter。这个counter初始值被参数initval指定，一般初值设置为0。
*/
    int createEventfd() {
        int evtfd = ::eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
        if (evtfd < 0) {
            std::cout << "eventfd failed." << std::endl;
            abort();
        }

        return evtfd;
    }

    class IgnoreSigPipe {
    public:
        IgnoreSigPipe() {
            ::signal(SIGPIPE, SIG_IGN);//对一个对端已经关闭的socket调用两次write, 第一次会收到一个RST响应，第二次将会生成SIGPIPE信号, 该信号默认结束进程.
            //所以这里忽略SIGPIPE信号
            // LOG_TRACE << "Ignore SIGPIPE";
        }

    };

    // 一个全局变量，用于实现对SIGPIPE信号的忽略
    IgnoreSigPipe initObj;
}

EventLoop*  EventLoop::getEventLoopOfCurrentThread() {
    return t_loopInThisThread;
}

EventLoop::EventLoop()
    : looping_(false),
      quit_(false),
      eventHandling_(false),
      callingPendingFunctors_(false),
      iteration_(0),
      threadId_(currentthread::tid()),
      poller_(Poller::newDefaultPoller(this)),
      timerQueue_(new TimerQueue(this)),
      wakeupFd_(createEventfd()),
      wakeupChannel_(new Channel(this, wakeupFd_)),
      currentActiveChannel_(NULL) {
        std::cout << "EventLoop created " << this << " in thread " << threadId_ << std::endl;
        if (t_loopInThisThread) {
            std::cout << "another event loop " << t_loopInThisThread << " exists in this thread " << threadId_ << std::endl; 

        } else {
            t_loopInThisThread = this;
        }

        // 设置唤醒事件通道的读回调函数为handleRead 
        wakeupChannel_->setReadCallback(
            boost::bind(&EventLoop::handleRead, this));

        // we are always reading the wakeupfd  
        // 启用读功能 
        wakeupChannel_->enableReading();

}

EventLoop::~EventLoop() {
    std::cout << "EventLoop " << this << " of thread " << threadId_
        << " destructs in thread " << currentthread::tid() << std::endl;

    wakeupChannel_->disableAll();
    wakeupChannel_->remove();
    ::close(wakeupFd_);
    t_loopInThisThread = NULL;
}

// 事件循环，必须在同一个线程内创建Reactor对象和调用它的loop函数 
void EventLoop::loop() {
    assert(!looping_);
    assertInLoopThread();
    looping_ = true;
    quit_ = false;

    std::cout << "EventLoop " << this << " start looping" << std::endl;

    while (!quit_) {
        // 清理已激活事件通道的队列
        activeChannels_.clear();
        // 开始轮询  
        pollReturnTime_ = poller_->poll(kPollTimeMs, &activeChannels_);
        // 记录循环的次数
        ++iteration_;

        //调式
        printActiveChannels();

        eventHandling_ = true;

        // 遍历已激活事件处理器队列，执行每一个事件处理器的处理 
        for (ChannelList::iterator it = activeChannels_.begin();
            it != activeChannels_.end(); ++it) {
            currentActiveChannel_ = *it;

            // 处理事件
            currentActiveChannel_->handleEvent(pollReturnTime_);
        }

        currentActiveChannel_ = NULL;
        eventHandling_ = false;

        // 执行投递回调函数
        doPendingFunctors();
    }

    std::cout << "EventLoop " << this << " stop looping" << std::endl;
    looping_ = false;

}

/* 
 * 让Reactor退出循环 
 * 因为调用quit的线程和执行Reactor循环的线程不一定相同，如果他们不是同一个线程， 
 * 那么必须使用wakeup函数让wakeupChannel_被激活，然后轮询函数返回，就可以处理事件，然后进入下一个循环， 
 * 由于quit_已经被设置为true，所以就会跳出循环 
 */ 

void EventLoop::quit() {
    quit_ = true;

    if (!isInLoopThread()) {
        wakeup();
    }

}

// 运行一个回调函数
void EventLoop::runInLoop(const Functor& cb) {
    if (isInLoopThread()) {
        cb();
    } else {
        // 如果不是同一个线程，那么将他添加到投递回调函数队列中
        queueInLoop(cb);
    }
}

// 把一个回调函数添加到投递回调函数队列中，并唤醒Reactor 
void EventLoop::queueInLoop(const Functor& cb) {
    {
        MutexLockGuard lock(mutex_);
        pendingFunctors_.push_back(cb);
    }

    if (!isInLoopThread() || callingPendingFunctors_) {
        wakeup();
    }

}

size_t EventLoop::queueSize() const {
    MutexLockGuard lock(mutex_);
    return pendingFunctors_.size();
}

// 添加定时器事件：在某个时间点执行
TimerId EventLoop::runAt(const Timestamp& time, const TimerCallback& cb) {
    return timerQueue_->addTimer(cb, time, 0.0);
}

// 在delay秒之后调用回调函数 
TimerId EventLoop::runAfter(double delay, const TimerCallback& cb) {
    Timestamp time(addTime(Timestamp::now(), delay));
    return runAt(time, cb);
}

// 每隔interval妙调用一次回调函数
TimerId EventLoop::runEvery(double interval, const TimerCallback& cb) {
    Timestamp time(addTime(Timestamp::now(), interval));
    return timerQueue_->addTimer(cb, time, interval);
}

// 取消一个计时器
void EventLoop::cancel(TimerId timerId) {
    return timerQueue_->cancel(timerId);
}

// 更新事件通道
void EventLoop::updateChannel(Channel* channel) {
    assert(channel->ownerLoop()  == this);
    assertInLoopThread();
    poller_->updateChannel(channel);
}

// 移除事件通道
void EventLoop::removeChannel(Channel* channel) {
    assert(channel->ownerLoop() == this);
    assertInLoopThread();
    if (eventHandling_) {
        assert(currentActiveChannel_ == channel ||
            std::find(activeChannels_.begin(), activeChannels_.end(), channel) == activeChannels_.end());
    }

    poller_->removeChannel(channel);
}

// 是否有某一个事件处理器 
bool EventLoop::hasChannel(Channel* channel) {
    assert(channel->ownerLoop() == this);
    assertInLoopThread();
    return poller_->hasChannel(channel);
}

// 如果创建Reactor的线程和运行Reactor的线程不同就退出进程  
void EventLoop::abortNotInLoopThread() {
    std::cout << "EventLoop::abortNotInLoopThread - EventLoop " << this 
        << " was created in threadId_ =" << threadId_
        << ", current thread id =" << currentthread::tid() << std::endl;
    abort();
}

/* 
 * 唤醒 
 * 其实就是告诉正在处理循环的Reactor，发生了某一件事 ，这里主动写进一个8字节的整数1，来触发wakeupFd_的写操作
 */
void EventLoop::wakeup() {
    uint64_t one = 1;
    ssize_t n = sockets::write(wakeupFd_, &one, sizeof(one));
    if (n != sizeof(one)) {
        std::cout << "EventLoop::wakeup() writes " << n << " bytes instead of 8" << std::endl;
    }

}


/* 
 * 处理读事件 
 * 被wakeupChannel_使用 
 * wakeupChannel_有事件发生时候（wakeup一旦被调用，wakeupChannel_就被激活），就会执行handleEvent 
 * 而handleEvent内部就会调用处理读的回调函数，wakeupChannel_的读回调函数就是handleRead 
 */
void EventLoop::handleRead() {
    uint64_t one = 1;
    ssize_t n = sockets::read(wakeupFd_, &one, sizeof(one));
    if (n != sizeof(one)) {
        std::cout << "EventLoop::handleRead() reads " << n << " bytes instead of 8" << std::endl;
    }
}

// 执行投递的回调函数（投递的回调函数是在一次循环中，所有的事件都处理完毕之后才调用的） 
void EventLoop::doPendingFunctors() {
    std::vector<Functor> functors;
    callingPendingFunctors_ = true;

    {
        // 交换的目的是让pendingFunctors_继续存放投递的回调函数  
        // 而functors则专门用于执行投递的回调函数  
        // 这样就不会因为长期锁住pendingFunctors_而造成阻塞了  
        MutexLockGuard lock(mutex_);
        functors.swap(pendingFunctors_);
    }

    for (size_t i = 0; i < functors.size(); ++i) {
        functors[i](); //执行投递的回调函数
    }

    callingPendingFunctors_ = false;

}

// 打印已激活的事件通道
void EventLoop::printActiveChannels() const {
    for (ChannelList::const_iterator it = activeChannels_.begin();
        it != activeChannels_.end(); ++it) {
        const Channel* ch = *it;
        std::cout << "{" << ch->reventsToString() << "}" << std::endl; 
    }

}


