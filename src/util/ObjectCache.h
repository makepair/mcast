#ifndef CAST_OBJECTCACHE_H_
#define CAST_OBJECTCACHE_H_

#include <stddef.h>
#include <stdint.h>

#include <atomic>
#include <cassert>
#include <cstdlib>
#include <deque>
#include <functional>
#include <list>
#include <mutex>
#include <vector>

#include "Noncopyable.h"
#include "Thread.h"
#include "util_config.h"

namespace mcast {

inline constexpr int AlignedObjectSize(const int objsize, const int alignment) {
  const int align_mask = alignment - 1;
  const int align_objsize = (objsize + align_mask) & ~align_mask;

  return align_objsize;
}

inline constexpr int EstimateBufferSize(const int align_objsize,
                                        const int /*alignment*/) {
  int buf_size = kPageSize;
  int num = align_objsize < buf_size ? buf_size / align_objsize : 16;
  if (num < 16)
    num = 16;

  return (num * align_objsize + kPageSize - 1) / kPageSize * kPageSize;
}

inline constexpr int EstimateNumOfBufferObjects(const int buf_size,
                                                const int align_objsize,
                                                const int /*alignment*/) {
  const int num = buf_size / align_objsize;
  return num;
}

template <int AlignedObjectSize, int Alignment = kWordSize,
          int NumCacheObjects = kPageSize / kPointerSize,
          int BufferSize = EstimateBufferSize(AlignedObjectSize, Alignment)>
class MemCache : public Noncopyable {
  static_assert(Alignment > 0, "Alignment <= 0");
  static_assert(NumCacheObjects > 0, "NumCacheObjects <= 0");
  static_assert(BufferSize >= AlignedObjectSize,
                "BufferSize < AlignedObjectSize");

  struct Cache {
    static constexpr int NumObjects = NumCacheObjects;

    size_t size;
    void* objects[NumObjects];
  };

  struct Buffer {
    static const int NumObjects =
        EstimateNumOfBufferObjects(BufferSize, AlignedObjectSize, Alignment);

    static_assert(NumObjects > 0, "NumObjects == 0");

    size_t size;
    uint8_t buffer[BufferSize];

    void* get(size_t i) {
      return buffer + i * AlignedObjectSize;
    }
  };

  static const int kNumBufferObjects = Buffer::NumObjects;

 public:
  static const int kAlignedObjectSize = AlignedObjectSize;
  static const int kNumCacheObjects = NumCacheObjects;
  static const int kBufferSize = BufferSize;
  static const int kAlignment = Alignment;

  static MemCache& singleton() {
    auto ptr = singleton_ptr.load(std::memory_order_acquire);
    if (__builtin_expect(ptr != nullptr, 1)) {
      return *ptr;
    }
    {
      std::lock_guard<std::mutex> gl(singleton_ptr_mutex_);
      ptr = singleton_ptr.load(std::memory_order_relaxed);
      if (ptr) {
        return *ptr;
      }

      ptr = new MemCache();
      singleton_ptr.store(ptr, std::memory_order_release);
    }
    std::atexit(destory);
    return *ptr;
  }

  void* get();
  void put(void* obj_ptr);

 private:
  static void destory() {
    assert(singleton_ptr);
    delete singleton_ptr;
  }

  static void OnThreadExit() {
    auto& this_thread_cache = this_thread_data_.cache;
    delete this_thread_cache;
    this_thread_cache = nullptr;

    auto& this_thread_buffer = this_thread_data_.cache;
    delete this_thread_buffer;
    this_thread_buffer = nullptr;
  }

  MemCache() {
  }
  ~MemCache() {
    {
      std::lock_guard<std::mutex> gl(cache_mutex_);
      for (Cache* c : cache_list_) {
        delete c;
      }

      cache_list_.clear();
    }

    std::lock_guard<std::mutex> gl(buffer_mutex_);
    for (Buffer* buf : buffer_list_) {
      delete buf;
    }
    buffer_list_.clear();
  }

  Cache* AllocCache() {
    Cache* cache = new Cache();
    cache->size = 0;
    return cache;
  }

  Buffer* AllocBuffer() {
    Buffer* buf = new Buffer();
    buf->size = 0;
    return buf;
  }

  bool IsCacheListEmpty_Unsyn() {
    std::atomic_thread_fence(std::memory_order_seq_cst);
    return cache_list_.empty();
  }

  std::mutex cache_mutex_;
  std::vector<Cache*> cache_list_;

  std::mutex buffer_mutex_;
  std::vector<Buffer*> buffer_list_;

  struct ThreadLocalData {
    Cache* cache;
    Buffer* buffer;
  };

  thread_local static ThreadLocalData this_thread_data_;

