#ifndef KAYCC_NET_CHANNEL_H 
#define KAYCC_NET_CHANNEL_H 

#include <boost/function.hpp>
#include <boost/noncopyable.hpp>
#include <boost/share_ptr.hpp>
#include <boost/weak_ptr.hpp>

#include "../base/timestamp.h"

//Channel类，即通道类
//负责注册读写事件的类，并保存了fd读写事件发生时调用的回调函数，如果poll/epoll有读写事件发生则将这些事件添加到对应的通道中。
//一个通道对应唯一EventLoop，一个EventLoop可以有多个通道
//Channel类不负责fd的生存期，fd的生存期是有socket决定的，断开连接关闭描述符。
//当有fd返回读写事件时，调用提前注册的回调函数处理读写事件

namespace kaycc {
namespace net {

    class EventLoop;

    class Channel : boost::noncopyable {
    public:
        typedef boost::function<void()> EventCallback;
        typedef boost::function<void(Timestamp)> ReadEventCallback;

        Channel(EventLoop* loop, int fd);
        ~Channel();

        void handleEvent(Timestamp receiveTime);
        void setReadCallback(const ReadEventCallback& cb) {
            readCallback_ = cb;
        }

        void setWriteCallback(const EventCallback& cb) {
            writeCallback_ = cb;
        }

        void setCloseCallback(const EventCallback& cb) {
            closeCallback_ = cb;
        }

        void setErrorCallback(const EventCallback& cb) {
            errorCallback_ = cb;
        }

        /// Tie this channel to the owner object managed by shared_ptr,
        /// prevent the owner object being destroyed in handleEvent.
        // 把当前事件处理器依附到某一个对象上
        void tie(const boost::share_ptr<void>&);

        int fd() const {
            return fd_;
        }

        int events() const {
            return events_;
        }

        void set_revents(int revt) {
            revents_ = revt;
        } 

        bool isNoneEvent() const {
            return events_ == kNoneEvent;
        }

        void enableReading() {
            events_ |= kReadEvent;
            update();
        }

        void disableReading() {
            events_ &= ~kReadEvent;
            update();
        }

        void enableWriting() {
            events_ |= kWriteEvent;
            update();
        }

        void disableWriting() {
            events_ &= ~kWriteEvent;
            update();
        }

        void disableAll() {
            events_ = kNoneEvent;
            update();
        }

        bool isWriting() const {
            return events_ & kWriteEvent;
        }

        bool isReading() const {
            return events_ & kReadEvent;
        }

        int index() {
            return index_;
        }

        void set_index(int idx) {
            index_ = idx;
        }

        std::string reventsToString() const;
        std::string eventsToString() const;

        // 所属的事件循环 
        EventLoop* ownerloop() {
            return loop_;
        }

        // 从事件循环对象中把自己(channel, 事件处理器)删除 
        void remove();

    private:
        static std::string eventsToString(int fd, int ev);

        void update();
        void handleEventWithGuard(Timestamp receiveTime);

        static const int kNoneEvent;
        static const int kReadEvent;
        static const int kWriteEvent;

        const int fd_;
        int events_; //关心的事件

        //目前活动的事件，it's the received event types of epoll or poll
        int revents_; 
        int index_;  //索引，used by Poller.

        boost::weak_ptr<void> tie_;//用于把自己依附到某一个对象上
        bool tied_;
 
        bool eventHandling_;  // 是否正在处理事件 
        bool addedToLoop_; //是否已经被添加到事件循环中 

        ReadEventCallback readCallback_;
        EventCallback writeCallback_;
        EventCallback closeCallback_;
        EventCallback errorCallback_;

    };

} //end net
}

#endif