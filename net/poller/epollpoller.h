#ifndef KAYCC_NET_EPOLLPOLLER_H 
#define KAYCC_NET_EPOLLPOLLER_H 

#include "../poller.h"

#include <vector>

struct epoll_event;

namespace kaycc {
namespace net {

    class EPollPoller : public Poller {
    public:
        EPollPoller(EventLoop* loop);
        virtual ~EPollPoller();

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
        static const int kInitEventListSize = 16;

        static const char* operationToString(int op);

        void fillActiveChannels(int numEvents, ChannelList* activeChannels) const;

        void update(int operation, Channel* channel);

        typedef std::vector<struct epoll_event> EventList;

        int epollfd_;
        EventList events_;
            //struct epoll_event {
            //   __uint32_t   events;      /* Epoll events */
            //   epoll_data_t data;        /* User data variable */
           //};

            /*
            typedef union epoll_data {
               void    *ptr;
               int      fd;
               uint32_t u32;
               uint64_t u64;
           } epoll_data_t;
           */

    };

}
}

#endif