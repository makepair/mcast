#include "Status.h"

#include "gtest/gtest.h"
#include "Logging.h"

using namespace mcast;

TEST(StatusTest, test) {
  {
    Status s;
    ASSERT_TRUE(s.IsOK());
    ASSERT_TRUE(s);
    LOG_INFO << s;
  }
  {
    Status s = Status::OK();
    ASSERT_TRUE(s.IsOK());
    ASSERT_TRUE(s);
  }
  {
    Status s = Status(kInvailArgument, "InvailArgument");
    ASSERT_TRUE(!s.IsOK());
    ASSERT_TRUE(!s);
    ASSERT_TRUE(s.IsInvailArgument());
    ASSERT_EQ(s.ErrorText(), "InvailArgument");
    LOG_INFO << s;
  }
  {
    Status s = Status(kFailed, "Failed");
    ASSERT_TRUE(!s.IsOK());
    ASSERT_TRUE(!s);
    ASSERT_TRUE(s.IsFailed());
    ASSERT_EQ(s.ErrorText(), "Failed");

    Status s2(std::move(s));
    ASSERT_TRUE(s.IsOK());
    ASSERT_TRUE(s);
    ASSERT_TRUE(!s2.IsOK());
    ASSERT_TRUE(!s2);
    ASSERT_TRUE(s2.IsFailed());
    ASSERT_EQ(s2.ErrorText(), "Failed");

    Status s3;
    s3 = std::move(s2);
    ASSERT_TRUE(s2.IsOK());
    ASSERT_TRUE(s2);
    ASSERT_TRUE(!s3.IsOK());
    ASSERT_TRUE(!s3);
    ASSERT_TRUE(s3.IsFailed());
    ASSERT_EQ(s3.ErrorText(), "Failed");
  }
}
