#include "eventloopthread.h"

#include "eventloop.h"

#include <boost/bind.hpp>

using namespace kaycc;
using namespace kaycc::net;

EventLoopThread::EventLoopThread(const ThreadInitCallback& cb, const std::string& name)
    : loop_(NULL), 
      exiting_(false),
      thread_(boost::bind(&EventLoopThread::threadFunc, this), name), //绑定线程运行函数  
      mutex_(),
      cond_(mutex_),
      callback_(cb) {

}

EventLoopThread::~EventLoopThread() {
    exiting_ = true;

    // not 100% race-free, eg. threadFunc could be running callback_.  
    // still a tiny chance to call destructed object, if threadFunc exits just now.  
    // but when EventLoopThread destructs, usually programming is exiting anyway.
    if (loop_ != NULL) {
        loop_->quit(); //退出I/O线程，让I/O线程的loop循环退出，从而退出了I/O线程  
        thread_.join();
    }
}

//启动成员thread_线程，该线程就成了I/O线程，内部调用thread_.start()
EventLoop* EventLoopThread::startLoop() {
    assert(!thread_.started());
    thread_.start();

    {
        // 等待线程启动完毕 
        MutexLockGuard lock(mutex_);
        while (loop_ == NULL) {
            cond_.wait();
        }
    }

    return loop_;
}

// 线程函数：用于执行EVentLoop的循环
void EventLoopThread::threadFunc() {
    EventLoop loop;

    // 如果有初始化函数，就先调用初始化函数 
    if (callback_) {
        callback_(&loop);
    }

    {
        MutexLockGuard lock(mutex_);
        loop_ = &loop;

        // 通知startLoop线程已经启动完毕 
        cond_.notify();
    }

    // 事件循环
    loop.loop();
    loop_ = NULL;

}