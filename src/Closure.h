#pragma once

#include <functional>

#include "util/Noncopyable.h"

namespace mcast {

class ClosureI : Noncopyable {
 public:
  virtual ~ClosureI() {}
  virtual void Run() = 0;
};

typedef std::function<void()> CallClosure;

//class CallClosure : ClosureI {
// public:
//  typedef std::function<void()> ClosureFunc;
//
//  CallClosure() = default;
//  explicit CallClosure(const ClosureFunc& func) : func_(func) {}
//
//  void Run() override {
//    func_();
//  }
//
// private:
//  template <typename Func>
//  void SetClosure(Func&& f) {
//    func_ = std::move(f);
//  }
//
// private:
//  ClosureFunc func_;
//};

}  // namespace mcast
