#ifndef CAST_MPSCQUEUE_H_
#define CAST_MPSCQUEUE_H_

#include <atomic>
#include <cassert>
#include <memory>
#include <type_traits>

#include "Noncopyable.h"
#include "ObjectCache.h"
#include "util_config.h"

namespace mcast {
namespace concurrence {
namespace waitfree {

template <typename T>
class MPSCQueue : public Noncopyable {
 public:
  static_assert(std::is_nothrow_move_constructible<T>::value,
                "T's move constructor must be noexcept");

  static_assert(std::is_nothrow_move_assignable<T>::value,
                "T's move assignment must be noexcept");

  struct Node {
    Node() = default;

    explicit Node(T&& x) noexcept : value(std::move(x)) {}

    explicit Node(const T& x) : value(x) {}

    const T& Get() const noexcept { return value; }

    void Set(const T& x) { value = x; }

   private:
    friend class MPSCQueue;
    std::atomic<Node*> next{nullptr};
    T value{};
  };

  MPSCQueue(Node* stub) : tail_(stub), head_(stub) {}

  ~MPSCQueue() {
    Clear();
    assert(size_ == 0);
    tail_.store(nullptr);
    head_ = nullptr;
  }

  void Push(Node* node) noexcept {
    size_.fetch_add(1, std::memory_order_relaxed);
    Node* old_tail = tail_.exchange(node, std::memory_order_acq_rel);
    old_tail->next.store(node, std::memory_order_release);
  }

  Node* Pop() noexcept {
    Node* next = head_->next.load(std::memory_order_acquire);
    if (next == nullptr)
      return next;

    head_->value = std::move(next->value);
    Node* old_head = head_;
    head_ = next;
    size_.fetch_sub(1, std::memory_order_relaxed);
    return old_head;
  }

  void Clear() {
    while (Pop())
      ;

    assert(size_ == 0);
  }

  bool Empty() { return tail_.load(std::memory_order_relaxed) == head_; }

  size_t Size() { return size_.load(std::memory_order_relaxed); }

 private:
  std::atomic<Node*> tail_;
  alignas(kCacheLineSize) Node* head_;
  std::atomic<size_t> size_{0};
};
}  // namespace waitfree
}  // namespace concurrence
}  // namespace mcast

#endif  // CAST_MPSCQUEUE_H_