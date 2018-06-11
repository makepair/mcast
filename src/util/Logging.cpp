#include "Logging.h"

#include <execinfo.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <unistd.h>

#include <sys/time.h>
#include <time.h>
#include <cassert>
#include <string>

namespace mcast {

LogNullStream<char> Logger::s_null_stream;

static const char *const g_level_string[] = {"X", "T", "I", "W", "F"};

static char *Timestamp(char *buf, size_t size) {
  assert(size > 0);

  struct timeval tv;
  buf[0] = '\0';
  if (-1 != gettimeofday(&tv, NULL)) {
    struct tm tm_time;
    localtime_r(&tv.tv_sec, &tm_time);

    std::snprintf(buf, size, "%4d-%02d-%02d %02d:%02d:%02d.%06u", tm_time.tm_year + 1900,
                  tm_time.tm_mon + 1, tm_time.tm_mday, tm_time.tm_hour, tm_time.tm_min,
                  tm_time.tm_sec, static_cast<unsigned int>(tv.tv_usec));
  }

  return buf;
}

static void DefaultLogPrint(LogLevel lev, const char *file_name, const char *line,
                            const char *cmsg, size_t len) noexcept {
  if (len > 0 && lev < LogLevel::kNum) {
    LogStream<1024> stream;
    char buf[64];
    assert(static_cast<size_t>(lev) < sizeof(g_level_string) / sizeof(g_level_string[0]));

    // FIXME: use thread_local to cache syscall(SYS_gettid)
    stream << g_level_string[static_cast<unsigned int>(lev)] << ' ' << syscall(SYS_gettid)
           << ' ' << Timestamp(buf, sizeof buf) << " ";
    stream << file_name << ':' << line << " " << cmsg << '\n';

    fwrite(stream.c_string(), 1, stream.avail(), stdout);

    if (lev >= LogLevel::kWarning) {
      fflush(stdout);
      if (lev == LogLevel::kFata) {
        //        fsync(STDOUT_FILENO);
        std::abort();
      }
    }
  }
}

LogLevel Logger::s_level = LogLevel::kInfo;
Logger::Printer Logger::s_printer{DefaultLogPrint};

}  // namespace mcast
