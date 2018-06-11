#include "Logging.h"
#include "gtest/gtest.h"

#include "google/protobuf/message.h"

int main(int argc, char *argv[]) {
  GOOGLE_PROTOBUF_VERIFY_VERSION;

  ::testing::InitGoogleTest(&argc, argv);
  SetLogLevel(mcast::LogLevel::kInfo);
  // ::testing::GTEST_FLAG(filter) = "SystemTest.MessageDrivenServiceTestCase*";
  // ::testing::GTEST_FLAG(filter) = "RPCServerTest.*";
  auto res = RUN_ALL_TESTS();

  google::protobuf::ShutdownProtobufLibrary();
  return res;
}
