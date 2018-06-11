#include "ThreadPool.h"
#include "gtest/gtest.h"

#include <atomic>
#include <chrono>
#include <vector>

#include "Thread.h"

using namespace mcast;

static const int thread_num = 4;
static const int task_num = 100;
static const int per_thread_cout = 1000;
static const int sum = task_num * per_thread_cout;

static void ThreadCount(std::atomic_int *c) {
  for (int i = 0; i < per_thread_cout; ++i) {
    c->fetch_add(1, std::memory_order_relaxed);
  }
}

TEST(ThreadPoolTest, singleThreadTest) {
  ThreadPool<SingleWorkQueuePolicy> tp;
  tp.Start(thread_num);

  std::atomic_int count{0};
  for (int i = 0; i < task_num; ++i) {
    tp.Submit([&count]() mutable { ThreadCount(&count); });
  }

  int trynum = 100000;
  while (count.load() != sum && --trynum)
    this_thread::SleepFor(std::chrono::microseconds(1));

  tp.Stop();

  ASSERT_EQ(count.load(), sum);
}

TEST(ThreadPoolTest, WorkStealingQueuePolicyTest) {
  ThreadPool<WorkStealingQueuePolicy> tp;
  tp.Start(thread_num);

  std::atomic_int count{0};
  for (int i = 0; i < task_num; ++i) {
    tp.Submit([&count]() mutable { ThreadCount(&count); });
  }

  int trynum = 100000;
  while (count.load() != sum && --trynum)
    this_thread::SleepFor(std::chrono::microseconds(1));

  tp.Stop();

  ASSERT_EQ(count.load(), sum);
}