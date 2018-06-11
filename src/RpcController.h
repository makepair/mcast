#pragma once

#include "google/protobuf/service.h"

#include "util/Status.h"

namespace mcast {

class RpcController : public google::protobuf::RpcController {
 public:
  // Client-side methods ---------------------------------------------
  virtual void Reset() { status_ = Status::OK(); }

  virtual bool Failed() const { return !status_.IsOK(); }

  // If Failed() is true, returns a human-readable description of the error.
  virtual std::string ErrorText() const {
    assert(!status_);
    return status_.ErrorText();
  }

  virtual void StartCancel() { assert(false); }

  // Server-side methods ---------------------------------------------
  virtual void SetFailed(const std::string& reason) {
    status_ = Status(kFailed, reason);
  }

  virtual bool IsCanceled() const {
    assert(false);
    return false;
  }

  virtual void NotifyOnCancel(google::protobuf::Closure*) { assert(false); }

 private:
  Status status_;
};


}
