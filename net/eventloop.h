#ifndef KAYCC_NET_EVENTLOOP_H 
#define KAYCC_NET_EVENTLOOP_H 

#include <vector>
#include <boost/any.hpp>
#include <boost/function.hpp>
#include <boost/noncopyable.hpp>
#include <boost/scoped_ptr.hpp>

#include "../base/mutex.h"
#include "../base/current_thread.h"
#include "../base/timestamp.h"

#include "callbacks.h"
#include "timerid.h"

/*Reactor是整个框架最核心的部分
它创建轮询器（Poller），创建用于传递消息的管道，初始化各个部分，然后进入一个无限循环，每一次循环中调用轮询器的轮询函数（等待函数），等待事件发生，如果有事件发生，就依次调用套接字（或其他的文件描述符）的事件处理器处理各个事件，然后调用投递的回调函数；接着进入下一次循环。
*/

namespace kaycc {
namespace net {

    class Channel;
    class Poller;
    class TimerQueue;

    class EventLoop : boost::noncopyable {
    public:
        typedef boost::function<void()> Functor;

        EventLoop();
        ~EventLoop();

        // 事件循环，必须在同一个线程内创建Reactor对象和调用它的loop函数 
        void loop();

        void quit();

        Timestamp pollReturnTime() const {
            return pollReturnTime_;
        }

        int64_t iteration() const {
            return iteration_;
        }

        /// Runs callback immediately in the loop thread.  
        /// It wakes up the loop, and run the cb.  
        /// If in the same loop thread, cb is run within the function.  
        /// Safe to call from other threads.  
        /* 
        * 在事件循环中周期性的调用回调函数 
        */  
        void runInLoop(const Functor& cb);

        /// Queues callback in the loop thread.  
        /// Runs after finish pooling.  
        /// Safe to call from other threads.  
        // 把回调函数放在队列中，在轮询结束之后调用 
        void queueInLoop(const Functor& cb);

        size_t queueSize() const;

    #if _cplusplus >= 201103L
        void runInLoop(const Functor&& cb);
        void queueInLoop(const Functor&& cb);
    #endif

        // 在指定的时间调用回调函数
        TimerId runAt(const Timestamp& time, const TimerCallback& cb);

        // 在delay秒之后调用回调函数 
        TimerId runAfter(double delay, const TimerCallback& cb);

        // 每隔interval秒调用一次回调函数
        TimerId runEvery(double interval, const TimerCallback& cb);

    #if _cplusplus >= 201103L
        TimerId runAt(const Timestamp& time, const TimerCallback&& cb);
        TimerId runAfter(double delay, const TimerCallback&& cb);
        TimerId runEvery(double interval, const TimerCallback&& cb);
    #endif

        // 取消一个计时器
        void cancel(TimerId timerId);

        // internal usage 内部使用
        void wakeup();

        // 更新事件通道
        void updateChannel(Channel* channel);

        // 移除事件通道
        void removeChannel(Channel* channel);

        // 是否有某一个事件处理器 
        bool hasChannel(Channel* channel);

        // 断言自己是否在循环线程中 
        void assertInLoopThread() {
            if (!isInLoopThread()) {
                abortNotInLoopThread();
            }
        }

        // 是否在循环线程中 
        bool isInLoopThread() const {
            return threadId_ == currentthread::tid();
        }

        // 是否正在处理事件 
        bool eventHandling() const {
            return eventHandling_;
        }

        // 设置上下文
        void setContext(const boost::any& context) {
            context_ = context;
        }

        // 返回上下文
        const boost::any& getContext() const {
            return context_;
        }

        // 返回可修改的上下文
        boost::any* getMutableContext() {
            return &context_;
        }

        // 返回当前线程的Reactor对象
        static EventLoop* getEventLoopOfCurrentThread();

    private:
        // 如果创建Reactor的线程和运行Reactor的线程不同就退出进程  
        void abortNotInLoopThread();

        // 处理读事件 
        void handleRead(); // waked up

        // 执行投递的回调函数 
        void doPendingFunctors();

        void printActiveChannels() const; // DEBUG

        typedef std::vector<Channel*> ChannelList;

        // 是否正在循环中 
        bool looping_; /* atomic */
        // 是否退出循环
        bool quit_; /* atomic and shared between threads, okay on x86, I guess. */

        // 是否正在处理事件
        bool eventHandling_;  /* atomic */

        // 是否正在调用投递的回调函数
        bool callingPendingFunctors_; /* atomic */

        // 迭代器 
        int64_t iteration_;

        // 线程id 
        const pid_t threadId_;

         // 轮询返回的时间
        Timestamp pollReturnTime_;

        // 轮询器 
        boost::scoped_ptr<Poller> poller_;

        // 定时器队列 
        boost::scoped_ptr<TimerQueue> timerQueue_;

        // 用于唤醒的描述符（将Reactor从等待中唤醒，一般是由于调用轮询器的等待函数而造成的阻塞）
        int wakeupFd_;

        // unlike in TimerQueue, which is an internal class,  
        // we don't expose Channel to client.  
        // 唤醒事件的通道
        boost::scoped_ptr<Channel> wakeupChannel_;

        // 上下文
        boost::any context_; //保存任意类型

        // scratch variables  
        // 已激活的事件通道的队列 
        ChannelList activeChannels_;

        // 当前正在调用的事件通道
        Channel* currentActiveChannel_;

        mutable MutexLock mutex_;
        // 投递的回调函数列表
        std::vector<Functor> pendingFunctors_; // @GuardedBy mutex_

    };

}
}

#endif


