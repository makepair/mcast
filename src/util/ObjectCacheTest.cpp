#include "ObjectCache.h"

#include "Test.h"

using namespace mcast;

TEST(ObjectCacheTest, PODTestCase) {
  typedef ObjectCache<int> Cache;
  int* ptr = Cache::get(123456);
  ASSERT_TRUE(ptr);
  ASSERT_EQ(*ptr, 123456);

  Cache::put(ptr);
}

struct TestObj {
  TestObj() = default;
  TestObj(int x, const std::string& str) : x_(x), str_(str) {}

  int x_{1};
  std::string str_{"123"};
};

struct TestObj2 {
  int x_{1};
  std::string str_{"123"};
};

TEST(ObjectCacheTest, SameCache) {
  ASSERT_TRUE(sizeof(TestObj) == sizeof(TestObj2));

  typedef ObjectCache<TestObj> m1;
  typedef ObjectCache<TestObj2> m2;
  auto* p1 = m1::get();
  m1::put(p1);
  auto* p2 = m2::get();
  ASSERT_EQ(static_cast<void*>(p1), static_cast<void*>(p2));
  m2::put(p2);
}

TEST(ObjectCacheTest, ObjectTestCase) {
  typedef ObjectCache<TestObj> Cache;
  TestObj* ptr = Cache::get();
  ASSERT_TRUE(ptr);
  ASSERT_EQ(ptr->x_, 1);
  ASSERT_EQ(ptr->str_, "123");
  Cache::put(ptr);

  ptr = Cache::get(4, "456");
  ASSERT_TRUE(ptr);
  ASSERT_EQ(ptr->x_, 4);
  ASSERT_EQ(ptr->str_, "456");
  Cache::put(ptr);
}

TEST(ObjectCacheTest, TestCase) {
  {
    typedef ObjectCache<TestObj, 1, 1> Cache;
    TestObj* ptr = Cache::get();  // get path 3
    ASSERT_TRUE(ptr);
    TestObj* ptr2 = Cache::get();  // get path 4
    ASSERT_TRUE(ptr2);

    Cache::put(ptr);               // put path 1
    TestObj* ptr3 = Cache::get();  // get path 1
    ASSERT_TRUE(ptr3);

    Cache::put(ptr2);  // put path 1
    Cache::put(ptr3);  // put path 2

    TestObj* ptr4 = Cache::get();  // get path 2
    ASSERT_TRUE(ptr4);
    Cache::put(ptr4);
  }
}