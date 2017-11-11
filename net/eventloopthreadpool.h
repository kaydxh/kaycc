#ifndef KAYCC_NET_EVENTLOOPTHREADPOOL_H 
#define KAYCC_NET_EVENTLOOPTHREADPOOL_H

#include <vector>
#include <boost/function.hpp>
#include <boost/noncopyable.hpp>
#include <boost/ptr_container/ptr_vector.hpp>

namespace kaycc {
namespace net {

    class EventLoop;
    class EventLoopThread;

    class EventLoopThreadPool : boost::noncopyable {
    public:
        typedef boost::function<void(EventLoop*)> ThreadInitCallback;

        EventLoopThreadPool(EventLoop* baseLoop, const std::string& name);
        ~EventLoopThreadPool();

         // 设置线程池的线程数量 
        void setThreadNum(int numThreads) {
            numThreads_ = numThreads;
        }

        // 启动线程池 
        void start(const ThreadInitCallback& cb = ThreadInitCallback());

        // 获取下一个EventLoop对象
        EventLoop* getNextLoop();

        // 根据一个哈希码返回一个EventLoop对象 
        EventLoop* getLoopForHash(size_t hashCode);

        // 获取所有的EventLoop对象
        std::vector<EventLoop*> getAllLoops();

        // 判断线程池是否已经启动
        bool started() const {
            return started_;
        }

        // 线程池的名字
        const std::string& name() const {
            return name_;
        }

    private:
        //主要的EventLoop对象
        EventLoop* baseLoop_;

        // 线程池的名字
        std::string name_;

         // 线程池是否已经启动 
        bool started_;
        int numThreads_;
        int next_;
        boost::ptr_vector<EventLoopThread> threads_;

        // EventLoop对象列表
        std::vector<EventLoop*> loops_;

    };

} //end net
}

#endif