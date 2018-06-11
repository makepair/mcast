#ifndef CAST_SERVICE_HANDLE_H_
#define CAST_SERVICE_HANDLE_H_

#include <stdint.h>

#include <functional>
#include <memory>
#include <ostream>
#include <utility>

namespace mcast {

class Service;

template <typename T>
class BasicHandle {
  template <typename D>
  friend class BasicHandle;

 public:
  typedef int64_t IndexType;
  typedef T Type;

  BasicHandle() = default;
  BasicHandle(const BasicHandle&) = default;
  BasicHandle(BasicHandle&&) = default;

  explicit BasicHandle(const IndexType& i) noexcept : index_(i) {}

  template <typename D>
  BasicHandle(const BasicHandle<D>& h) noexcept : index_(h.index_) {}

  template <typename D>
  BasicHandle(BasicHandle<D>&& h) noexcept : index_(h.index_) {
    h.index_ = -1;
  }

  BasicHandle& operator=(const BasicHandle&) = default;
  BasicHandle& operator=(BasicHandle&&) = default;

  template <typename D>
  BasicHandle& operator=(const BasicHandle<D>& h) noexcept {
    index_ = h.index_;
    return *this;
  }

  template <typename D>
  BasicHandle& operator=(BasicHandle<D>&& h) noexcept {
    index_ = h.index_;
    h.index_ = -1;
    return *this;
  }

  IndexType index() const {
    return index_;
  }

  void index(IndexType i) {
    index_ = i;
  }

  explicit operator bool() const {
    return index_ != -1;
  }

  // friend std::basic_ostream<char>& operator<<(std::basic_ostream<char>& is,
  //                                             const BasicHandle& h) {
  //   is << h.index();
  //   return is;
  // }

  friend bool operator==(const BasicHandle& lh, const BasicHandle& rh) {
    return lh.index() == rh.index();
  }

  friend bool operator!=(const BasicHandle& lh, const BasicHandle& rh) {
    return lh.index() != rh.index();
  }

 private:
  IndexType index_ = -1;
};

typedef BasicHandle<Service> ServiceHandle;

}  // namespace mcast

#endif  // CAST_SERVICE_HANDLE_H_
