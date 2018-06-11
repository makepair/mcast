#include "ThreadSafeQueue.h"
#include "gtest/gtest.h"

#include <atomic>
#include <chrono>
#include <vector>

#include "Thread.h"


using namespace mcast;

TEST(ThreadSafeQueueTest, singleThreadTest) {
  ThreadSafeQueue<int> q;
  ASSERT_TRUE(q.empty());
  ASSERT_EQ(q.size(), 0);

  q.push(123);
  ASSERT_TRUE(!q.empty());
  ASSERT_EQ(q.size(), 1);

  ASSERT_EQ(*q.front(), 123);
  int x = 0;
  ASSERT_TRUE(q.pop(&x));
  ASSERT_EQ(x, 123);

  ASSERT_TRUE(q.empty());
  ASSERT_EQ(q.size(), 0);

  ASSERT_EQ(q.pushAndFetchSize(4), 1);
  ASSERT_EQ(q.pushAndFetchSize(5), 2);

}