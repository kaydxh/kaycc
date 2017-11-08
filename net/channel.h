#ifndef KAYCC_NET_CHANNEL_H 
#define KAYCC_NET_CHANNEL_H 

#include <boost/function.hpp>
#include <boost/noncopyable.hpp>
#include <boost/shared_ptr.hpp>
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
        void tie(const boost::shared_ptr<void>&);

        int fd() const {
            return fd_;
        }

        //注册的事件
        int events() const {
            return events_;
        }

        void set_revents(int revt) { // used by pollers，发生的事件
            revents_ = revt;
        } 

        //判断是否无关注事件类型，events为0 
        bool isNoneEvent() const {
            return events_ == kNoneEvent;
        }

        //关注可读事件，注册到EventLoop，通过它注册到Poller中  
        void enableReading() {
            events_ |= kReadEvent;
            update();
        }

        //取消读关注 
        void disableReading() {
            events_ &= ~kReadEvent;
            update();
        }

        //关注写事件
        void enableWriting() {
            events_ |= kWriteEvent;
            update();
        }

        //取消写关注 
        void disableWriting() {
            events_ &= ~kWriteEvent;
            update();
        }

        //全部关闭 
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

        // for Poller pollfds_数组中的下标
        int index() {
            return index_;
        }

        void set_index(int idx) {
            index_ = idx;
        }

        std::string reventsToString() const; //事件转化为字符串，方便打印调试 
        std::string eventsToString() const; //事件转化为字符串，方便打印调试 

        // 所属的事件循环 
        EventLoop* ownerLoop() {
            return loop_;
        }

        // 从事件循环对象中把自己(channel, 事件通道)删除 
        void remove();

    private:
        static std::string eventsToString(int fd, int ev);

        void update();
        void handleEventWithGuard(Timestamp receiveTime);

        static const int kNoneEvent;
        static const int kReadEvent;
        static const int kWriteEvent;

        EventLoop* loop_;
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