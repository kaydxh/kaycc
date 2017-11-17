#ifndef KAYCC_NET_ACCEPTOR_H 
#define KAYCC_NET_ACCEPTOR_H 

/*
Accpetor的作用如下：
1、创建监听（接收者）套接字
2、设置套接字选项
3、创建监听套接字的事件处理器，主要用于处理监听套接字的读事件（出现读事件表示有新的连接到来）
4、绑定地址
5、开始监听
6、等待事件到来
*/

/*
写服务器应用程序必须要考虑到服务器资源不足的情况，其中常见的一个是打开的文件数量（文件描述符数量）不能超过系统限制，当接受的连接太多时就会到达系统的限制，即表示打开的套接字文件描述符太多，从而导致accpet失败，返回EMFILE错误，但此时连接已经在系统内核建立好了，所以占用了系统的资源，我们不能让接受不了的连接继续占用系统资源，如果不处理这种错误就会有越来越多的内核连接建立，系统资源被占用也会越来越多，直到系统崩溃。一个常见的处理方式就是，先打开一个文件，预留一个文件描述符，出现EMFILE错误的时候，把打开的文件关闭，此时就会空出一个可用的文件描述符，再次调用accept就会成功，接受到客户连接之后，我们马上把它关闭，这样这个连接在系统中占用的资源就会被释放。关闭之后又会有一个文件描述符空闲，我们再次打开一个文件，占用文件描述符，等待下一次的EMFILE错误
*/

#include <boost/function.hpp>
#include <boost/noncopyable.hpp>

#include "channel.h"
#include "socket.h"


namespace kaycc {
namespace net {

    class EventLoop;
    class InetAddress;

    class Acceptor : boost::noncopyable {
    public:
          // 新连接到来的回调函数 
        typedef boost::function<void (int sockfd, 
                                      const InetAddress&)> NewConnectionCallback;

        Acceptor(EventLoop* loop, const InetAddress& listenAddr, bool reuseport);
        ~Acceptor();

        // 设置新连接到来的回调函数  
        void setNewConnectionCallback(const NewConnectionCallback& cb) {
            newConnectionCallback_ = cb;
        }

        // 是否正在监听
        bool listening() const {
            return listenning_;
        } 

        //监听
        void listen();

    private:
        // 处理读事件，acceptChannel_的读事件回调函数 
        void handleRead();

        // 所属的Reactor
        EventLoop* loop_;

        // 监听/接收者套接字 
        Socket acceptSocket_;

        // 监听套接字的事件处理器（主要处理读事件，即新链接到来的事件）
        Channel acceptChannel_;

        // 新连接到来的回调函数  
        NewConnectionCallback newConnectionCallback_;

        // 是否正在监听  
        bool listenning_;

        // 预留的空闲文件描述符，用于备用 
        int idleFd_;
 
    };

} //end net
}

#endif