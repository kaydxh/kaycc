#ifndef KAYCC_NET_CALLBACKS_H 
#define KAYCC_NET_CALLBACKS_H 

#include <boost/function.hpp>
#include <boost/shared_ptr.hpp>

namespace kaycc {
namespace net {
    class Buffer;
    class TcpConnection;
    typedef boost::shared_ptr<TcpConnection> TcpConnectionPtr;
    typedef boost::function<void()> TimerCallback;
    typedef boost::function<void (const TcpConnectionPtr&)> ConnectionCallback;
    typedef boost::function<void (const TcpConnectionPtr&)> CloseCallback;
    typedef boost::function<void (const TcpConnectionPtr&)> WriteCompleteCallback;
} //end namespace net
}

#endif