#include "Queue.h"
#include "Test.h"

using namespace mcast;

TEST(QueueTest, test) {
  typedef Queue<int, 1> Queue;
  Queue q;
  int kNumOfObjectsPerBlock = Queue::kNumOfObjectsPerBlock;
  ASSERT_EQ(kNumOfObjectsPerBlock, 1);
  ASSERT_TRUE(q.Empty());
  ASSERT_EQ(q.Size(), 0);
  ASSERT_EQ(q.Capacity(), 1);

  const int x = 12345679;
  q.Push(x);
  ASSERT_EQ(q.Capacity(), 1);
  ASSERT_FALSE(q.Empty());
  ASSERT_EQ(q.Size(), 1);
  ASSERT_EQ(q.Front(), x);
  ASSERT_FALSE(q.Empty());
  q.Pop();
  ASSERT_TRUE(q.Empty());
  ASSERT_EQ(q.Size(), 0);

  // grow
  q.Push(1);
  q.Push(2);
  ASSERT_EQ(q.Size(), 2);
  ASSERT_EQ(q.Capacity(), 3);
  ASSERT_EQ(q.Front(), 1);
  q.Pop();
  ASSERT_EQ(q.Front(), 2);
  q.Pop();

  ASSERT_EQ(q.Capacity(), 1);
  ASSERT_TRUE(q.Empty());
  q.Clear();
  ASSERT_TRUE(q.Empty());
  ASSERT_EQ(q.Capacity(), 1);
}