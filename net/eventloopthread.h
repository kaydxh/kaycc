#ifndef KAYCC_NET_EVENTLOOPTHREAD_H 
#define KAYCC_NET_EVENTLOOPTHREAD_H

/*
关于EventLoopThread有以下几点：
1.任何一个线程，只要创建并运行了EventLoop，都称之为I/O线程。
2.I/O线程不一定是主线程。I/O线程中可能有I/O线程池和计算线程池。
3.muduo并发模型one loop per thread + threadpool
4.为了方便使用，就直接定义了一个I/O线程的类，就是EventLoopThread类，该类实际上就是对I/O线程的封装。
（1）EventLoopThread创建了一个线程。
（2）在该类线程函数中创建了一个EventLoop对象并调用EventLoop::loop 
*/

#include "../base/condition.h"
#include "../base/mutex.h"
#include "../base/thread.h"

#include <boost/noncopyable.hpp>


namespace kaycc {
namespace net {

    class EventLoop;

    class EventLoopThread : boost::noncopyable {
    public:
        typedef boost::function<void(EventLoop*)> ThreadInitCallback;

        EventLoopThread(const ThreadInitCallback& cb = ThreadInitCallback(), const std::string& name = std::string());
        ~EventLoopThread();

        //启动成员thread_线程，该线程就成了I/O线程，内部调用thread_.start()
        EventLoop* startLoop();

    private:
        //线程运行函数
        void threadFunc();

        //指向一个EventLoop对象，一个I/O线程有且只有一个EventLoop对象 
        EventLoop* loop_;
        // 是否正在退出 
        bool exiting_;

        //基于对象，包含了一个thread类对象 
        Thread thread_;
        MutexLock mutex_;

        // 条件变量，用于exiting_变化的通知
        Condition cond_;

        //回调函数在EventLoop::loop事件循环之前被调用  
        ThreadInitCallback callback_;

    };

} //end net
}

#endif