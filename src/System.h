#ifndef CAST_SYSTEM_H_
#define CAST_SYSTEM_H_

#include <ctype.h>
#include <inttypes.h>
#include <stdint.h>

#include <functional>
#include <future>
#include <map>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "util/Logging.h"
#include "util/Noncopyable.h"
#include "util/Status.h"

#include "Closure.h"
#include "IOService.h"
#include "Message.h"
#include "Service.h"
#include "ServiceContext.h"
#include "TimerService.h"

namespace mcast {

class IOService;
class WakeupService;

class System : public Noncopyable {
 public:
  typedef Service::Handle Handle;
  constexpr static int kVerySmallStackSize = 32 * 1024;
  constexpr static int kSmallStackSize = 128 * 1024;
  constexpr static int kNormalStackSize = 1024 * 1024;
  constexpr static int kLargeStackSize = 4 * 1024 * 1024;
  constexpr static int kVeryLargeStackSize = 8 * 1024 * 1024;

  template <typename T>
  using BasicHandle = Service::BasicHandle<T>;

  System() = default;
  ~System() {
    Stop();
  }

  Status Start(int worker_num_hint = 0);
  void Stop();
  void WaitStop();

  template <typename Service, typename... Args>
  BasicHandle<Service> LaunchService(Args&&... args) {
    return LaunchService<Service, kNormalStackSize>(std::forward<Args>(args)...);
  }

  template <typename ServiceType, int StackSize, typename... Args>
  BasicHandle<ServiceType> LaunchService(Args&&... args) {
    auto sptr = CreateService<ServiceType>(this, std::forward<Args>(args)...);
    return LaunchService<ServiceType, StackSize>(std::move(sptr));
  }

  template <typename ServiceType, int StackSize = kNormalStackSize>
  BasicHandle<ServiceType> LaunchService(BasicServicePtr<ServiceType>&& s);

  template <typename ServiceType, typename... FunArgs, typename... Args>
  Status CallMethod(const Handle& dest_service, void (ServiceType::*func)(FunArgs...),
                    Args&&... args);

  template <typename ServiceType, typename... FunArgs, typename... Args>
  Status CallMethodWithClosure(const Handle& dest_service,
                               void (ServiceType::*func)(FunArgs...), Args&&... args);

  template <typename Closure, typename ServiceType, typename... FunArgs, typename... Args>
  Status AsyncCallMethod(const Handle& dest_service, const Closure& done,
                         void (ServiceType::*func)(FunArgs...), Args&&... args);

  template <typename ServiceType, typename... FunArgs, typename... Args>
  Status AsyncCallMethod(const Handle& dest_service,
                         void (ServiceType::*func)(FunArgs...), Args&&... args);

  Status SendMessage(const MessagePtr& msg);
  Status SendStringMessage(const Handle& dest_service, const std::string& text,
                           const Message::Closure& done);

  Status SleepService(uint32_t milliseconds);
  uint64_t ServiceSleepTime(const Service* srv);  // milliseconds

  template <typename T>
  TimerHandle AddTimer(uint32_t timeMilliseconds, T&& callback) {
    return timer_srv_.AddTimer(timeMilliseconds, [callback]() mutable { callback(); });
  }

  void RemoveTimer(const TimerHandle& h) {
    timer_srv_.DeleteTimer(h);
  }

  bool StopService(const Handle& h);
  void StopService(const ServicePtr& srv);

  bool ServiceIsStopping(Service* srv) {
    return srv->context()->stopping.load(std::memory_order_relaxed);
  }

  void InterruptService(const Handle& h);
  Status WaitSignal();
  void Signal(const Handle& h);

  bool Schedule();
  ServicePtr GrabService(const Handle& h);

  Status WaitIO(int fd, unsigned int ioevents);

  bool WakeUp(const Handle& h, ServiceEvent e);
  bool WakeUp(const ServicePtr& srv, ServiceEvent events_signal);

