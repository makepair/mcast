#ifndef CAST_THREAD_H_
#define CAST_THREAD_H_

#include <atomic>
#include <cassert>
#include <chrono>
#include <functional>
#include <memory>
#include <thread>
#include <utility>
#include <vector>

#include "Logging.h"
#include "Noncopyable.h"

namespace mcast {

class Thread;

class ThreadImpl : public Noncopyable {
 public:
  typedef std::function<void()> Callback;
  typedef std::thread::id ID;

  ThreadImpl() = default;

  template <typename Function, typename... Args>
  void Run(Function&& f, Args&&... args) {
    thread_ =
        std::thread(std::forward<Function>(f), std::forward<Args>(args)...);
  }

  void Join() {
    thread_.join();
  }

  bool Joinable() {
    return thread_.joinable();
  }

  ID GetID() {
    return thread_.get_id();
  }

  void Interrupt() {
    interrupted_.store(true, std::memory_order_relaxed);
  }

  bool IsInterrupted() {
    return interrupted_.load(std::memory_order_relaxed);
  }

  void AtExit(const Callback& cb) {
    atexit_list_.push_back(cb);
  }

 private:
  std::atomic_bool interrupted_{false};
  std::thread thread_;
  std::vector<Callback> atexit_list_;

  friend class Thread;
};

namespace this_thread {
template <typename Callback>
void AtExit(Callback&& cb);

bool IsInterrupted();
void Interrupt();
}  // namespace this_thread

struct AtThreadExitCaller {
  typedef ThreadImpl::Callback Callback;
  AtThreadExitCaller() {
    // LOG_WARN << "AtThreadExitCaller";
  }

  ~AtThreadExitCaller() {
    // LOG_WARN << "~AtThreadExitCaller";
    for (auto& cb : atexit_list_) {
      cb();
    }

    atexit_list_.clear();
  }

  template <typename Callback>
  void AtExit(Callback&& cb) {
    return atexit_list_.push_back(cb);
  }

  std::vector<Callback> atexit_list_;
};

class Thread : public Noncopyable {
 public:
  typedef ThreadImpl::Callback Callback;
  typedef ThreadImpl::ID ID;

  Thread() : impl_(std::make_shared<ThreadImpl>()) {}

  Thread(Thread&& other) noexcept : impl_(std::move(other.impl_)) {}

  Thread& operator=(Thread&& other) noexcept {
    impl_ = std::move(other.impl_);
    return *this;
  }

  template <typename Function, typename... Args>
  Thread& Run(Function&& f, Args&&... args) & {
    impl_->Run(&Thread::ThreadMain<Function, Args...>, impl_,
               std::forward<Function>(f), std::forward<Args>(args)...);

    return *this;
  }

  template <typename Function, typename... Args>
  Thread&& Run(Function&& f, Args&&... args) && {
    impl_->Run(&Thread::ThreadMain<Function, Args...>, impl_,
               std::forward<Function>(f), std::forward<Args>(args)...);

    return std::move(*this);
  }

  void Join() {
    impl_->Join();
  }

  bool Joinable() {
    return impl_->Joinable();
  }

  ID GetID() {
    return impl_->GetID();
  }

  void Interrupt() {
    impl_->Interrupt();
  }

  bool IsInterrupted() {
    return impl_->IsInterrupted();
  }

  void AtExit(const Callback& cb) {
    impl_->AtExit(cb);
  }

 private:
  template <class Function, class... Args>
  static void ThreadMain(std::shared_ptr<ThreadImpl> impl_ptr, Function&& f,
                         Args&&... args) {
    s_self = impl_ptr.get();
    assert(impl_ptr);
    f(std::forward<Args>(args)...);

    for (auto& cb : impl_ptr->atexit_list_) {
      cb();
    }

    impl_ptr->atexit_list_.clear();
    impl_ptr.reset();
  }

  std::shared_ptr<ThreadImpl> impl_;
  static thread_local ThreadImpl* s_self;
  static thread_local AtThreadExitCaller s_at_thread_exit;

  friend bool this_thread::IsInterrupted();
  friend void this_thread::Interrupt();

  template <typename Callback>
  friend void this_thread::AtExit(Callback&& cb);
};

namespace this_thread {

template <typename Callback>
inline void AtExit(Callback&& cb) {
  if (Thread::s_self) {
    return Thread::s_self->AtExit(cb);
  } else {
    Thread::s_at_thread_exit.AtExit(cb);
  }
}

inline void Yield() {
  std::this_thread::yield();
}

inline bool IsInterrupted() {
  assert(Thread::s_self);
  return Thread::s_self->IsInterrupted();
}

inline void Interrupt() {
  assert(Thread::s_self);
  return Thread::s_self->Interrupt();
}

inline Thread::ID GetID() {
  return std::this_thread::get_id();
}

template <class Rep, class Period>
static void SleepFor(const std::chrono::duration<Rep, Period>& sleep_duration) {
  std::this_thread::sleep_for(sleep_duration);
}

template <class Clock, class Duration>
static void SleepUntil(
    const std::chrono::time_point<Clock, Duration>& sleep_time) {
  std::this_thread::sleep_until(sleep_time);
}

inline unsigned int HardwareConcurrency() noexcept {
  return std::thread::hardware_concurrency();
}

}  // namespace this_thread

}  // namespace mcast

#endif  // CAST_THREAD_H_