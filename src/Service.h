#ifndef CAST_SERVICE_H_
#define CAST_SERVICE_H_

#include <stdint.h>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "Message.h"
#include "ServiceContext.h"
#include "ServiceHandle.h"
#include "util/Noncopyable.h"
#include "util/Status.h"

namespace mcast {

class System;

class Service : public Noncopyable {
 public:
  typedef ServiceHandle Handle;
  template <typename T>
  using BasicHandle = BasicHandle<T>;

  typedef std::shared_ptr<Service> ServicePtr;

  enum Type {
    kNone = 1,
    kUserThreadService = 2,
    kMessageDrivenService = 4,

    // kMethodCallService is derived from kMessageDrivenService,
    // kMethodCallService & kMessageDrivenService == true
    kMethodCallService = 12,
  };

  Service() = default;
  Service(System* sys, const std::string& name) : system_(sys), name_(name) {}
  Service(Service&& s) = default;
  virtual ~Service();
  Service& operator=(Service&& s) = default;

  template <typename D>
  BasicHandle<D> GetHandle() {
    auto d_ptr = dynamic_cast<D*>(this);
    if (d_ptr) {
      return BasicHandle<D>(handle_.index());
    } else {
      return BasicHandle<D>();
    }
  }

  void handle(const Handle& h) {
    handle_ = h;
  }

  const Handle& handle() const {
    return handle_;
  }

  const std::string& name() const {
    return name_;
  }

  System* system() const {
    return system_;
  }

  void Yield();
  Status Sleep(uint32_t milliseconds);

  Status WaitSignal();
  Status WaitInput(int fd);
  Status WaitOutput(int fd);

  virtual Type ServiceType() const {
    assert(false);
    return kNone;
  }

  void Stop();
  bool IsStopping();

 protected:
  virtual void OnServiceStart() {}
  virtual void OnServiceStop() {}

 private:
  friend class System;

  ServiceContext* context() const {
    return context_.get();
  }

  void SetContext(const std::shared_ptr<ServiceContext>& contxt) {
    context_ = contxt;
  }

  std::shared_ptr<ServiceContext> context_;
  System* system_ = nullptr;
  Handle handle_;
  std::string name_;
};

typedef Service::ServicePtr ServicePtr;

template <typename T>
using BasicServicePtr = std::unique_ptr<T>;

class MessageDrivenService : public Service {
 public:
  using Service::Service;

  virtual void HandleMessage(const MessagePtr& msg) = 0;

  Type ServiceType() const override {
    return kMessageDrivenService;
  }
};

class MethodCallService : public MessageDrivenService {
 public:
  using MessageDrivenService::MessageDrivenService;

  void HandleMessage(const MessagePtr& msg) override {
    CHECK(msg);
    if (msg->type() == Message::kCallMethod) {
      Dispatch(msg);
    } else {
      HandleNotMethodCallMessage(msg);
    }
  }

  Type ServiceType() const override {
    return kMethodCallService;
  }

 protected:
  void Dispatch(const MessagePtr& msg) {
    assert(msg && msg->type() == Message::kCallMethod);
    // FIXME: remove dynamic_cast
    MethodCallMessage* mmsg = dynamic_cast<MethodCallMessage*>(msg.get());
    CHECK(mmsg);
    mmsg->CallMethod(this);
    mmsg->Done(Status::OK());
  }

  virtual void HandleNotMethodCallMessage(const MessagePtr& msg) {
    CHECK(false);
    CHECK(msg && msg->type() != Message::kCallMethod);
    msg->Done(Status(kFailed, "HandleNotMethodCallMessage"));
  }
};

class UserThreadService : public Service {
 public:
  using Service::Service;

  Type ServiceType() const override {
    return kUserThreadService;
  }

  virtual void Main() = 0;
};

}  // namespace mcast

#endif  // CAST_SERVICE_H
