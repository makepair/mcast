#ifndef CAST_TIMERSERVICE_H_
#define CAST_TIMERSERVICE_H_

#include <stdint.h>

#include <atomic>
#include <cassert>
#include <chrono>
#include <functional>
#include <list>
#include <map>
#include <memory>
#include <mutex>
#include <vector>

#include "util/Noncopyable.h"
#include "util/Thread.h"

namespace mcast {

class TimerService;

struct TimerSlot {
  typedef std::function<void()> Callback;
  typedef std::shared_ptr<TimerSlot> TimerPtr;
  typedef std::list<TimerPtr> TimerList;

  explicit TimerSlot(const Callback &callback) : cb(callback) {}

  uint32_t tm = 0;
  Callback cb;

  TimerList *slot = nullptr;
  TimerList::iterator pos;
};

typedef std::weak_ptr<TimerSlot> TimerHandle;

class TimerService : public Noncopyable {
  static constexpr int kSection1Bits = 8;
  static constexpr int kSection2Bits = 6;

  static constexpr int kSection1Num = 1 << kSection1Bits;
  static constexpr int kSection2Num = 1 << kSection2Bits;

  static constexpr int kSection1Mask = (kSection1Num - 1);
  static constexpr int kSection2Mask = (kSection2Num - 1);

 public:
  typedef TimerSlot::Callback Callback;
  typedef uint32_t Timestamp;

  static constexpr int kMillisecondsPerSecond = 1000;
  static constexpr int kMicrosecondsPerSecond = kMillisecondsPerSecond * 1000;
  static constexpr int kNonosecondsPerSecond = kMicrosecondsPerSecond * 1000;

  const int static kPeriod = 10;  // milliseconds

  TimerHandle AddTimer(uint32_t timeoutMilliSeconds, const Callback &callback);
  bool DeleteTimer(const TimerHandle &timer);

  uint64_t ToMilliseconds(Timestamp tm) {
    return tm * kPeriod;
  }

  void Run();

  Timestamp GetCurrentTime() const {
    return cur_time_.load();
  }

  Timestamp DurationSince(Timestamp timeStart) const {
    assert(cur_time_ >= timeStart);
    return cur_time_.load() - timeStart;
  }

 private:
  typedef TimerSlot::TimerPtr TimerPtr;
  typedef TimerSlot::TimerList TimerList;

  void DoAdd(uint32_t cur_time, const TimerPtr &tm);
  void Update(uint32_t tm);

  uint32_t GetSection2Index(uint32_t t, int Section);
  void AddSection1(const TimerPtr &tm);
  void AddSection2(int Section, const TimerPtr &tm);
  bool TickSection2(uint32_t t, int Section);

  void SetCurrentTime(uint32_t tm) {
    cur_time_ = tm;
  }

  TimerList time_slot_section1_[kSection1Num];
  TimerList time_slot_section2_[4][kSection2Num];

  std::atomic<uint32_t> cur_time_{1};

  std::mutex mutex_;

  friend void TimerServiceTest();
};

}  // namespace mcast

#endif  // CAST_TIMERSERVICE_H_