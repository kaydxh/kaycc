#ifndef KAYCC_NET_CALLBACKS_H 
#define KAYCC_NET_CALLBACKS_H 

#include <boost/function.hpp>
#include <boost/shared_ptr.hpp>

#include "../base/timestamp.h"

namespace kaycc {
namespace net {
    class Buffer;
    class TcpConnection;

    // 定义TcpConnection的指针类型
    typedef boost::shared_ptr<TcpConnection> TcpConnectionPtr;

    // 计时器回调函数的定义 
    typedef boost::function<void()> TimerCallback;

    // 连接完成回调函数的定义
    typedef boost::function<void (const TcpConnectionPtr&)> ConnectionCallback;

    // 关闭一个连接的回调函数
    typedef boost::function<void (const TcpConnectionPtr&)> CloseCallback;

    // 写完成回调函数的定义 
    typedef boost::function<void (const TcpConnectionPtr&)> WriteCompleteCallback;

    // 高水位标记回调函数的定义
    typedef boost::function<void (const TcpConnectionPtr&, size_t)> HighWaterMarkCallback;

    // the data has been read to (buf, len)  
    // 当数据到来的时候的回调函数，此时数据已经被存放在buffer中 
    typedef boost::function<void (const TcpConnectionPtr&,
                            Buffer*,
                            Timestamp)> MessageCallback;

    // 默认的连接完成回调函数
    void defaultConnectionCallback(const TcpConnectionPtr& conn);

    // 默认的消息到来的回调函数 
    void defaultMessageCallback(const TcpConnectionPtr& conn, Buffer* buffer, Timestamp receiveTime);
} //end namespace net
}

#endif