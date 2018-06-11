#ifndef CAST_THREADSAFE_QUEUE_H
#define CAST_THREADSAFE_QUEUE_H

#include <deque>
#include <list>
#include <mutex>
#include <atomic>

#include "Noncopyable.h"
#include "util_config.h"

namespace mcast {

template <typename T>
class alignas(kCacheLineSize) ThreadSafeQueue : Noncopyable {
 public:
  typedef typename std::deque<T>::size_type size_type;

  ThreadSafeQueue() noexcept {}

  ThreadSafeQueue(ThreadSafeQueue &&q) noexcept : queue_(std::move(q.queue_)) {}

  ThreadSafeQueue &operator=(ThreadSafeQueue &&rhs) noexcept {
    queue_.swap(rhs.queue_);
    return *this;
  }

  void push(const T &x) {
    std::lock_guard<std::mutex> lg(mutex_);
    queue_.push_back(x);
  }

  void push(T &&x) {
    std::lock_guard<std::mutex> lg(mutex_);
    queue_.push_back(std::move(x));
  }

  size_type pushAndFetchSize(const T &x) {
    std::lock_guard<std::mutex> lg(mutex_);
    queue_.push_back(x);
    return queue_.size();
  }

  size_type pushAndFetchSize(T &&x) {
    std::lock_guard<std::mutex> lg(mutex_);
    queue_.push_back(std::move(x));
    return queue_.size();
  }

  T *front() {
    std::lock_guard<std::mutex> lg(mutex_);
    if (queue_.empty())
      return nullptr;

    return &queue_.front();
  }

  void pop() {
    std::lock_guard<std::mutex> lg(mutex_);
    if (queue_.empty())
      return;

    queue_.pop_front();
  }

  size_type popAndFetchSize() {
    std::lock_guard<std::mutex> lg(mutex_);
    if (queue_.empty())
      return 0;

    queue_.pop_front();
    return queue_.size();
  }

  bool pop(T *x) {
    std::lock_guard<std::mutex> lg(mutex_);
    if (queue_.empty())
      return false;

    *x = std::move(queue_.front());
    queue_.pop_front();
    return true;
  }

  bool empty() {
    std::lock_guard<std::mutex> lg(mutex_);
    return queue_.empty();
  }

  bool empty_unsyn() {
    std::atomic_thread_fence(std::memory_order_seq_cst);
    return queue_.empty();
  }

  size_type size() {
    std::lock_guard<std::mutex> lg(mutex_);
    return queue_.size();
  }

  size_type size_unsyn() {
    std::atomic_thread_fence(std::memory_order_seq_cst);
    return queue_.size();
  }

  void clear() {
    std::lock_guard<std::mutex> lg(mutex_);
    queue_.clear();
  }

 private:
  std::mutex mutex_;
  std::deque<T> queue_;
};

}  // namespace mcast

#endif  // CAST_THREADSAFE_QUEUE_H