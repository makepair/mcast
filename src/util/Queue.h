#ifndef CAST_QUEUE_H
#define CAST_QUEUE_H

#include "stddef.h"

#include <cassert>
#include <memory>
#include <mutex>
#include <type_traits>
#include <vector>

#include <pthread.h>

#include "CircularBuffer.h"
#include "Noncopyable.h"
#include "ObjectCache.h"
#include "util.h"
#include "util_config.h"

namespace mcast {

template <typename T>
inline constexpr int EstimateNumOfObjectsPerBlock() {
  typedef typename std::aligned_storage<sizeof(T), sizeof(T)>::type
      AlignedObjectType;
  const int aligned_objsize = sizeof(AlignedObjectType);

  return (aligned_objsize * 32) <= kPageSize ? kPageSize / aligned_objsize : 32;
}

template <typename T,
          size_t NumOfObjectsPerBlock = EstimateNumOfObjectsPerBlock<T>()>
class Queue : public Noncopyable {
  typedef typename std::aligned_storage<sizeof(T), sizeof(T)>::type
      AlignedObjectType;

  static const int kObjectSize = sizeof(AlignedObjectType);
  typedef CircularBuffer<AlignedObjectType*> MapType;

  struct Iterator {
    Iterator() = default;
    explicit Iterator(AlignedObjectType* p)
        : pos(p), end(p + NumOfObjectsPerBlock) {
    }

    AlignedObjectType* pos = nullptr;
    AlignedObjectType* end = nullptr;
  };

  typedef std::mutex Mutex;

 public:
  typedef T value_type;
  typedef size_t size_type;
  static const int kNumOfObjectsPerBlock = NumOfObjectsPerBlock;

  Queue() : map_(1), size_(0) {
    map_.PushBack(NewBlock());
    begin_ = Iterator(*map_.begin());
    end_ = begin_;
  }

  ~Queue() {
    Clear();
  }

  void Push(const value_type& x) {
    if (__builtin_expect(end_.pos >= end_.end, 0)) {
      map_.PushBack(NewBlock());
      end_ = Iterator(map_.Back());
    }

    Construct(end_.pos, x);
    ++end_.pos;
    ++size_;
  }

  void Push(value_type&& x) {
    if (__builtin_expect(end_.pos >= end_.end, 0)) {
      map_.PushBack(NewBlock());
      end_ = Iterator(map_.Back());
    }

    Construct(end_.pos, std::move(x));
    ++end_.pos;
    ++size_;
  }

  value_type& Front() noexcept {
    return (*reinterpret_cast<value_type*>(begin_.pos));
  }

  void Pop() noexcept {
    if (__builtin_expect(begin_.pos >= begin_.end, 0)) {
      DeleteBlock(map_.Front());
      map_.PopFront();
      begin_ = Iterator(map_.Front());
    }

    Destroy(begin_.pos);
    ++begin_.pos;
    --size_;
  }

  bool Empty() noexcept {
    return Size() == 0;
  }

  size_type Size() noexcept {
    return size_;
  }

  size_type Capacity() noexcept {
    return map_.Size() * kNumOfObjectsPerBlock;
  }

  void Clear() noexcept {
    for (auto it = begin_.pos; it != begin_.end; ++it) {
      Destroy(it);
    }

    for (auto map_it = ++map_.begin(); map_it != map_.end(); ++map_it) {
      for (auto it = *map_it;
           it != *map_it + kNumOfObjectsPerBlock && it != end_.pos; ++it) {
        Destroy(it);
      }

      DeleteBlock(map_.Front());
      map_.PopFront();
    }

    size_ = 0;
    begin_ = Iterator(map_.Front());
    end_ = begin_;
  }

 private:
  void Construct(void* ptr, value_type&& value) noexcept {
    new (ptr) value_type(std::move(value));
  }

  void Construct(void* ptr, const value_type& value) noexcept {
    new (ptr) value_type(value);
  }

  void Destroy(void* ptr) {
    value_type* objptr = reinterpret_cast<value_type*>(ptr);
    objptr->~value_type();
  }

  AlignedObjectType* NewBlock() {
    void* ptr =
        MemCache<kObjectSize * kNumOfObjectsPerBlock>::singleton().get();
    AlignedObjectType* obj_ptr = reinterpret_cast<AlignedObjectType*>(ptr);
    return obj_ptr;
  }

  void DeleteBlock(AlignedObjectType* ptr) {
    MemCache<kObjectSize * kNumOfObjectsPerBlock>::singleton().put(ptr);
  }

  Iterator begin_;
  Iterator end_;

  MapType map_;
  size_type size_;
};
}

#endif
