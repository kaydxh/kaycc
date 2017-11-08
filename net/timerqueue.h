#ifndef KAYCC_NET_TIMERQUEUE_H 
#define KAYCC_NET_TIMERQUEUE_H

#include <set>
#include <vector>
#include <boost/noncopyable.hpp>

#include "../base/timestamp.h"
#include "callbacks.h"
#include "channel.h"

namespace kaycc {
namespace net {
    class EventLoop;
    class Timer;
    class TimerId;

    class TimerQueue : boost::noncopyable {
    public:
        explicit TimerQueue(EventLoop* loop);
        ~TimerQueue();

        /// Must be thread safe. Usually be called from other threads.  
        // 添加一个定时器 
        TimerId addTimer(const TimerCallback& cb, Timestamp when, double interval);

        // 取消一个定时器  
        void cancel(TimerId timerId);

    private:

        typedef std::pair<Timestamp, Timer*> Entry; //计数器的实体类型，key-value，key为时间戳，value为计时器的指针
        typedef std::set<Entry> TimerList; //计时器的列表
        typedef std::pair<Timer*, int64_t> ActiveTimer; //活动的计时器, key为Timer*， value为sequence
        typedef std::set<ActiveTimer> ActiveTimerSet; //活动的计时器集合, 保存的目前的有效的Timer指针

        void addTimerInLoop(Timer* timer); //添加计时器
        void cancelInLoop(TimerId timerId); //取消计时器

        void handleRead();

        std::vector<Entry> getExpired(Timestamp now); //获取所有超时时间比当前时间早的定时器
        void reset(const std::vector<Entry>& expired, Timestamp now); // 重置所有周期性的定时器 

        bool insert(Timer* timer);

        // 所属的Reactor
        EventLoop* loop_;
        const int timerfd_; //定时器文件描述符（Reactor用这个文件描述符产生的事件激活定时器事件处理器）

        // 定时器事件通道
        Channel timerfdChannel_;

        TimerList timers_; //活动定时器列表,与activeTimers_存放的timer一致，只是，一个以Timestamp作为key，一个以Timer*作为key

        ActiveTimerSet activeTimers_;   //活动的定时器集合，保存的目前的有效的Timer指针
        bool callingExpiredTimers_; // 是否正在处理超时任务
        ActiveTimerSet cancelingTimers_; // 被取消的定时器的集合 

    };

}
} 

#endif