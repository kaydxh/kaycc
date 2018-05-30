#include "threadpool.h"

#include <boost/bind.hpp>
#include <assert.h>
#include <stdio.h>
#include "log.h"

using namespace kaycc;

ThreadPool::ThreadPool(const std::string& name)
    : mutex_(),
      notEmpty_(mutex_), 
      notFull_(mutex_),
      name_(name),
      maxQueueSize_(0),
      running_(false) {

}

ThreadPool::~ThreadPool() {
    if (running_) {
        stop();
    }
}

void ThreadPool::start(int numThreads) {
    assert(threads_.empty());
    running_ = true;
    threads_.reserve(numThreads);

    //numThreads个线程可以看作numThreads个消费者
    for (int i = 0; i < numThreads; ++i) {
        char id[32];
        snprintf(id, sizeof(id), "%d", i+1);

        //runInThread是线程池里的线程回调函数，可看作线程处理函数，该回调在线处理函数中调用
        threads_.push_back(new kaycc::Thread(
            boost::bind(&ThreadPool::runInThread, this), name_ + id));
        threads_[i].start();
    }

    if (numThreads == 0 && threadInitCallback_) { //如果线程池里的线程为0，若设置了threadInitCallback_，就调用threadInitCallback_
        threadInitCallback_();
    }
}

//关闭线程池
void ThreadPool::stop() {
    {
        MutexLockGuard lock(mutex_);
        running_ = false; //running_置为false
        notEmpty_.notifyAll();//通知takeTask，如果此时有线程等待，就从队列里取走一个任务，执行完毕后，退出线程
    }

    //等待线程退出
    for_each(threads_.begin(), threads_.end(),
        boost::bind(&kaycc::Thread::join, _1)); //_1为占位，这里调用时传kayc::Thread对象
}

size_t ThreadPool::queueSize() const {
    MutexLockGuard lock(mutex_);
    return queue_.size();
}

void ThreadPool::run(const TaskFunc& task) {
    if (threads_.empty()) { //如果线程池没有线程，那么直接执行任务，也就是说假设没有消费者，那么生产者直接消费产品. 
        task();             //而不把任务加入任务队列
    } else {//如果线程池有线程,加锁，如果队列已满，则挂起等待
        MutexLockGuard lock(mutex_);
        while (isFull()) { 
            notFull_.wait();
        }

        //wait返回后，队列就不是满的，此时在把任务加入队列，并通知notEmpty_，可以取任务执行了
        assert(!isFull()); 
        queue_.push_back(task);
        notEmpty_.notify();
    }

}

#if __cplusplus >= 201103L
void ThreadPool::run(TaskFunc&& task) {
    if (threads_.empty()) {
        task();
    } else {
        MutexLockGuard lock(mutex_);
        while (isFull()) {
            notFull_.wait();
        }

        assert(!isFull());
        queue_.push_back(std::move(task));
        notEmpty_.notify();
    }
}
#endif

ThreadPool::TaskFunc ThreadPool::takeTask() {
    MutexLockGuard lock(mutex_);

    while (queue_.empty() && running_) { //使用while防止惊群效应，如在多处理器系统中，pthread_cond_signal 可能会唤醒多个等待条件的线程，这也是一种spurious wakeup。
        notEmpty_.wait(); //如果队列为空，就挂起等待
    }

    TaskFunc task;
    if (!queue_.empty()) {
        task = queue_.front();
        queue_.pop_front();

        if (maxQueueSize_ > 0) { //如果队列的最大容量大于0，notFull就通知，如果有新的任务，就加入队列
            notFull_.notify();
        }
    }

    return task;
}

bool ThreadPool::isFull() const {
    mutex_.assertLockByThisThread(); //锁已经被当前线程（调用ThreadPool的线程）锁住程锁住
    return maxQueueSize_ > 0 && queue_.size() >= maxQueueSize_;
}

void ThreadPool::runInThread() {
    try {

        if (threadInitCallback_) {
            threadInitCallback_();
        }

        while (running_) {
            TaskFunc task(takeTask());
            if (task) { //执行任务，消费产品
                task();
            }
        }
    } catch (std::exception& ex) {
        LOG << "exception caught in ThreadPool " << name_ << " ,reason: " << ex.what() << std::endl;
        abort();
    } catch (...) {
        LOG << "unknown exception caught in ThreadPool " << name_ << std::endl;
        throw;
    }

}