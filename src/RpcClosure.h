#pragma once

#include "google/protobuf/service.h"

#include "Closure.h"

namespace mcast {

class NullClosure : public google::protobuf::Closure {
 public:
  NullClosure() {}

  static NullClosure& Instance() {
    static NullClosure null_closure;
    return null_closure;
  }

  virtual void Run() override {}
};

class RpcClosure : public google::protobuf::Closure {
 public:
  RpcClosure() = default;
  explicit RpcClosure(const CallClosure& cc) : call_closure_(cc) {}

  void Run() override {
    assert(call_closure_);
    call_closure_();
    delete this;
  }

 private:
  CallClosure call_closure_;
};

}  // namespace mcast
