#ifndef CAST_TIMER_H_
#define CAST_TIMER_H_

#include <stdint.h>
#include <time.h>

#include <cassert>

namespace mcast {

namespace internal {

extern const uint64_t g_invariant_cpu_freq;

inline uint64_t CpuCycles() {
  uint32_t low = 0;
  uint32_t high = 0;
  __asm__ __volatile__("rdtsc" : "=a"(low), "=d"(high));
  return static_cast<uint64_t>(high) << 32 | low;
}
}

class Timestamp {
 public:
  static constexpr uint64_t kMillisecondsPerSecond = 1000;
  static constexpr uint64_t kMicrosecondsPerSecond = kMillisecondsPerSecond * 1000;
  static constexpr uint64_t kNonosecondsPerSecond = kMicrosecondsPerSecond * 1000;

  Timestamp() = default;
  Timestamp(uint64_t count) noexcept : time_(count) {}

  inline static Timestamp Now() {
    if (internal::g_invariant_cpu_freq) {
      uint64_t cycles = internal::CpuCycles();
      return Timestamp(cycles);
    } else {
      timespec ts;
      clock_gettime(CLOCK_REALTIME, &ts);
      return Timestamp(static_cast<uint64_t>(ts.tv_sec) * kNonosecondsPerSecond +
                       static_cast<uint64_t>(ts.tv_nsec));
    }
  }

  inline uint64_t count() const {
    return time_;
  }

  inline uint64_t ToNonoseconds() const {
    if (internal::g_invariant_cpu_freq) {
      uint64_t cycles = time_;
      uint64_t sec = cycles / internal::g_invariant_cpu_freq;
      uint64_t nsec = (cycles - sec * kNonosecondsPerSecond) * kNonosecondsPerSecond /
                      internal::g_invariant_cpu_freq;

      return sec * kNonosecondsPerSecond + nsec;

    } else {
      return time_;
    }
  }

  inline uint64_t ToMicroseconds() const {
    return ToNonoseconds() / 1000;
  }

  inline uint64_t ToMilliseconds() const {
    return ToNonoseconds() / 1000000;
  }

  inline uint64_t ToSeconds_Int() const {
    return ToNonoseconds() / kNonosecondsPerSecond;
  }

  inline double ToSeconds() const {
    double t = static_cast<double>(ToNonoseconds());
    return t / kNonosecondsPerSecond;
  }

 private:
  uint64_t time_ = 0;
  friend class Timer;
};

class Timer {
 public:
  void Start() {
    start_ = Timestamp::Now();
  }

  Timestamp Elapsed() {
    auto now = Timestamp::Now();
    return Timestamp(now.count() - start_.count());
  }

 private:
  Timestamp start_;
};
}
#endif  // CAST_TIMER_H_
