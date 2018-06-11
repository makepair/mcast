#include "Acceptor.h"

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

static const std::string str_msg = "s";
static int touch = 0;
static int thread_num = 1;
static uint16_t port = 8881;

static Test_Task test_task;

TEST(AcceptorTest, test) {
  touch = false;
  System s;
  s.Start(thread_num);

  auto acceptor = System::CreateObject<Acceptor>();
  ASSERT_TRUE(acceptor->Initialize(port));
  acceptor->SetOnNewConnection([](int fd, const net::InetAddress &peer_addr) {
    if (fd >= 0) {
      close(fd);
    }
    ++touch;
    if (2 == touch)
      test_task.Done();
  });

  auto acceptor_handle = s.LaunchService<AcceptorService>(std::move(acceptor));
  ASSERT_TRUE(acceptor_handle);
  {
    auto r = net::tcp::Socket();
    ASSERT_TRUE(r);
    ASSERT_TRUE(net::tcp::Connect(r.get(), "127.0.0.1", port));
    close(r.get());
  }
  {
    auto r = net::tcp::Socket();
    ASSERT_TRUE(r);
    ASSERT_TRUE(net::tcp::Connect(r.get(), "127.0.0.1", port));
    close(r.get());
  }
  test_task.Wait();
  ASSERT_EQ(touch, 2);
  s.Stop();
}