  Status WaitInput(int fd) {
    return WaitIO(fd, EPOLLIN | EPOLLET);
  }
  Status WaitOutput(int fd) {
    return WaitIO(fd, EPOLLOUT | EPOLLET);
  }

  Status WakeupIfWaitTimeout(const Handle& h, uint32_t max_sleeptime_ms);
  Status WakeupIfWaitTimeout(const Service* srv, uint32_t max_sleeptime_ms);

  IOService* GetIOService() {
    return &io_srv_;
  }

  template <typename Class, typename... Args>
  static std::unique_ptr<Class> CreateObject(Args&&... args) {
    return std::unique_ptr<Class>(new Class(std::forward<Args>(args)...));
  }

  template <typename Class, typename... Args>
  static std::shared_ptr<Class> CreateSharedObject(Args&&... args) {
    return std::make_shared<Class>(std::forward<Args>(args)...);
  }

  // template <typename Class, typename... Args>
  // static std::shared_ptr<Class> CreateSharedObjectFromCache(Args&&... args) {
  //   auto ptr = ObjectCache<Class>::get(std::forward<Args>(args)...);
  //   return std::shared_ptr<Class>(
  //       ptr, [](Class* obj) { ObjectCache<Class>::put(obj); });
  // }

  template <typename Service, typename... Args>
  static std::unique_ptr<Service> CreateService(Args&&... args) {
    return std::unique_ptr<Service>(new Service(std::forward<Args>(args)...));
  }

 private:
  void OnIOReady(ServicePtr& srv, int fd, unsigned int io_events);

  size_t ServiceMessageQueueSize(const Service* srv) {
    std::lock_guard<std::mutex> ul(srv->context()->mutex);
    return srv->context()->msg_queue.size();
  }

  ServicePtr CurrentService() {
    assert(this_thread_data_ && this_thread_data_->current_service);
    return this_thread_data_->current_service;
  }

  template <typename ServiceType, int StackSize>
  ServiceContextPtr CreateServiceContext(ServiceType* s, void (*ServiceMain)(intptr_t)) {
    return ServiceContext::Create(s, this, StackSize, ServiceMain);
  }

  ServiceEvent Wait(ServiceEvent event);
  ServiceEvent Wait_Locked(Service* srv, ServiceEvent event,
                           std::unique_lock<std::mutex>* unique_lock);

  void ThreadMain(int thread_index);

  static void MessageDrivenServiceMain(intptr_t);
  static void UserThreadServiceMain(intptr_t);

  bool SwitchToNext();
  bool SwitchTo(ServicePtr&& cur_srv, ServicePtr&& next_srv);
  void OnResume(ServicePtr& cur_srv, ServicePtr& prev_srv);

  bool Wakeup_Locked(const ServicePtr& srv, ServiceEvent events);

  void SetServiceStatus(const Service* srv, ServiceStatus s) {
    srv->context()->status.store(s, std::memory_order_relaxed);
  }

  bool ServiceIsBlocked(const Service* srv) {
    return srv->context()->status.load(std::memory_order_relaxed) ==
           ServiceStatus::kBlocked;
  }

  ServiceStatus GetServiceStatus(const Service* srv) {
    return srv->context()->status.load(std::memory_order_relaxed);
  }

  void StopAllService();
  Handle::IndexType NewHandleIndex();

  bool NeedSchedule() {
    return !run_queue_.empty();
  }

  bool IsIdleService(Service* s) {
    return s->handle().index() == idle_service_index_;
  }

  void StopUnguard();
  Status StartBuitinServices();

  ServicePtr GetReadyService();
  void PutReadyService(const ServicePtr& srv);
  void RebalanceReadyQueue();

  void SetServiceFD(const Service* srv, int fd) {
    srv->context()->fd = fd;
  }
  int GetServiceFD(const Service* srv) {
    return srv->context()->fd;
  }

  void AddService(const ServicePtr& s) {
    std::lock_guard<std::shared_timed_mutex> g(services_shared_mutex_);
    services_.emplace(s->handle().index(), s);
  }

