#include "../tcpserver.h"

#include "../../base/log.h"
#include "../../base/thread.h"
#include "../eventloop.h"
#include "../inetaddress.h"

#include <boost/bind.hpp>

#include <utility>

#include <stdio.h>
#include <unistd.h>

using namespace kaycc;
using namespace kaycc::net;

int numThreads = 0;

class EchoServer
{
 public:
  EchoServer(EventLoop* loop, const InetAddress& listenAddr)
    : loop_(loop),
      server_(loop, listenAddr, "EchoServer")
  {
    server_.setConnectionCallback(
        boost::bind(&EchoServer::onConnection, this, _1));
    server_.setMessageCallback(
        boost::bind(&EchoServer::onMessage, this, _1, _2, _3));
    server_.setThreadNum(numThreads);
  }

  void start()
  {
    server_.start();
  }
  // void stop();

 private:
  void onConnection(const TcpConnectionPtr& conn)
  {
    LOG << conn->peerAddress().toIpPort() << " -> "
        << conn->localAddress().toIpPort() << " is "
        << (conn->connected() ? "UP" : "DOWN") << std::endl;
    LOG << conn->getTcpInfoString() << std::endl;

    conn->send("hello\n");
  }

  void onMessage(const TcpConnectionPtr& conn, Buffer* buf, Timestamp time)
  {
    std::string msg(buf->retrieveAllAsString());
    LOG << conn->name() << " recv " << msg.size() << " bytes at " << time.toString() << std::endl;
    if (msg == "exit\n")
    {
      conn->send("bye\n");
      conn->shutdown();
    }
    if (msg == "quit\n")
    {
      loop_->quit();
    }
    conn->send(msg);
  }

  EventLoop* loop_;
  TcpServer server_;
};

int main(int argc, char* argv[])
{
  LOG << "pid = " << getpid() << ", tid = " << currentthread::tid() << std::endl;
  LOG << "sizeof TcpConnection = " << sizeof(TcpConnection) << std::endl;
  if (argc > 1)
  {
    numThreads = atoi(argv[1]);
  }
  bool ipv6 = argc > 2;
  EventLoop loop;
  InetAddress listenAddr(2000, false, ipv6);
  EchoServer server(&loop, listenAddr);

  server.start();

  loop.loop();
}

