#ifndef CAST_MESSAGE_H_
#define CAST_MESSAGE_H_

#include <stdint.h>
#include <cassert>
#include <functional>
#include <memory>
#include <string>
#include <tuple>
#include <utility>

#include "ServiceHandle.h"
#include "util/Noncopyable.h"
#include "util/Status.h"
#include "util/util.h"

namespace mcast {

class Service;

class Message : Noncopyable {
 public:
  typedef ServiceHandle Handle;

  template <typename T>
  using BasicHandle = BasicHandle<T>;

  typedef std::function<void(const Status&)> Closure;

  enum Type { kInvaild = 0, kMessage, KString, kCallMethod };

  Message() {}
  explicit Message(const Handle& src, const Handle& dest) noexcept : source_(src),
                                                                     destination_(dest) {}

  explicit Message(Handle src, Handle dest, const Closure& c)
      : source_(src), destination_(dest), closure_(c) {}

  virtual ~Message() {}

  void Done(const Status& s) {
    if (closure_)
      closure_(s);
  }

  void SetClosure(const Closure& c) {
    closure_ = c;
  }
  virtual Type type() const {
    return kInvaild;
  }

  Handle source() const {
    return source_;
  }
  Handle destination() const {
    return destination_;
  }

 private:
  Handle source_;
  Handle destination_;
  Closure closure_;
};

typedef std::shared_ptr<Message> MessagePtr;

class StringMessage : public Message {
 public:
  explicit StringMessage(const Handle& src, const Handle& dest, const Closure& done,
                         const std::string& msg)
      : Message(src, dest, done), msg_(msg) {}

  Type type() const override {
    return KString;
  }

  void set_msg(const std::string& msg) {
    msg_ = msg;
  }
  const std::string& get_msg() const {
    return msg_;
  }

 private:
  std::string msg_;
};

class MethodCallMessage : public Message {
 public:
  using Message::Message;

  Type type() const override {
    return kCallMethod;
  }

  virtual void CallMethod(Service* s) = 0;
};

template <typename ClassType, typename MemFunc, typename... Args>
class MemberFunctionCallMessage : public MethodCallMessage {
 public:
  using MethodCallMessage::MethodCallMessage;

  MemberFunctionCallMessage(const Handle& src, const Handle& dest, MemFunc func,
                            Args... args)
      : MethodCallMessage(src, dest),
        func_(std::move(func)),
        args_(std::forward<Args>(args)...) {}

 protected:
  void CallMethod(Service* s) override {
    ClassType* concrete_srv = dynamic_cast<ClassType*>(s);
    CHECK(concrete_srv != NULL);
    Invoke(concrete_srv, std::index_sequence_for<Args...>{});
  }

 private:
  template <std::size_t... I>
  void Invoke(ClassType* obj, std::index_sequence<I...>) {
    (obj->*func_)(std::forward<Args>(std::get<I>(args_))...);
  }

  MemFunc func_;
  std::tuple<typename std::decay<Args>::type...> args_;
};

template <typename ClassType, typename... FunArgs, typename... Args>
inline auto MakeMethodCallMessage(const Message::Handle& src, const Message::Handle& dest,
                                  void (ClassType::*func)(FunArgs...), Args&&... args)
    -> std::shared_ptr<MemberFunctionCallMessage<ClassType, decltype(func), FunArgs...>> {
  typedef MemberFunctionCallMessage<ClassType, decltype(func), FunArgs...> ObjType;

  return std::make_shared<ObjType>(src, dest, func, std::forward<Args>(args)...);
}

}  // namespace mcast

#endif  // CAST_MESSAGE_H_