  void RemoveService(ServiceHandle h) {
    std::lock_guard<std::shared_timed_mutex> g(services_shared_mutex_);
    services_.erase(h.index());
  }

  ServicePtr FindService(ServiceHandle h) {
    {
      std::shared_lock<std::shared_timed_mutex> g(services_shared_mutex_);
      auto it = services_.find(h.index());
      if (it != services_.end())
        return it->second;
    }

    return ServicePtr();
  }

  struct PerthreadData {
    ServiceContext::ContextType ucontext;
    ServicePtr main_service;
    ServicePtr current_service;
    ServicePtr prev_service;
    ThreadSafeQueue<Service*> local_ready_queue;
    int thread_index = -1;
  };

  static thread_local PerthreadData* this_thread_data_;
  std::vector<PerthreadData> perthread_data_;
  ThreadSafeQueue<ServicePtr> run_queue_;

  std::shared_timed_mutex services_shared_mutex_;
  std::unordered_map<Handle::IndexType, ServicePtr> services_;
  std::atomic<int64_t> service_index_alloc_{16};

  std::mutex mutex_;
  std::atomic_bool stopped{true};
  std::condition_variable stop_cond_;
  std::atomic_int worker_init_num_{0};
  std::vector<Thread> threads_;

  IOService io_srv_;
  TimerService timer_srv_;

  Handle::IndexType idle_service_index_{1};
  BasicHandle<WakeupService> wakeup_srv_handle_;

