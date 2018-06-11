#ifndef CASTC_IRCULAR_BUFFER_H
#define CASTC_IRCULAR_BUFFER_H

#include "stddef.h"

#include <cassert>
#include <memory>
#include <type_traits>

#include "Noncopyable.h"
#include "ObjectCache.h"
#include "util.h"
#include "util_config.h"

namespace mcast {

template <typename T>
class CircularBuffer : public Noncopyable {
  typedef typename std::aligned_storage<sizeof(T), sizeof(T)>::type
      AlignedObjectType;

  static const int kObjectSize = sizeof(AlignedObjectType);
  static const int kBlockNumObjects =
      (kObjectSize * 32) <= kPageSize ? kPageSize / kObjectSize : 32;

  class Iterator {
   public:
    typedef T value_type;
    typedef T& reference;
    typedef T* pointer;
    typedef size_t size_type;
    typedef std::bidirectional_iterator_tag iterator_category;

    Iterator() = default;
    explicit Iterator(size_type i, CircularBuffer* buf) noexcept
        : index(i), buffer(buf) {}

    reference operator*() { return *buffer->Get(index); }

    pointer operator->() { return buffer->Get(index); }

    Iterator& operator++() {
      ++index;
      return *this;
    }

    Iterator operator++(int) { return Iterator(index--, buffer); }

    Iterator& operator--() {
      --index;
      return *this;
    }

    Iterator operator--(int) { return Iterator(index--, buffer); }

    friend bool operator==(const Iterator lhs, const Iterator rhs) {
      return lhs.buffer == rhs.buffer && lhs.index == rhs.index;
    }

    friend bool operator!=(const Iterator lhs, const Iterator rhs) {
      return !(lhs == rhs);
    }

   private:
    size_type index = 0;
    CircularBuffer* buffer = nullptr;
  };

  class ConstIterator {
   public:
    typedef T value_type;
    typedef const T& reference;
    typedef const T* pointer;
    typedef size_t size_type;
    typedef std::bidirectional_iterator_tag iterator_category;

    ConstIterator() = default;
    explicit ConstIterator(size_type i, const CircularBuffer* buf) noexcept
        : index(i), buffer(buf) {}

    reference operator*() const { return *buffer->Get(index); }

    pointer operator->() const { return buffer->Get(index); }

    ConstIterator& operator++() {
      ++index;
      return *this;
    }

    ConstIterator operator++(int) { return Iterator(index++, buffer); }

    ConstIterator& operator--() {
      --index;
      return *this;
    }

    ConstIterator operator--(int) { return Iterator(index--, buffer); }

    friend bool operator==(const ConstIterator lhs, const ConstIterator rhs) {
      return lhs.buffer == rhs.buffer && lhs.index == rhs.index;
    }

    friend bool operator!=(const ConstIterator lhs, const ConstIterator rhs) {
      return !(lhs == rhs);
    }

   private:
    size_type index = 0;
    const CircularBuffer* buffer = nullptr;
  };

 public:
  typedef T value_type;
  typedef size_t size_type;
  typedef Iterator iterator;
  typedef ConstIterator const_iterator;

  explicit CircularBuffer(size_type cap = kBlockNumObjects) noexcept
      : size_(0), capacity_(PowOfTwo(cap)), begin_(0), end_(0) {
    assert(capacity_ > 0);
    vector_ = NewBlock(capacity_);
  }

  ~CircularBuffer() {
    Clear();
    DeleteBlock(vector_);
  }

  iterator begin() noexcept { return Iterator(begin_, this); }

  iterator end() noexcept { return Iterator(end_, this); }

  const_iterator cbegin() const noexcept {
    return const_iterator(begin_, this);
  }

  const_iterator cend() const noexcept { return const_iterator(end_, this); }

  size_type BeginPosition() const { return GetIndex(begin_); }

  size_type EndPosition() const { return GetIndex(end_); }

  void PushBack(const value_type& x) {
    if (size_ == capacity_) {
      Grow(capacity_ * 2);
      assert(size_ < capacity_);
    }

    Construct(Get(end_), x);
    ++end_;
    ++size_;
  }

  void PushBack(value_type&& x) {
    if (size_ == capacity_) {
      Grow(capacity_ * 2);
      assert(size_ < capacity_);
    }

    Construct(Get(end_), std::move(x));
    ++end_;
    ++size_;
  }

  value_type& Back() noexcept {
    assert(size_ > 0);
    value_type* obj = Get(end_ - 1);
    return *obj;
  }

  value_type& Front() noexcept {
    assert(size_ > 0);
    value_type* obj = Get(begin_);
    return *obj;
  }

  void PopFront() noexcept {
    assert(size_ > 0);
    value_type* obj = Get(begin_);
    Destroy(obj);
    ++begin_;
    --size_;
  }

  bool Empty() noexcept { return Size() == 0; }

  size_type Size() noexcept { return size_; }

  size_type capacity() noexcept { return capacity_; }

  void Clear() noexcept {
    size_type first = begin_;
    while (size_ > 0) {
      Destroy(Get(first));
      ++first;
      --size_;
    }

    begin_ = end_ = 0;
  }

 private:
  size_type GetIndex(size_type p) const { return p & (capacity_ - 1); }

  value_type* Get(size_type i) const {
    return reinterpret_cast<value_type*>(&vector_[GetIndex(i)]);
  }

  void Construct(void* ptr, value_type&& value) noexcept {
    new (ptr) value_type(std::move(value));
  }

  void Construct(void* ptr, const value_type& value) {
    new (ptr) value_type(value);
  }

  void Destroy(value_type* ptr) { ptr->~value_type(); }

  AlignedObjectType* NewBlock(size_t size) {
    AlignedObjectType* ptr = new AlignedObjectType[size];
    return ptr;
  }

  void DeleteBlock(AlignedObjectType* ptr) { delete[] ptr; }

  void Grow(size_type const new_cap) {
    assert(new_cap > capacity_);

    AlignedObjectType* new_vector = NewBlock(new_cap);
    for (size_type i = 0; i < size_; ++i, ++begin_) {
      // nothrow
      Construct(&new_vector[i], std::move(*Get(begin_)));
    }

    auto old_vector = vector_;
    vector_ = new_vector;
    begin_ = 0;
    end_ = begin_ + size_;
    capacity_ = new_cap;

    DeleteBlock(old_vector);
  }

  size_type size_;
  size_type capacity_;

  size_type begin_;
  size_type end_;

  AlignedObjectType* vector_;
};
}  // namespace mcast

#endif  // CAST_CIRCULAR_BUFFER_H