  static std::mutex singleton_ptr_mutex_;
  static std::atomic<MemCache*> singleton_ptr;
};

template <int AlignedObjectSize, int Alignment, int kNumCacheObjects,
          int BufferSize>
std::mutex MemCache<AlignedObjectSize, Alignment, kNumCacheObjects,
                    BufferSize>::singleton_ptr_mutex_;

template <int AlignedObjectSize, int Alignment, int kNumCacheObjects,
          int BufferSize>
std::atomic<
    MemCache<AlignedObjectSize, Alignment, kNumCacheObjects, BufferSize>*>
    MemCache<AlignedObjectSize, Alignment, kNumCacheObjects,
             BufferSize>::singleton_ptr{nullptr};

template <int AlignedObjectSize, int Alignment, int kNumCacheObjects,
          int BufferSize>
thread_local typename MemCache<AlignedObjectSize, Alignment, kNumCacheObjects,
                               BufferSize>::ThreadLocalData
    MemCache<AlignedObjectSize, Alignment, kNumCacheObjects,
             BufferSize>::this_thread_data_{nullptr, nullptr};

template <int AlignedObjectSize, int Alignment, int kNumCacheObjects,
          int BufferSize>
void* MemCache<AlignedObjectSize, Alignment, kNumCacheObjects,
               BufferSize>::get() {
  auto& this_thread_cache = this_thread_data_.cache;
  if (__builtin_expect(nullptr == this_thread_cache, 0)) {
    this_thread_cache = AllocCache();
    this_thread::AtExit(&MemCache::OnThreadExit);
  }

  if (this_thread_cache->size > 0) {
    void* ptr = this_thread_cache->objects[--this_thread_cache->size];
    return ptr;  // path 1
  }

  Cache* new_cache_queue = nullptr;
  {
    if (!IsCacheListEmpty_Unsyn()) {
      std::lock_guard<std::mutex> gl(cache_mutex_);
      if (!cache_list_.empty()) {
        new_cache_queue = cache_list_.back();
        cache_list_.pop_back();
      }
    }
  }

  if (new_cache_queue) {
    delete this_thread_cache;

    assert(new_cache_queue->size == kNumCacheObjects);
    void* ptr = new_cache_queue->objects[--new_cache_queue->size];
    this_thread_cache = new_cache_queue;
    return ptr;  // path 2
  }

  auto& this_thread_buffer = this_thread_data_.buffer;
  if (__builtin_expect(this_thread_buffer == nullptr, 0)) {
    this_thread_buffer = AllocBuffer();
  }

  if (this_thread_buffer->size < this_thread_buffer->NumObjects) {
    void* ptr = this_thread_buffer->get(this_thread_buffer->size++);
    return ptr;  // path 3
  }

  {
    std::lock_guard<std::mutex> gl(buffer_mutex_);
    buffer_list_.push_back(this_thread_buffer);
  }

  this_thread_buffer = AllocBuffer();
  void* ptr = this_thread_buffer->get(this_thread_buffer->size++);
  return ptr;  // path 4
}

template <int AlignedObjectSize, int Alignment, int kNumCacheObjects,
          int BufferSize>
void MemCache<AlignedObjectSize, Alignment, kNumCacheObjects, BufferSize>::put(
    void* obj_ptr) {
  auto& this_thread_cache = this_thread_data_.cache;
  if (__builtin_expect(nullptr == this_thread_cache, 0)) {
    this_thread_cache = AllocCache();
    this_thread::AtExit(&MemCache::OnThreadExit);
  }

  if (this_thread_cache &&
      this_thread_cache->size < this_thread_cache->NumObjects) {
    this_thread_cache->objects[this_thread_cache->size++] = obj_ptr;  // path 1
    return;
  }

  {
    std::lock_guard<std::mutex> gl(cache_mutex_);
    cache_list_.push_back(this_thread_cache);
  }

  this_thread_cache = AllocCache();
  this_thread_cache->objects[this_thread_cache->size++] = obj_ptr;  // path 2
}

template <typename T, int Alignment = kWordSize,
          int NumCacheObjects = kPageSize / kPointerSize,
          int BufferSize = EstimateBufferSize(
              AlignedObjectSize(sizeof(T), Alignment), Alignment)>
class ObjectCache : public Noncopyable {
 public:
  typedef MemCache<AlignedObjectSize(sizeof(T), Alignment), Alignment,
                   NumCacheObjects, BufferSize>
      MemCacheType;

  static const int kAlignedObjectSize = MemCacheType::kAlignedObjectSize;
  static const int kNumCacheObjects = MemCacheType::kNumCacheObjects;
  static const int kBufferSize = MemCacheType::kBufferSize;
  static const int kAlignment = MemCacheType::kAlignment;

  template <typename... Args>
  static T* get(Args&&... args) {
    void* ptr = MemCacheType::singleton().get();
    new (ptr) T(std::forward<Args>(args)...);
    return reinterpret_cast<T*>(ptr);
  }

  static void put(T* obj_ptr) {
    obj_ptr->~T();
    MemCacheType::singleton().put(obj_ptr);
  }
};
}

#endif  // CAST_OBJECTCACHE_H_
