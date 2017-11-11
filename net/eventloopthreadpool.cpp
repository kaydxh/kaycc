#include "eventloopthreadpool.h"

#include "../base/types.h"
#include "eventloop.h"
#include "eventloopthread.h"


#include <boost/bind.hpp>

#include <stdio.h>

using namespace kaycc;
using namespace kaycc::net;

EventLoopThreadPool::EventLoopThreadPool(EventLoop* baseLoop, const std::string& name)
    : baseLoop_(baseLoop),
      name_(name),
      started_(false),
      numThreads_(0),
      next_(0) {

}

EventLoopThreadPool::~EventLoopThreadPool() {
     // Don't delete loop, it's stack variable
}

// 启动线程池 
void EventLoopThreadPool::start(const ThreadInitCallback& cb) {
    assert(!started_);
    baseLoop_->assertInLoopThread();

    started_ = true;

    // 创建指定数量的线程，并启动 
    for (int i = 0; i < numThreads_; ++i) {
        char buf[name_.size() + 32];
        snprintf(buf, sizeof(buf), "%s%d", name_.c_str(), i);

        EventLoopThread* t = new EventLoopThread(cb, buf); 
        threads_.push_back(t);
        loops_.push_back(t->startLoop());
    }

    //如果指定线程数为0，那么就使用baseLoop_
    if (numThreads_ == 0 && cb) {
        cb(baseLoop_);
    }

}

// 获取下一个EventLoop对象
EventLoop* EventLoopThreadPool::getNextLoop() {
    baseLoop_->assertInLoopThread();
    assert(started_);

    EventLoop* loop = baseLoop_;
    if (!loops_.empty()) {
        // round-robin, 这里改为loops[next_ % loops.size()]不太好，因为这里需要将next_归0, 参考getLoopForHash实现   
        loop = loops_[next_];
        ++next_;

        if (implicit_cast<size_t>(next_) >= loops_.size()) {
            next_ = 0;
        }
    }

    return loop;
}

// 根据一个哈希码返回一个EventLoop对象 
EventLoop* EventLoopThreadPool::getLoopForHash(size_t hashCode) {
    baseLoop_->assertInLoopThread();
    EventLoop* loop = baseLoop_;

    if (!loops_.empty()) {
        loop = loops_[hashCode % loops_.size()];
    }

    return loop;
}

// 获取所有的EventLoop对象
std::vector<EventLoop*> EventLoopThreadPool::getAllLoops() {
    baseLoop_->assertInLoopThread();
    assert(started_);

    if (loops_.empty()) {
        return std::vector<EventLoop*>(1, baseLoop_);
    } else {
        return loops_;
    }

}