  friend class Service;
  friend class IOService;
  friend class IdleService;
};  // class System

template <typename ServiceType, int StackSize>
BasicHandle<ServiceType> System::LaunchService(BasicServicePtr<ServiceType>&& srv) {
  if (stopped.load(std::memory_order_relaxed))
    return BasicHandle<ServiceType>();

  auto* mainfunc = srv->ServiceType() == ServiceType::kUserThreadService
                       ? &UserThreadServiceMain
                       : &MessageDrivenServiceMain;
  {
    ServiceContextPtr sctxt =
        CreateServiceContext<ServiceType, StackSize>(srv.get(), mainfunc);

    if (!sctxt) {
      return BasicHandle<ServiceType>();
    }
    auto const curtime = timer_srv_.GetCurrentTime();
    sctxt->blocked_time.store(curtime, std::memory_order_relaxed);
    sctxt->wakeup_time.store(curtime, std::memory_order_relaxed);
    srv->SetContext(std::move(sctxt));
  }

  ServicePtr srv_ptr(srv.release());
  assert(!srv);
  const auto h = BasicHandle<ServiceType>(++service_index_alloc_);
  srv_ptr->handle(h);

  std::lock_guard<std::mutex> gl(srv_ptr->context()->mutex);
  AddService(srv_ptr);

  SetServiceStatus(srv_ptr.get(), ServiceStatus::kBlocked);
  if (srv_ptr->ServiceType() == ServiceType::kUserThreadService) {
    srv_ptr->context()->wait_events = ServiceEvent::kServiceStart;
    if (!Wakeup_Locked(srv_ptr, ServiceEvent::kServiceStart)) {
      srv_ptr->context()->wait_events = 0;
      SetServiceStatus(srv_ptr.get(), ServiceStatus::kCreated);
      RemoveService(srv_ptr->handle());
      return BasicHandle<ServiceType>();
    }
  } else {
    srv_ptr->context()->wait_events = ServiceEvent::kMessage | ServiceEvent::kServiceStop;
    assert(srv_ptr->context()->msg_queue.empty());
  }

  LOG_TRACE << "LaunchService " << srv_ptr->name() << " " << h.index();
  return h;
}

template <typename ServiceType, typename... FunArgs, typename... Args>
Status System::CallMethod(const Handle& dest, void (ServiceType::*func)(FunArgs...),
                          Args&&... args) {
  CHECK(dest);
  Handle src;
  if (this_thread_data_) {
    assert(this_thread_data_->current_service);
    src = this_thread_data_->current_service->handle();
    assert(src);
  }

  auto const msg = MakeMethodCallMessage(src, dest, func, std::forward<Args>(args)...);

  if (src) {
    msg->SetClosure(
        [this, src](const Status&) mutable { WakeUp(src, ServiceEvent::kResponse); });
    auto status = SendMessage(msg);
    if (status) {
      ServiceEvent revents = Wait(ServiceEvent::kResponse);
      CHECK(revents & ServiceEvent::kResponse);
      // when SendMessage return successfully(kOK), this service(src)
      // will be waked up only by kMessage event, this ensures the message
      // will be handled eventually.
    }
    return status;
  } else {
    std::mutex mutex;
    std::condition_variable cvar;
    bool done = false;
    msg->SetClosure([&cvar, &done, &mutex](const Status&) mutable {
      std::lock_guard<std::mutex> gl(mutex);
      done = true;
      cvar.notify_one();
    });

    auto status = SendMessage(msg);
    if (status) {
      std::unique_lock<std::mutex> lk(mutex);
      cvar.wait(lk, [&done] { return done; });
    }
    return status;
  }
}

template <typename ServiceType, typename... FunArgs, typename... Args>
Status System::CallMethodWithClosure(const Handle& dest,
                                     void (ServiceType::*func)(FunArgs...),
                                     Args&&... args) {
  CHECK(dest);
  Handle src;
  if (this_thread_data_) {
    assert(this_thread_data_->current_service);
    src = this_thread_data_->current_service->handle();
    assert(src);
  }

  if (src) {
    CallClosure closure([this, src] { this->WakeUp(src, ServiceEvent::kResponse); });

    auto const msg =
        MakeMethodCallMessage(src, dest, func, std::forward<Args>(args)..., closure);
    auto status = SendMessage(msg);
    if (status) {
      ServiceEvent revents = Wait(ServiceEvent::kResponse);
      CHECK(revents & ServiceEvent::kResponse);
      // when SendMessage return successfully(kOK), this service(src)
      // will be waked up only by kMessage event, this ensures the message
      // will be handled eventually.
    }
    return status;
  } else {
    std::mutex mutex;
    std::condition_variable cvar;
    bool done = false;
    CallClosure closure([&cvar, &done, &mutex]() mutable {
      std::lock_guard<std::mutex> gl(mutex);
      done = true;
      cvar.notify_one();
    });

    auto const msg =
        MakeMethodCallMessage(src, dest, func, std::forward<Args>(args)..., closure);
    auto status = SendMessage(msg);
    if (status) {
      std::unique_lock<std::mutex> lk(mutex);
      cvar.wait(lk, [&done] { return done; });
    }
    return status;
  }
}

template <typename ServiceType, typename... FunArgs, typename... Args>
Status System::AsyncCallMethod(const Handle& dest, void (ServiceType::*func)(FunArgs...),
                               Args&&... args) {
  CHECK(dest);
  Handle src;
  if (this_thread_data_) {
    assert(this_thread_data_->current_service);
    src = this_thread_data_->current_service->handle();
    assert(src);
  }

  auto const msg = MakeMethodCallMessage(src, dest, func, std::forward<Args>(args)...);

  // when SendMessage failed, the Closure will not be called, the caller should
  // handle this situation
  return SendMessage(msg);
}

template <typename Closure, typename ServiceType, typename... FunArgs, typename... Args>
Status System::AsyncCallMethod(const Handle& dest, const Closure& done,
                               void (ServiceType::*func)(FunArgs...), Args&&... args) {
  CHECK(dest);
  Handle src;
  if (this_thread_data_) {
    assert(this_thread_data_->current_service);
    src = this_thread_data_->current_service->handle();
    assert(src);
  }

  auto const msg = MakeMethodCallMessage(src, dest, func, std::forward<Args>(args)...);
  msg->SetClosure(done);

  // when SendMessage failed, the Closure will not be called, the caller should
  // handle this situation
  return SendMessage(msg);
}

}  // namespace mcast

#endif  // CAST_SYSTEM_H_
