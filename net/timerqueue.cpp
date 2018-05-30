#include "timerqueue.h"

#include "eventloop.h"
#include "timer.h"
#include "timerid.h"
#include "../base/log.h"

#include <sys/timerfd.h>
#include <stdint.h>

#include <boost/bind.hpp>

namespace kaycc {
namespace net {
namespace detail {

    //timerfd_create函数主要用于生成一个定时器对象，返回与之关联的文件描述符，clockid可以设置CLOCK_REALTIME和CLOCK_MONOTONIC，flags可以设置为TFD_NONBLOCK（非阻塞），TFD_CLOEXEC（同O_CLOEXEC）

    //CLOCK_REALTIME：相对时间，从1970.1.1到目前的时间。更改系统时间会更改获取的值。也就是，它以系统时间为坐标。
    //CLOCK_MONOTONIC：与CLOCK_REALTIME相反，它是以绝对时间为准，获取的时间为系统重启到现在的时间，更改系统时间对齐没有影响。
    int createTimerfd() {
        int timerfd = ::timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK | TFD_CLOEXEC);
        if (timerfd < 0) {
            LOG << "Failed in timerfd_create" << std::endl;
        }

        return timerfd;
    }

    // 从现在到某一个指定的时间点的时间长度 
    struct timespec howMuchTimeFromNow(Timestamp when) {
        int64_t microseconds = when.microSecondsSinceEpoch() -  Timestamp::now().microSecondsSinceEpoch();
        if (microseconds < 100) {
            microseconds = 100;
        }

        struct  timespec ts;
        ts.tv_sec = static_cast<time_t>(microseconds / Timestamp::kMicroSecondsPerSecond);
        ts.tv_nsec = static_cast<long>((microseconds % Timestamp::kMicroSecondsPerSecond) * 1000); 

        return ts;
    }

    void readTimerfd(int timerfd, Timestamp now) {
        uint64_t howmany;

        ssize_t n = ::read(timerfd, &howmany, sizeof(howmany));
        LOG << "TimerQueue::handleRead() " << howmany << " at " << now.toString() << std::endl;

        if (n != sizeof(howmany)) {
            LOG << "TimerQueue::handleRead() reads " << n << " bytes instead of 8" << std::endl;
        }
    }
    
    //    struct timespec {
    //        time_t tv_sec;                /* Seconds */
    //        long   tv_nsec;               /* Nanoseconds */
    //    };

    //    struct itimerspec {
    //        struct timespec it_interval;  /* Interval for periodic timer */
    //        struct timespec it_value;     /* Initial expiration */
    //    };

    void resetTimerfd(int timerfd, Timestamp expiration) {
        struct itimerspec newValue;
        struct itimerspec oldValue;
        bzero(&newValue, sizeof(newValue));
        bzero(&oldValue, sizeof(oldValue));

        newValue.it_value = howMuchTimeFromNow(expiration);
        int ret = ::timerfd_settime(timerfd, 0, &newValue, &oldValue);
        if (ret) {
            LOG << "timerfd_settime()" << std::endl;
        }

    }


} //end detail
} //end net
} //end kaycc

using namespace kaycc;
using namespace kaycc::net;
using namespace kaycc::net::detail;

TimerQueue::TimerQueue(EventLoop* loop)
    : loop_(loop),
      timerfd_(createTimerfd()),
      timerfdChannel_(loop, timerfd_),
      timers_(),
      callingExpiredTimers_(false) {

    // 设置超时的回调函数,处理读事件
    timerfdChannel_.setReadCallback(
        boost::bind(&TimerQueue::handleRead, this));

     // 设置为可读  
    timerfdChannel_.enableReading();

}

/* 
 * 销毁定时器队列 
 */ 
TimerQueue::~TimerQueue() {
    timerfdChannel_.disableAll();
    timerfdChannel_.remove(); //从当前eventloop中移除
    ::close(timerfd_);

    for (TimerList::iterator it = timers_.begin(); 
        it != timers_.end(); ++it) {
        delete it->second;
    }
}

// 添加一个定时器 
TimerId TimerQueue::addTimer(const TimerCallback& cb, Timestamp when, double interval) {
    Timer* timer = new Timer(cb, when, interval);
    loop_->runInLoop(
        boost::bind(&TimerQueue::addTimerInLoop, this, timer));

    return TimerId(timer, timer->sequence());
}

#if __cpluscplus >= 201103L
 TimerId TimerQueue::addTimer(const TimerCallback&& cb, Timestamp when, double interval) {
    Timer* timer = new Timer(std::move(cb), when, interval);
    loop_->runInLoop(
        boost::bind(&TimerQueue::addTimerInLoop, this, timer));

    return TimerId(timer, timer->sequence());
}
#endif

void TimerQueue::cancel(TimerId timerId) {
    loop_->runInLoop(
        boost::bind(&TimerQueue::cancelInLoop, this, timerId));
}

// 添加定时器（在Reactor的循环中添加）
void TimerQueue::addTimerInLoop(Timer* timer) {
    loop_->assertInLoopThread();
    bool earliestChanged = insert(timer);
    if (earliestChanged) { //如果该计时器是最早超时的那个，需要重新设置系统定时器的超时事件
        // 重新设置系统定时器的超时时间  
        resetTimerfd(timerfd_, timer->expiration());
    }

}

