#include "Thread.h"

#include <atomic>
#include <chrono>
#include <vector>

#include "Test.h"

using namespace mcast;
using namespace mcast::test;

static Test_Task test_task;

void testInterruptedMain() {
  this_thread::AtExit([] { test_task.Done(); });

  while (!this_thread::IsInterrupted()) {
    this_thread::Yield();
  }
}

TEST(ThreadTest, TestInterruptedAndExit) {
  Thread t(Thread().Run(&testInterruptedMain));
  ASSERT_TRUE(t.Joinable());
  ASSERT_TRUE(!t.IsInterrupted());

  t.Interrupt();
  ASSERT_TRUE(t.IsInterrupted());
  test_task.Wait();
  t.Join();
  ASSERT_TRUE(t.IsInterrupted());

  Thread t2(std::move(t));
  ASSERT_TRUE(t2.IsInterrupted());

  Thread t3;
  t3 = std::move(t2);
  ASSERT_TRUE(t3.IsInterrupted());
}

static void ThreadCount(std::atomic_int *c) {
  this_thread::SleepFor(std::chrono::milliseconds(1));
  for (int i = 0; i < 100; ++i)
    c->fetch_add(1, std::memory_order_relaxed);
}

TEST(ThreadTest, testCount) {
  std::vector<Thread> ts;
  ts.resize(5);

  std::atomic_int count{0};
  for (unsigned i = 0; i < 5; ++i) {
    ts[i] = Thread().Run(&ThreadCount, &count);
  }

  for (int i = 0; i < 5; ++i) {
    ts.push_back(Thread().Run(&ThreadCount, &count));
  }

  for (auto &t : ts)
    t.Join();

  ASSERT_EQ(count.load(), 1000);
}
