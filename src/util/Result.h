#ifndef CAST_RESULT_H_
#define CAST_RESULT_H_

#include <utility>

#include "Status.h"

namespace mcast {

template <typename T>
class Result {
 public:
  Result() = default;                            // OK
  /*explicit*/ Result(const T& x) : data_(x) {}  // OK
  Result(T&& x) : data_(std::move(x)) {}         // OK

  Result(const Status& x) : state_(x) {}
  Result(Status&& x) : state_(std::move(x)) {}

  const T& get() const {
    assert(state_);
    return data_;
  }

  void set(const T& x) {
    assert(state_);
    data_ = x;
  }

  explicit operator bool() const {
    return state_.IsOK();
  }

  const Status& status() const {
    return state_;
  }

  void status(const Status& s) {
    state_ = s;
  }

 private:
  Status state_;
  T data_{};
};

}  // namespace mcast

#endif  // CAST_RESULT_H_
