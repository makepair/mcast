#include "System.h"

#include <atomic>
#include <chrono>
#include <iostream>
#include <vector>

#include "Message.h"
#include "Service.h"

#include "util/Test.h"

using namespace mcast;
using namespace mcast::test;

static const std::string g_str_msg{"hello, world!"};

struct SystemTest : public testing::Test {
 protected:
  virtual void SetUp() {
    touch = 0;
    ASSERT_TRUE(sys.Start(thread_num));
  }

  virtual void TearDown() {
    sys.Stop();
  }

 protected:
  const std::string str_msg{g_str_msg};
  int touch = 0;
  int thread_num = 2;
  System sys;
  Test_Task test_task;
};

struct MessageDrivenServiceTest : public MessageDrivenService {
  using MessageDrivenService::MessageDrivenService;

  ~MessageDrivenServiceTest() override {
    // LOG_INFO << "~MessageDrivenServiceTest()";
  }

  void OnServiceStart() override {
    // LOG_INFO << "MessageDrivenServiceTest::OnServiceStart()";
  }

  void OnServiceStop() override {
    // LOG_INFO << "MessageDrivenServiceTest::OnServiceStop()";
  }

  virtual void HandleMessage(const MessagePtr& msg) override {
    assert(msg->type() == Message::KString);
    StringMessage* smsg = dynamic_cast<StringMessage*>(msg.get());
    ASSERT_TRUE(smsg);
    ASSERT_EQ(smsg->get_msg(), g_str_msg);
    msg->Done(Status::OK());
    Stop();
  }
};

TEST_F(SystemTest, MessageDrivenServiceTestCase) {
  auto sh = sys.LaunchService<MessageDrivenServiceTest>("MessageDrivenServiceTest");
  ASSERT_TRUE(sh);
  auto r = sys.SendStringMessage(sh, str_msg, [this](const Status& status) {
    if (status)
      touch++;
    test_task.Done();
  });

  ASSERT_TRUE(r);
  test_task.Wait();
  ASSERT_EQ(1, touch);
}

struct UserThreadServiceTest : public UserThreadService {
  using UserThreadService::UserThreadService;
  UserThreadServiceTest(System* sys, const std::string& name, Test_Task* tt, int* touch)
      : UserThreadService(sys, name), touch_(touch), test_task(tt) {}

  virtual void Main() override {
    ++*touch_;
    test_task->Done();
  }

  int* touch_ = nullptr;
  Test_Task* test_task = nullptr;
};

TEST_F(SystemTest, UserThreadServiceTestCase) {
  auto sh = sys.LaunchService<UserThreadServiceTest>("UserThreadServiceTest", &test_task,
                                                     &touch);
  ASSERT_TRUE(sh);
  test_task.Wait();
  ASSERT_EQ(1, touch);
}

struct MethodCallServiceTest : public MethodCallService {
  using MethodCallService::MethodCallService;

  void foo1(int req, int* res) {
    *res = req;
  }

  void foo2(std::string&& req, std::string* res) {
    *res = std::move(req);
  }
};

TEST_F(SystemTest, MethodCallServiceTestCase) {
  auto sh = sys.LaunchService<MethodCallServiceTest>("MethodCallServiceTest");
  ASSERT_TRUE(sh);

  int int_res = 0;
  ASSERT_TRUE(sys.CallMethod(sh, &MethodCallServiceTest::foo1, 123, &int_res));
  ASSERT_EQ(int_res, 123);

  std::string res_str;
  ASSERT_TRUE(
      sys.CallMethod(sh, &MethodCallServiceTest::foo2, std::string("123"), &res_str));

  ASSERT_EQ(res_str, "123");
}

struct WakeupSleepServiceTest : public UserThreadService {
  WakeupSleepServiceTest(System* sys, const std::string& name, Test_Task* tt)
      : UserThreadService(sys, name), test_task(tt) {}

  virtual void Main() override {
    auto s = Sleep(10000000);
    ASSERT_FALSE(s);
    test_task->Done();
  }

  Test_Task* test_task = nullptr;
};

TEST_F(SystemTest, WakeupTestServiceTestCase) {
  auto sh =
      sys.LaunchService<WakeupSleepServiceTest>("WakeupSleepServiceTest", &test_task);
  ASSERT_TRUE(sh);
  sys.WakeupIfWaitTimeout(sh, 100);
  test_task.Wait();
}

struct ServiceInterruptTest : public UserThreadService {
  ServiceInterruptTest(System* sys, const std::string& name, Test_Task* tt)
      : UserThreadService(sys, name), test_task(tt) {}

  virtual void Main() override {
    auto s = Sleep(100000000);
    ASSERT_TRUE(s.IsInterrupt());
    test_task->Done();
  }

  Test_Task* test_task = nullptr;
};

TEST_F(SystemTest, ServiceInterruptTestCase) {
  auto sh = sys.LaunchService<ServiceInterruptTest>("ServiceInterruptTest", &test_task);
  ASSERT_TRUE(sh);
  std::this_thread::sleep_for(std::chrono::milliseconds(30));
  sys.InterruptService(sh);
  test_task.Wait();
}

struct ServiceSignalTest : public UserThreadService {
  ServiceSignalTest(System* sys, const std::string& name, Test_Task* tt)
      : UserThreadService(sys, name), test_task(tt) {}

  virtual void Main() override {
    WaitSignal();
    test_task->Done();
  }

  Test_Task* test_task = nullptr;
};

TEST_F(SystemTest, ServiceSignalTestCase) {
  auto sh = sys.LaunchService<ServiceSignalTest>("ServiceSignalTest", &test_task);
  ASSERT_TRUE(sh);
  sys.Signal(sh);
  test_task.Wait();
}

// struct SleepServiceTest : public UserThreadService {
//   using UserThreadService::UserThreadService;

//   virtual void Main() override {
//     int i = 1;
//     while (!IsStopping()) {
//       Sleep(1000);
//       LOG_WARN << "Sleep " << i++;
//     }
//   }
// };

// TEST_F(SystemTest, SleepServiceTest) {
//   auto sh = sys.LaunchService<SleepServiceTest>("SleepServiceTest");
//   this_thread::SleepFor(std::chrono::seconds(10));
//   sys.StopService(sh);
// }
//

// struct TimerServiceTest : public MethodCallService {
//  using MethodCallService::MethodCallService;
//
//  void OnServiceStop() override { system()->RemoveTimer(timer_h_); }
//
//  void StartTimer() {
//    LOG_INFO << "timer tick " << this->handle().index();
//    system()->RemoveTimer(timer_h_);
//    auto self_h = GetHandle<TimerServiceTest>();
//    auto sys = system();
//    timer_h_ = system()->AddTimer(100, [sys, self_h]() {
//      sys->AsyncCallMethod(self_h, &TimerServiceTest::StartTimer);
//    });
//  }
//
//  TimerHandle timer_h_;
//};
//
// TEST_F(SystemTest, TimerServiceTest) {
//  auto sh = sys.LaunchService<TimerServiceTest>("TimerServiceTest");
//  sys.CallMethod(sh, &TimerServiceTest::StartTimer);
//  auto sh2 = sys.LaunchService<TimerServiceTest>("TimerServiceTest");
//  sys.CallMethod(sh2, &TimerServiceTest::StartTimer);
//  this_thread::SleepFor(std::chrono::seconds(30));
//}
