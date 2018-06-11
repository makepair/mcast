#include "TimerService.h"

#include "gtest/gtest.h"

#include <functional>

#include "util/Test.h"

using namespace mcast;

namespace mcast {

void TimerServiceTest() {
  TimerService tsrv;

  int count = 0;
  auto curtime = tsrv.GetCurrentTime();

  tsrv.AddTimer(0, [&count]() mutable { ++count; });
  tsrv.Update(curtime);
  ASSERT_EQ(count, 1);

  count = 0;
  curtime = tsrv.GetCurrentTime();
  tsrv.AddTimer(10, [&count]() mutable { ++count; });
  tsrv.Update(curtime);
  ASSERT_EQ(count, 0);
  ++curtime;
  tsrv.SetCurrentTime(curtime);
  tsrv.Update(curtime);
  ASSERT_EQ(count, 1);

  count = 0;
  auto h = tsrv.AddTimer(10, [&count]() mutable { ++count; });
  ASSERT_TRUE(tsrv.DeleteTimer(h));
  ASSERT_TRUE(!tsrv.DeleteTimer(h));
  tsrv.Update(curtime+1);
  ASSERT_EQ(count, 0);

  count = 0;
  curtime = tsrv.GetCurrentTime();
  tsrv.AddTimer(10 * (TimerService::kSection1Num - 1),
                [&count]() mutable { ++count; });
  curtime += (TimerService::kSection1Num - 1);
  tsrv.SetCurrentTime(curtime);
  tsrv.Update(curtime);
  ASSERT_EQ(count, 1);

  count = 0;
  curtime = 0;
  tsrv.SetCurrentTime(curtime);
  tsrv.AddTimer(10 * TimerService::kSection1Num, [&count]() mutable { ++count; });
  curtime += TimerService::kSection1Num;
  tsrv.SetCurrentTime(curtime);
  tsrv.Update(curtime);
  ASSERT_EQ(count, 1);

  count = 0;
  curtime = 0;
  tsrv.SetCurrentTime(curtime);
  uint32_t timeout = TimerService::kSection1Num * TimerService::kSection2Num;
  tsrv.AddTimer(10 * timeout, [&count]() mutable { ++count; });
  curtime += timeout;
  tsrv.SetCurrentTime(curtime);
  tsrv.Update(curtime);
  ASSERT_EQ(count, 1);

  count = 0;
  curtime = 0;
  tsrv.SetCurrentTime(curtime);
  timeout = TimerService::kSection1Num * TimerService::kSection2Num *
            TimerService::kSection2Num;
  tsrv.AddTimer(10 * timeout, [&count]() mutable { ++count; });
  curtime += timeout;
  tsrv.SetCurrentTime(curtime);
  tsrv.Update(curtime);
  ASSERT_EQ(count, 1);

  count = 0;
  curtime = 0;
  tsrv.SetCurrentTime(curtime);
  timeout = TimerService::kSection1Num * TimerService::kSection2Num *
            TimerService::kSection2Num * TimerService::kSection2Num;
  tsrv.AddTimer(10 * timeout, [&count]() mutable { ++count; });
  curtime += timeout;
  tsrv.SetCurrentTime(curtime);
  tsrv.Update(curtime);
  ASSERT_EQ(count, 1);
}
}  // namespace mcast

// TEST(TimerServiceTest, TestEverySecond) {
//   TimerServiceTest();

//   TimerService tsrv;
//   Thread th;
//   th.Run([&tsrv]() mutable { tsrv.Run(); });

//   for (int i = 1; i < 100000; ++i) {
//     tsrv.AddTimer(i * 1000, [i] { LOG_WARN << i; });
//   }

//   this_thread::SleepFor(std::chrono::seconds(300));
//   th.Interrupt();
//   th.Join();
// }
