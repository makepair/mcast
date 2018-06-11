#include "TcpServer.h"

#include "gtest/gtest.h"

#include <functional>

#include "System.h"

#include "util/Test.h"

using namespace mcast;
using namespace mcast::test;

static std::string g_str_msg = "1234567890";
static uint16_t port = 13087;

static int ConnectToServer() {
  auto r = net::tcp::Socket();

  EXPECT_TRUE(r);
  EXPECT_TRUE(net::tcp::Connect(r.get(), "127.0.0.1", port));
  net::tcp::SetNoDelay(r.get());
  return r.get();
}

struct TcpServerTest : public testing::Test {
 protected:
  virtual void SetUp() override {
    touch = false;
    ASSERT_TRUE(sys.Start(thread_num));
  }

  virtual void TearDown() override {
    server.Stop();
    sys.Stop();
  }

 protected:
  const std::string str_msg{g_str_msg};
  int touch = 0;
  int thread_num = 2;
  TcpServer server;
  System sys;
  Test_Task test_task;
};

TEST_F(TcpServerTest, testEcho) {
  server.SetOnNewConnection([](TcpConnection *conn) {
    conn->SetTcpNoDelay();
    return [conn]() mutable {
      char buf[128];
      auto s = conn->Read(buf, g_str_msg.size());
      if (s) {
        conn->Write(buf, g_str_msg.size());
      } else {
        LOG_WARN << "tcp conntion reset";
      }
    };
  });

  ASSERT_TRUE(server.Start(&sys, port));
  {
    int clientfd = ConnectToServer();
    const std::string msg = g_str_msg;
    auto r = net::tcp::Send(clientfd, msg.data(), msg.size());
    ASSERT_TRUE(r);
    ASSERT_TRUE(net::tcp::Send(clientfd, msg.data(), msg.size()));
    char buf[32] = {0};
    ASSERT_TRUE(net::tcp::Recv(clientfd, buf, 32));
    ASSERT_EQ(std::string(msg), buf);
    close(clientfd);
  }
}

TEST_F(TcpServerTest, testReadError) {
  server.SetOnNewConnection([this](TcpConnection *conn) {
    return [conn, this]() mutable {
      conn->SetTcpNoDelay();
      char buf[128];
      auto s = conn->Read(buf, g_str_msg.size());
      if (!s) {
        touch++;
      }
      test_task.Done();
    };
  });

  ASSERT_TRUE(server.Start(&sys, port));
  {
    int clientfd = ConnectToServer();
    close(clientfd);
  }

  test_task.Wait();
  ASSERT_EQ(1, touch);
}

TEST_F(TcpServerTest, testReadEof) {
  server.SetOnNewConnection([this](TcpConnection *conn) {
    conn->SetTcpNoDelay();
    return [conn, this]() mutable {
      char buf[32];
      auto s = conn->Read(buf, g_str_msg.size());
      if (s.IsEof()) {
        touch++;
      }
      test_task.Done();
    };
  });

  ASSERT_TRUE(server.Start(&sys, port));
  int clientfd = ConnectToServer();
  net::tcp::ShutdownWrite(clientfd);
  test_task.Wait();
  ASSERT_EQ(1, touch);
  close(clientfd);
}

// TEST(TcpServerTest, testWatcher) {
//   touch = false;
//   System sys;
//   sys.Start(thread_num);
//   TcpServer server;
//   server.SetOnNewConnection([](TcpConnection *conn) {
//     return [conn]() mutable {
//       char buf[32];
//       conn->Read(buf, test_str.size());
//     };
//   });

//   server.Start(&sys, port);
//   {
//     int clientfd = net::tcp::Socket();
//     net::tcp::Connect(clientfd, "127.0.0.1", port);
//     this_thread::SleepFor(std::chrono::microseconds(100));
//   }

//   this_thread::SleepFor(std::chrono::seconds(10));
//   server.Stop();
//   sys.Stop();
// }
