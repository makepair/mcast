#include "MPSCQueue.h"

#include "Test.h"

using namespace mcast::concurrence::waitfree;

TEST(MPSCQueueTest, test) {
  // typedef MPSCQueue<int> Queue;
  // Queue::Node stub;
  // Queue q(&stub);

  // ASSERT_TRUE(q.Empty());
  // ASSERT_EQ(q.Size(), 0);

  // Queue::Node nodex;
  // const int x = 12345679;
  // nodex.Set(x);
  // q.Push(&nodex);
  // ASSERT_FALSE(q.Empty());
  // ASSERT_EQ(q.Size(), 1);

  // Queue::Node *nodey;
  // ASSERT_TRUE(nodey = q.Pop());
  // ASSERT_EQ(x, nodey->Get());
  // ASSERT_TRUE(q.Empty());
  // ASSERT_EQ(q.Size(), 0);

  // nodex.Set(369);
  // q.Push(&nodex);
  // ASSERT_TRUE(!q.Empty());
  // ASSERT_EQ(q.Size(), 1);
  // q.Clear();
  // ASSERT_TRUE(q.Empty());
  // ASSERT_EQ(q.Size(), 0);

  // Queue::Node nodes[10];
  // for (int i = 0; i < 10; ++i) {
  //   nodes[i].Set(i);
  //   q.Push(&nodes[i]);
  // }

  // for (int i = 0; i < 10; ++i) {

  //   ASSERT_TRUE(nodey = q.Pop());
  //   ASSERT_EQ(i, nodey->Get());
  // }

  // ASSERT_TRUE(q.Empty());
  // ASSERT_EQ(q.Size(), 0);
}
