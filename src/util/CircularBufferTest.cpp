#include "CircularBuffer.h"
#include "Test.h"

using namespace mcast;

TEST(CircularBufferTest, test_main_ops) {
  CircularBuffer<int> q(2);
  ASSERT_TRUE(q.Empty());
  ASSERT_EQ(q.Size(), 0);
  ASSERT_EQ(q.begin(), q.end());
  ASSERT_EQ(q.cbegin(), q.cend());
  ASSERT_EQ(q.EndPosition(), 0);
  ASSERT_EQ(q.BeginPosition(), 0);

  const int x = 12345679;
  q.PushBack(x);
  ASSERT_EQ(q.EndPosition(), 1);

  ASSERT_FALSE(q.Empty());
  ASSERT_EQ(q.Size(), 1);
  int y = q.Front();
  ASSERT_EQ(y, x);
  q.PopFront();
  ASSERT_EQ(q.BeginPosition(), 1);
  ASSERT_TRUE(q.Empty());
  ASSERT_EQ(q.Size(), 0);

  q.PushBack(147);
  ASSERT_EQ(q.BeginPosition(), 1);
  ASSERT_EQ(q.EndPosition(), 0);
  ASSERT_FALSE(q.Empty());
  q.Clear();
  ASSERT_TRUE(q.Empty());
  ASSERT_EQ(q.BeginPosition(), 0);
  ASSERT_EQ(q.EndPosition(), 0);

  q.PushBack(1);
  q.PopFront();
  ASSERT_TRUE(q.Empty());
  ASSERT_EQ(q.BeginPosition(), 1);
  ASSERT_EQ(q.EndPosition(), 1);

  const int num = q.capacity();
  ASSERT_EQ(num, 2);

  q.PushBack(1);
  q.PushBack(2);
  ASSERT_EQ(q.BeginPosition(), 1);
  ASSERT_EQ(q.EndPosition(), 1);
  y = -1;
  y = q.Front();
  q.PopFront();
  ASSERT_EQ(y, 1);
  ASSERT_EQ(q.BeginPosition(), 0);
  ASSERT_EQ(q.EndPosition(), 1);
  y = q.Front();
  q.PopFront();
  ASSERT_EQ(y, 2);
  ASSERT_TRUE(q.Empty());

  // grow
  ASSERT_TRUE(q.Empty());
  ASSERT_EQ(q.BeginPosition(), 1);
  ASSERT_EQ(q.EndPosition(), 1);

  for (size_t i = 0; i < num + 1; i++) {
    q.PushBack(i + 100);
  }

  ASSERT_TRUE(q.capacity() > num);
  
  for (size_t i = 0; i < num + 1; i++) {
    y = q.Front();
    q.PopFront();
    ASSERT_EQ(y, i + 100);
  }
}
