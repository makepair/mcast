#ifndef CAST_BLOCKINGQUEUE_H_
#define CAST_BLOCKINGQUEUE_H_

#include <condition_variable>
#include <mutex>
#include <queue>

#include "Noncopyable.h"
#include "Thread.h"
#include "util_config.h"

namespace mcast {

template <typename T>
class alignas(kCacheLineSize) BlockingQueue : Noncopyable {
 public:
  typedef typename std::queue<T>::size_type size_type;

  void push(const T &x) {
    std::lock_guard<std::mutex> lg(mutex_);
    queue_.push(x);
    queue_.size() > 1 ? cond_.notify_all() : cond_.notify_one();
  }

  size_type pushAndFetchSize(const T &x) {
    std::lock_guard<std::mutex> lg(mutex_);
    queue_.push(x);
    queue_.size() > 1 ? cond_.notify_all() : cond_.notify_one();
    return queue_.size();
  }

  bool pop(T *x) {
    std::unique_lock<std::mutex> ul(mutex_);
    cond_.wait(ul, [this]() -> bool {
      return !queue_.empty() || this_thread::IsInterrupted();
    });

    if (this_thread::IsInterrupted())
      return false;

    *x = std::move(queue_.front());
    queue_.pop();
    return true;
  }

  void wakeAll() {
    std::unique_lock<std::mutex> ul(mutex_);
    cond_.notify_all();
  }

  bool empty() {
    std::lock_guard<std::mutex> lg(mutex_);
    return queue_.empty();
  }

  typename std::queue<T>::size_type size() {
    return queue_.size();
  }

 private:
  std::mutex mutex_;
  std::queue<T> queue_;
  std::condition_variable cond_;
};

}  // namespace mcast

#endif  // CAST_BLOCKINGQUEUE_H_