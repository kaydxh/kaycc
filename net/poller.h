#ifndef KAYCC_NET_POLLER_H 
#define KAYCC_NET_POLLER_H 

#include <map>
#include <vector>
#include <boost/noncopyable.hpp>

#include "../base/timestamp.h"
#include "eventloop.h"
#include "../base/log.h"

//通常一个Reactor对应一个轮询器，轮询器用于等待事件的发生

namespace kaycc {
namespace net {

    class Channel;

    //轮询器基类 
    class Poller : boost::noncopyable {
    public:
        typedef std::vector<Channel*> ChannelList;

        Poller(EventLoop* loop);
        virtual ~Poller();

        /// Must be called in the loop thread.  
        // 轮询，通常是调用select、poll、epoll_wait等函数  
        virtual Timestamp poll(int timeoutMs, ChannelList* activeChannels) = 0;

        /// Must be called in the loop thread.  
        // 更新事件处理器（通常是要处理的事件发生改变时调用）
        virtual void updateChannel(Channel* channel) = 0;

        /// Must be called in the loop thread.  
        // 移除事件处理器 
        virtual void removeChannel(Channel* channel) = 0;

        // 判断是否有某个事件处理器 
        virtual bool hasChannel(Channel* channel) const;

        // 创建一个默认的轮询器 
        static Poller* newDefaultPoller(EventLoop* loop);

        void assertInLoopThread() const {
            ownerLoop_->assertInLoopThread();
        }

    protected:
        // 文件描述符和事件处理器的映射  
        typedef std::map<int, Channel*> ChannelMap;
        ChannelMap channels_;

    private:
        // 所属的Reactor 
        EventLoop* ownerLoop_;

    };

}
}

#endif