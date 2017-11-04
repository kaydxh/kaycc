#ifndef KAYCC_NET_POLLPOLLER_H 
#define KAYCC_NET_POLLPOLLER_H 

#include "../poller.h"

struct pollfd;

namespace kaycc {
namespace net {

    class PollPoller : public Poller {
    public:
        PollPoller(EventLoop* loop);
        virtual ~PollPoller();

        /// Must be called in the loop thread.  
        // 轮询，通常是调用select、poll、epoll_wait等函数  
        virtual Timestamp poll(int timeoutMs, ChannelList* activeChannels);

        /// Must be called in the loop thread.  
        // 更新事件处理器（通常是要处理的事件发生改变时调用）
        virtual void updateChannel(Channel* channel);

        /// Must be called in the loop thread.  
        // 移除事件处理器 
        virtual void removeChannel(Channel* channel);

    private:
        void fillActiveChannels(int numEvents, ChannelList* activeChannels) const;

        typedef std::vector<struct pollfd> PollFdList;
        PollFdList pollfds_;

    };

} //end net
}

#endif