#include "../tcpclient.h"

#include "../../base/log.h"
#include "../../base/thread.h"
#include "../../base/types.h"
#include "../eventloop.h"
#include "../inetaddress.h"

#include <boost/bind.hpp>
#include <boost/ptr_container/ptr_vector.hpp>

#include <utility>

#include <stdio.h>
#include <unistd.h>

using namespace kaycc;
using namespace kaycc::net;

int numThreads = 0;
class EchoClient;
boost::ptr_vector<EchoClient> clients;
int current = 0;

class EchoClient : boost::noncopyable
{
 public:
  EchoClient(EventLoop* loop, const InetAddress& listenAddr, const std::string& id)
    : loop_(loop),
      client_(loop, listenAddr, "EchoClient"+id)
  {
    client_.setConnectionCallback(
        boost::bind(&EchoClient::onConnection, this, _1));
    client_.setMessageCallback(
        boost::bind(&EchoClient::onMessage, this, _1, _2, _3));
    //client_.enableRetry();
  }

  void connect()
  {
    client_.connect();
  }
  // void stop();

 private:
  void onConnection(const TcpConnectionPtr& conn)
  {
    LOG << conn->localAddress().toIpPort() << " -> "
        << conn->peerAddress().toIpPort() << " is "
        << (conn->connected() ? "UP" : "DOWN") << std::endl;

    if (conn->connected())
    {
      ++current;
      if (implicit_cast<size_t>(current) < clients.size())
      {
        clients[current].connect();
      }
      LOG << "*** connected " << current << std::endl;
    }
    conn->send("world\n");
  }

  void onMessage(const TcpConnectionPtr& conn, Buffer* buf, Timestamp time)
  {
    std::string msg(buf->retrieveAllAsString());
    LOG << conn->name() << " recv " << msg.size() << " bytes at " << time.toString() << std::endl;
    if (msg == "quit\n")
    {
      conn->send("bye\n");
      conn->shutdown();
    }
    else if (msg == "shutdown\n")
    {
      loop_->quit();
    }
    else
    {
      conn->send(msg);
    }
  }

  EventLoop* loop_;
  TcpClient client_;
};

int main(int argc, char* argv[])
{
  LOG << "pid = " << getpid() << ", tid = " << currentthread::tid() << std::endl;
  if (argc > 1)
  {
    EventLoop loop;
    bool ipv6 = argc > 3;
    InetAddress serverAddr(argv[1], 2000, ipv6);

    int n = 1;
    if (argc > 2)
    {
      n = atoi(argv[2]);
    }

    clients.reserve(n);
    for (int i = 0; i < n; ++i)
    {
      char buf[32];
      snprintf(buf, sizeof buf, "%d", i+1);
      clients.push_back(new EchoClient(&loop, serverAddr, buf));
    }

    clients[current].connect();
    loop.loop();
  }
  else
  {
    printf("Usage: %s host_ip [current#]\n", argv[0]);
  }
}

