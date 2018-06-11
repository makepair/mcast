#ifndef CAST_STATUS_H_
#define CAST_STATUS_H_

#include <stdint.h>

#include <cassert>
#include <cstring>
#include <iostream>
#include <string>

#include "Noncopyable.h"

namespace mcast {

enum State {
  kOK = 0,
  kInvailArgument,
  kNotFound,
  kFailed,
  kInterrupt,
  kEof,
  kAgain,
  kStateCount
  //@note need to add log for ostream <<
};

// Status provides a utility to specify the state of the method call, successful
// or failed.
// This utility is inspired by leveldb::Status.

class Status {
 public:
  Status() = default;  // kOk

  explicit Status(State s) : Status(s, "", 0) {}

  explicit Status(State s, const std::string& msg) : Status(s, msg.c_str(), msg.size()) {}

  template <unsigned int Size>
  explicit Status(State s, char (&msg)[Size]) : Status(s, &msg, Size) {}

  explicit Status(State s, const char* msg, const size_t size) {
    assert(s != kOK);

    state_ = new char[size + 2];
    state_[0] = static_cast<char>(s);

    assert(size <= 255);
    state_[1] = static_cast<char>(size);
    std::memcpy(state_ + 2, msg, size);
  }

  Status(const Status& s) {
    if (s.IsOK()) {
      assert(state_ == nullptr);
      return;
    }

    size_t size = s.ErrorTextSize() + 2;
    state_ = new char[size];
    std::memcpy(state_, s.state_, size);
  }

  Status(Status&& s) : state_(s.state_) {
    s.state_ = nullptr;
  }

  ~Status() {
    delete[] state_;
  }

  static Status OK() {
    return Status();
  }

  Status& operator=(const Status& s) {
    Status tmp(s);
    swap(tmp);
    return *this;
  }

  Status& operator=(Status&& s) {
    if (this != &s) {
      delete[] state_;
      state_ = s.state_;
      s.state_ = nullptr;
    }
    return *this;
  }

  void swap(Status& s) {
    std::swap(state_, s.state_);
  }

  bool IsOK() const {
    return state_ == nullptr;
  }

  bool IsInvailArgument() const {
    return StateValue() == kInvailArgument;
  }

  bool IsNotFound() const {
    return StateValue() == kNotFound;
  }

  bool IsInterrupt() const {
    return StateValue() == kInterrupt;
  }

  bool IsFailed() const {
    return StateValue() == kFailed;
  }

  bool IsEof() const {
    return StateValue() == kEof;
  }

  bool IsAgain() const {
    return StateValue() == kAgain;
  }

  std::string ErrorText() const {
    if (IsOK())
      return std::string{};

    size_t size = ErrorTextSize();
    if (0 == size)
      return std::string();
    else
      return std::string(state_ + 2, size);
  }

  explicit operator bool() const {
    return IsOK();
  }

  State StateValue() const {
    if (state_ == nullptr)
      return kOK;

    return static_cast<State>(state_[0]);
  }

 private:
  uint8_t ErrorTextSize() const {
    assert(!IsOK());
    return static_cast<uint8_t>(state_[1]);
  }

  char* state_ = nullptr;
};

inline void swap(Status& lhs, Status& rhs) {
  lhs.swap(rhs);
}

inline std::ostream& operator<<(std::ostream& os, const Status& s) {
  static const char* txt_ary[] = {
      "OK", "InvailArgument", "NotFound", "Failed", "Interrupt", "Eof", "Again",
  };

  assert(s.StateValue() <= sizeof(txt_ary) / sizeof(txt_ary[0]));

  if (s.IsOK())
    os << txt_ary[s.StateValue()];
  else
    os << txt_ary[s.StateValue()] << ":" << s.ErrorText();

  return os;
}

}  // namespace mcast

#endif  // CAST_STATUS_H_
