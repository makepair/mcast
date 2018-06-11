#include "TcpConnection.h"

#include <atomic>
#include <chrono>
#include <iostream>
#include <vector>

#include "Message.h"
#include "Service.h"
#include "System.h"

#include "util/Test.h"

using namespace mcast;
using namespace mcast::test;

static bool touch = false;
static int thread_num = 1;
static uint16_t port = 8889;
static const std::string str_msg = "hello, world";

static Test_Task test_task;

struct TcpConnectionServiceTest : public UserThreadService {
  TcpConnectionServiceTest(System* sys)
      : UserThreadService(sys, "TcpConnectionServiceTest") {
  }

  virtual void Main() override {
    StartEcho();
  }

  void StartEcho() {
    std::string buf;
    buf.resize(str_msg.size());
    auto s = conn_->Read(&buf[0], buf.size());
    if (s) {
      if (conn_->Write(buf.data(), buf.size())) {
      } else {
        LOG_WARN << "Write error " << s.ErrorText();
      }
    } else {
      LOG_WARN << "tcp conntion reset";
    }

    auto s2 = conn_->Read(&buf[0], buf.size());
    assert(s2.IsEof());
    test_task.Done();
  }

  TcpConnectionPtr conn_;
};

TEST(TcpConnectionTest, test) {
  touch = false;
  System sys;
  ASSERT_TRUE(sys.Start(thread_num));

  auto r = net::tcp::Socket();
  ASSERT_TRUE(r);
  int listenfd = r.get();
  ASSERT_TRUE(net::SetReuseAddr(listenfd));
  ASSERT_TRUE(net::tcp::Bind(listenfd, nullptr, port));
  ASSERT_TRUE(net::tcp::Listen(listenfd));

  r = net::tcp::Socket();
  ASSERT_TRUE(r);
  int clientfd = r.get();
  ASSERT_TRUE(net::tcp::Connect(clientfd, "127.0.0.1", port));

  int connfd = -1;
  r = net::tcp::Accept(listenfd);
  ASSERT_TRUE(r);
  connfd = r.get();

  ASSERT_GE(connfd, 0);

  Service::Handle tcpch;
  auto tcpsrv = System::CreateService<TcpConnectionServiceTest>(&sys);
  auto conn = System::CreateSharedObject<TcpConnection>(tcpsrv.get(), connfd);
  ASSERT_TRUE(conn->SetNonBlocking());

  tcpsrv->conn_ = conn;
  tcpch = sys.LaunchService(std::move(tcpsrv));
  ASSERT_TRUE(tcpch);

  const std::string msg = str_msg;
  ASSERT_TRUE(net::tcp::Send(clientfd, msg.data(), msg.size()));

  char buf[32] = {0};
  net::tcp::Recv(clientfd, buf, 31);
  ASSERT_EQ(std::string(msg), buf);
  close(clientfd);
  close(listenfd);

  test_task.Wait();
  sys.Stop();
}
