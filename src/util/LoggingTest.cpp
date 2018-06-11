#include "Logging.h"
#include "gtest/gtest.h"

using namespace mcast;

static LogLevel curlevel = LogLevel::kInfo;
static std::string msg;

static void Printer(LogLevel lev, const char *, const char *, const char *s,
                    size_t len) {
  ASSERT_EQ(curlevel, lev);
  msg.assign(s, len);
};

TEST(LoggingTest, test) {
  auto oldlev = SetLogLevel(LogLevel::kInfo);
  auto oldp = SetLogPrinter(Printer);
  curlevel = LogLevel::kInfo;
  LOG_INFO << '1' << "2" << 3 << std::string("4") << 5.0;
  ASSERT_EQ(msg, "12345");

  msg.clear();
  curlevel = LogLevel::kTrace;
  LOG_TRACE << "feed";
  ASSERT_TRUE(msg.empty());
  SetLogLevel(LogLevel::kTrace);
  LOG_TRACE << "feed";
  ASSERT_EQ(msg, "feed");

  curlevel = LogLevel::kWarning;
  LOG_WARN << "feed";
  ASSERT_EQ(msg, "feed");

  curlevel = LogLevel::kFata;
  LOG_FATA << "feed";
  ASSERT_EQ(msg, "feed");

  SetLogLevel(oldlev);
  SetLogPrinter(oldp);

  // LOG_FATA << "oops";

}