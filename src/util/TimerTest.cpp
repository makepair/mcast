#include "Timer.h"

#include "Test.h"

using namespace mcast;

// TEST(TimerTest, test) {
//   Timer t;
//   t.Start();
//   LOG_WARN  << t.Elapsed().count();
//   LOG_WARN << t.Elapsed().ToNonoseconds();
//   LOG_WARN << std::fixed << std::setprecision(9) << t.Elapsed().ToSeconds();
//   sleep(1);
//   LOG_WARN << t.Elapsed().count();
//   LOG_WARN << t.Elapsed().ToNonoseconds();
//   LOG_WARN << std::fixed << std::setprecision(9) << t.Elapsed().ToSeconds();
// }