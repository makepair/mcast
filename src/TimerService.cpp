#include "TimerService.h"

#include <unistd.h>

#include "util/Logging.h"

namespace mcast {

TimerHandle TimerService::AddTimer(uint32_t timeout_milliseconds,
                                   const Callback &callback) {
  TimerHandle handle;
  uint32_t timeout = (timeout_milliseconds + kPeriod / 2) / kPeriod;
  if (0 == timeout) {
    callback();
    return handle;
  }

  TimerPtr timer(std::make_shared<TimerSlot>(callback));
  std::lock_guard<std::mutex> gl(mutex_);
  auto curtm = cur_time_.load();
  timer->tm = curtm + timeout;
  DoAdd(curtm, timer);

  return TimerHandle(timer);
}

bool TimerService::DeleteTimer(const TimerHandle &h) {
  auto timer = h.lock();
  if (!timer)
    return false;

  std::lock_guard<std::mutex> gl(mutex_);
  if (timer->slot)
    timer->slot->erase(timer->pos);
  return true;
}

void TimerService::DoAdd(uint32_t curtime, const TimerPtr &timer) {
  assert(timer->tm >= curtime);
  const auto tm_dx = timer->tm - curtime;

  if (tm_dx < kSection1Num) {
    AddSection1(timer);
  } else if (tm_dx < kSection1Num * kSection2Num) {
    AddSection2(0, timer);
  } else if (tm_dx < kSection1Num * kSection2Num * kSection2Num) {
    AddSection2(1, timer);
  } else if (tm_dx <
             kSection1Num * kSection2Num * kSection2Num * kSection2Num) {
    AddSection2(2, timer);
  } else {
    AddSection2(3, timer);
  }
}

void TimerService::AddSection1(const TimerPtr &timer) {
  const uint32_t i = timer->tm & kSection1Mask;
  assert(i < kSection1Num);

  time_slot_section1_[i].push_back(timer);
  timer->slot = &time_slot_section1_[i];
  timer->pos = --time_slot_section1_[i].end();
}

void TimerService::AddSection2(int section, const TimerPtr &timer) {
  uint32_t i = GetSection2Index(timer->tm, section);
  assert(i < kSection2Num);

  time_slot_section2_[section][i].push_back(timer);
  timer->slot = &time_slot_section2_[section][i];
  timer->pos = --time_slot_section2_[section][i].end();
}

bool TimerService::TickSection2(uint32_t curtime, int section) {
  uint32_t i = GetSection2Index(curtime, section);
  std::lock_guard<std::mutex> gl(mutex_);

  for (auto &timer : time_slot_section2_[section][i]) {
    DoAdd(curtime, std::move(timer));
  }
  time_slot_section2_[section][i].clear();

  return i == 0;
}

inline uint32_t TimerService::GetSection2Index(uint32_t t, int section) {
  assert(section < 4);
  t >>= kSection1Bits + kSection2Bits * section;
  return t & kSection2Mask;
}

void TimerService::Update(uint32_t curtime) {
  const uint32_t section1_i = curtime & kSection1Mask;
  if (section1_i == 0 && TickSection2(curtime, 0) && TickSection2(curtime, 1) &&
      TickSection2(curtime, 2)) {
    TickSection2(curtime, 3);
  }

  TimerList list;
  {
    std::lock_guard<std::mutex> gl(mutex_);
    for (auto &timer : time_slot_section1_[section1_i]) {
      timer->slot = nullptr;
    }

    list.splice(list.begin(), time_slot_section1_[section1_i]);
    assert(time_slot_section1_[section1_i].empty());
  }

  for (auto &timer : list) {
    timer->cb();
  }
}

void TimerService::Run() {
  using std::chrono::duration;
  using std::chrono::duration_cast;
  using std::chrono::steady_clock;
  typedef std::ratio<1, 1000 / kPeriod> ratio;

  auto Start = steady_clock::now();
  while (!this_thread::IsInterrupted()) {
    usleep(kPeriod * 1000 / 2);
    auto now = steady_clock::now();
    auto const d = duration_cast<duration<int64_t, ratio>>(now - Start);
    auto count = d.count();
    for (decltype(count) i = 0; i < count; ++i) {
      uint32_t tm = cur_time_.fetch_add(1);
      Update(tm);
    }

    Start += d;
  }
}

}  // namespace mcast