void TimerQueue::cancelInLoop(TimerId timerId) {
    loop_->assertInLoopThread();
    assert(timers_.size() == activeTimers_.size());
    ActiveTimer timer(timerId.timer_, timerId.sequence_);

    ActiveTimerSet::iterator it = activeTimers_.find(timer);
    if (it != activeTimers_.end()) { //如果活动的计时器队列里有该计数器，就删除
        size_t n = timers_.erase(Entry(it->first->expiration(), it->first));
        assert(n == 1); (void)n;
        delete it->first;
        activeTimers_.erase(it);

    } else if (callingExpiredTimers_) {
        // 如果没有这个定时器（表示它已经超时，正在被处理），而且又是正在处理已超时定时器，那么把它插入到正在取消的定时器对列中  
        // 等待处理超时定时器的工作完成，调用reset的时候会将其删除  
        cancelingTimers_.insert(timer);
    }

    assert(timers_.size() == activeTimers_.size());

}

void TimerQueue::handleRead() {
    loop_->assertInLoopThread();
    Timestamp now(Timestamp::now());
    // 将计时器里数据（这个数据是通过timerfd_settime写入的）读取出来，否则会重复激发定时器  
    readTimerfd(timerfd_, now);

    std::vector<Entry> expired = getExpired(now);

    callingExpiredTimers_ = true;
    cancelingTimers_.clear();

    // 一次调用每一个到期回调函数 
    for (std::vector<Entry>::iterator it = expired.begin();
        it != expired.end(); ++it) {
        it->second->run();
    }
    callingExpiredTimers_ = false;

    // 重置所有周期性的定时器, 不然就是一次性定时
    reset(expired, now);
}

std::vector<TimerQueue::Entry> TimerQueue::getExpired(Timestamp now) {
    assert(timers_.size() == activeTimers_.size());
    std::vector<Entry> expired;
    #define UINTPTR_MAX (4294967295U)
    Entry sentry(now, reinterpret_cast<Timer*>(UINTPTR_MAX));//UINTPTR_MAX, Maximum value of uintptr_t, 2^32-1, or higher
    // 获取所有超时时间比当前时间早的定时器，即已到期的定时器（timers_.begin()与end之间就是所有的已超时的定时器）
    TimerList::iterator end = timers_.lower_bound(sentry); //lower_bound返回第一个不小于sentry的位置
    assert(end == timers_.end() || now < end->first);

    // 将已超时的定时器复制到expired中
    std::copy(timers_.begin(), end, std::back_inserter(expired));
    timers_.erase(timers_.begin(), end);

    for (std::vector<Entry>::iterator it = expired.begin(); it != expired.end(); ++it) {
        ActiveTimer timer(it->second, it->second->sequence());
        size_t n = activeTimers_.erase(timer); // 将已超时的定时器从活动定时器列表中删除 
        assert(n == 1); (void)n;
    }

    assert(timers_.size() == activeTimers_.size());

    return expired;
}

// 重置所有周期性的定时器 
void  TimerQueue::reset(const std::vector<Entry>& expired, Timestamp now) {
    // 下一次的超时时间
    Timestamp nextExpire;
    for (std::vector<Entry>::const_iterator it = expired.begin(); it != expired.end(); ++it) {
        ActiveTimer timer(it->second, it->second->sequence());

        // 它是周期性的定时器，而且不在被取消计时器队列中 
        if (it->second->repeat()
            && cancelingTimers_.find(timer) == cancelingTimers_.end()) {

            // 重新设置超时时间, 此时restart()内部会更新timer的expiration_
            it->second->restart(now);
            // 再次插入计时器队列中
            insert(it->second);
        } else {
            // 一次性的定时器，或者在被取消的定时器队列中，那么将它删除 
            delete it->second;
        }
    }

    if (!timers_.empty()) {
        nextExpire = timers_.begin()->second->expiration();
    }

    // 重新设置系统计时器的超时时间  
    if (nextExpire.valid()) {
        resetTimerfd(timerfd_, nextExpire);
    }

}

bool TimerQueue::insert(Timer* timer) {
    loop_->assertInLoopThread();
    assert(timers_.size() == activeTimers_.size());

    bool earliestChanged = false;
    Timestamp when = timer->expiration();

    // 如果计时器队列是空的或者它比计时器队列中最早超时的那个计时器的超时时间还要早  
    // 那么当前计时器就是最早发生超时的那个计时器 
    TimerList::iterator it = timers_.begin();
    if  (it == timers_.end() || when < it->first) {
        earliestChanged = true;
    }

    {
        std::pair<TimerList::iterator, bool> result = timers_.insert(Entry(when, timer)); //插入成功，result->second 为true，如果眼睛存在，为false
        assert(result.second); (void) result;
    }

    {
        // 插入到活动的计时器队列
        std::pair<ActiveTimerSet::iterator, bool> result = activeTimers_.insert(ActiveTimer(timer, timer->sequence()));
        assert(result.second); (void) result;
    }

    assert(timers_.size() == activeTimers_.size());
    return earliestChanged;
}