#pragma once

#include <time.h>
#include <cstdio>
#include <string>

namespace mcast {

static inline std::string TimeToString(time_t time_s) {
  char buf[64];
  buf[0] = '\0';
  struct tm tm_time;
  localtime_r(&time_s, &tm_time);
  std::snprintf(buf, sizeof(buf), "%4d-%02d-%02d %02d:%02d:%02d", tm_time.tm_year + 1900,
                tm_time.tm_mon + 1, tm_time.tm_mday, tm_time.tm_hour, tm_time.tm_min,
                tm_time.tm_sec);

  return buf;
}

static inline int64_t NowMilliseconds() {
  struct timespec now;
  int r = clock_gettime(CLOCK_REALTIME, &now);
  CHECK_EQ(r, 0);
  return static_cast<int64_t>(now.tv_sec) * 1000 +
         static_cast<int64_t>(now.tv_nsec) / 1000000;
}

static inline std::string TimeMsToString(int64_t ms) {
  time_t time_s = static_cast<time_t>(ms / 1000);
  return TimeToString(time_s);
}

static inline int64_t NowSeconds() {
  return NowMilliseconds() / 1000;
}

// static inline std::string TimeToString(int64_t seconds) {
//  return TimeToString(static_cast<time_t>(seconds));
//}

}  // namespcae mcast
