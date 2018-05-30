#ifndef KAYCC_BASE_THREADPOOL_H
#define KAYCC_BASE_THREADPOOL_H

#include "condition.h"
#include "mutex.h"
#include "thread.h"

#include <boost/function.hpp>
#include <boost/noncopyable.hpp>
#include <boost/ptr_container/ptr_vector.hpp>

#include <deque>

namespace kaycc {
    class ThreadPool : boost::noncopyable {
    public:
        typedef boost::function<void ()> TaskFunc;

        explicit ThreadPool(const std::string& name = "ThreadPool");
        ~ThreadPool();

        void setMaxQueueSize(int maxSize) { maxQueueSize_ = maxSize; }
        void setThreadInitCallback(const TaskFunc& cb) {
            threadInitCallback_ = cb;
        }

        void start(int numThreads);
        void stop();

        const std::string& name() const {
            return name_;
        } 

        size_t queueSize() const;

        void run(const TaskFunc& f);
    #if __cplusplus >= 201103L
        void run(TaskFunc&& f);
    #endif

    private:
        bool isFull() const;
        void runInThread();
        TaskFunc takeTask();

    private:
        mutable MutexLock mutex_;
        Condition notEmpty_; //条件变量，队列没有空，就通知拿任务
        Condition notFull_; //条件变量，队列没雨满，就通知可以放入任务

        std::string name_;

        TaskFunc threadInitCallback_; //线程回调函数执行前的初始化回调函数
        boost::ptr_vector<kaycc::Thread> threads_;
        std::deque<TaskFunc> queue_;

        size_t maxQueueSize_;
        bool running_;

    };
}

#endif