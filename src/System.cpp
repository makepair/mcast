#include "System.h"

#include <iostream>
#include <limits>
#include <string>
#include <utility>

#include "WakeupService.h"

namespace mcast {

thread_local System::PerthreadData *System::this_thread_data_ = nullptr;

class IdleService : public UserThreadService {
 public:
  using UserThreadService::UserThreadService;

  void Main() override {
    while (!this_thread::IsInterrupted() || system()->NeedSchedule()) {
      if (!system()->Schedule()) {
        system()->RebalanceReadyQueue();
      }
    }
  }
};

Status System::Start(int thread_num) {
  CHECK(stopped);

  std::unique_lock<std::mutex> lk(mutex_);
  int const worker_num = thread_num <= 0 ? 2 : thread_num;
  LOG_INFO << "System start, the number of threads:" << worker_num;

  perthread_data_.resize(static_cast<size_t>(worker_num));
  auto status = io_srv_.Initialize(this);
  if (!status) {
    LOG_WARN << "IOService Initialize error:" << status.ErrorText();
    return status;
  }

  threads_.push_back(Thread().Run([this]() mutable { timer_srv_.Run(); }));
  threads_.push_back(Thread().Run([this]() mutable { io_srv_.Run(); }));

  for (int i = 0; i < worker_num; ++i) {
    threads_.push_back(Thread().Run([this, i]() mutable { ThreadMain(i); }));
  }

  while (worker_init_num_.load() != worker_num)
    this_thread::Yield();

  stopped.store(false);  // must be set before StartBuitinServices
  status = StartBuitinServices();
  if (!status) {
    LOG_WARN << "StartBuitinServices error:" << status.ErrorText();
    Stop();
    return status;
  }

  return Status::OK();
}

Status System::StartBuitinServices() {
  wakeup_srv_handle_ = LaunchService<WakeupService>("WakeupService");
  if (!wakeup_srv_handle_) {
    return Status(kFailed, "Launch WakeupService failed");
  }

  return CallMethod(wakeup_srv_handle_, &WakeupService::Start);
}

void System::Stop() {
  std::unique_lock<std::mutex> lk(mutex_);
  if (stopped) {
    return;
  }

  LOG_INFO << "System stop";
  stopped.store(true);
  StopAllService();
  io_srv_.Stop();
  for (auto &t : threads_) {
    t.Interrupt();
  }

  for (auto &t : threads_) {
    if (t.Joinable())
      t.Join();
  }
  threads_.clear();
  CHECK(run_queue_.empty());
  stop_cond_.notify_all();
}

void System::WaitStop() {
  std::unique_lock<std::mutex> lk(mutex_);
  stop_cond_.wait(lk, [this] { return stopped.load(); });
}

void System::StopAllService() {
  std::unique_lock<std::shared_timed_mutex> g(services_shared_mutex_);
  auto ss = services_;
  g.unlock();
  for (auto &p : ss) {
    auto &s = p.second;
    if (s && s->handle().index() != idle_service_index_)
      StopService(s);
  };
}

ServicePtr System::GrabService(const Handle &h) {
  assert(h);
  ServicePtr srv;
  if (idle_service_index_ != h.index()) {
    return FindService(h);
  } else {
    srv = this_thread_data_->main_service;
  }

  return srv;
}

bool System::StopService(const Handle &h) {
  auto srv = GrabService(h);
  if (srv) {
    StopService(srv);
    return true;
  }

  return false;
}

void System::StopService(const ServicePtr &srv) {
  std::unique_lock<std::mutex> gl(srv->context()->mutex);
  if (srv->context()->stopping.load(std::memory_order_relaxed)) {
    return;
  }

  srv->context()->stopping.store(true, std::memory_order_relaxed);
  Wakeup_Locked(srv, ServiceEvent::kServiceStop);
}

void System::InterruptService(const Handle &h) {
  auto srv = GrabService(h);
  if (!srv)
    return;

  std::unique_lock<std::mutex> gl(srv->context()->mutex);

  if (ServiceIsBlocked(srv.get()) &&
      (srv->context()->wait_events & ServiceEvent::kInterrupt)) {
    Wakeup_Locked(srv, ServiceEvent::kInterrupt);
  }
}

Status System::WaitSignal() {
  auto events =
      Wait(ServiceEvent::kSignal | ServiceEvent::kInterrupt | ServiceEvent::kServiceStop);
  if (events & ServiceEvent::kSignal)
    return Status::OK();

  return Status(kInterrupt);
}

void System::Signal(const Handle &h) {
  auto srv = GrabService(h);
  if (!srv)
    return;

  std::unique_lock<std::mutex> gl(srv->context()->mutex);
  Wakeup_Locked(srv, ServiceEvent::kSignal);
}

bool System::Schedule() {
  return SwitchTo(CurrentService(), GetReadyService());
}

bool System::SwitchTo(ServicePtr &&cur_srv, ServicePtr &&next_srv) {
  ServiceContext::ContextType *next_ucontext_ptr = nullptr;
  ServiceContext::ContextType *save_ucontext_ptr = nullptr;
  {
    if (cur_srv == next_srv)
      return false;

    assert(cur_srv);
    assert(next_srv);
    LOG_TRACE << "switch from " << cur_srv->name() << " to " << next_srv->name();
    assert(this_thread_data_);
    CHECK(cur_srv == this_thread_data_->current_service);

    {
      cur_srv->context()->is_swaping_out = true;
      cur_srv->context()->blocked_time.store(timer_srv_.GetCurrentTime(),
                                             std::memory_order_relaxed);

      save_ucontext_ptr = &cur_srv->context()->ucontext;
      next_ucontext_ptr = &next_srv->context()->ucontext;
      this_thread_data_->current_service = std::move(next_srv);
      this_thread_data_->prev_service = std::move(cur_srv);
    }
  }

  jump_fcontext(save_ucontext_ptr, *next_ucontext_ptr, reinterpret_cast<intptr_t>(this),
                true);

  CHECK(this_thread_data_->current_service);
  OnResume(this_thread_data_->current_service, this_thread_data_->prev_service);
  this_thread_data_->prev_service.reset();
  return true;
}

void System::OnResume(ServicePtr &cur_srv, ServicePtr &prev_srv) {
  assert(cur_srv);
  LOG_TRACE << "resuming " << cur_srv->name() << ", previous is "
            << (prev_srv ? prev_srv->name() : "None");
  if (prev_srv) {
    std::lock_guard<std::mutex> gl(prev_srv->context()->mutex);
    CHECK_EQ(prev_srv->context()->is_swaping_out, true);
    prev_srv->context()->is_swaping_out = false;
    if (!IsIdleService(prev_srv.get())) {
      if (prev_srv->context()->wakeup_signal && ServiceIsBlocked(prev_srv.get())) {
        SetServiceStatus(prev_srv.get(), ServiceStatus::kReady);
        PutReadyService(prev_srv);
      }
    }
  }
  {
    std::lock_guard<std::mutex> gl(cur_srv->context()->mutex);
    cur_srv->context()->last_thread_index_ = this_thread_data_->thread_index;
    CHECK_EQ(cur_srv->context()->is_swaping_out, false);
    CHECK(cur_srv->context()->status == ServiceStatus::kReady);

    cur_srv->context()->wakeup_signal = false;
    cur_srv->context()->wait_events = 0;
    cur_srv->context()->status = ServiceStatus::kRunning;
  }
  cur_srv->context()->wakeup_time.store(timer_srv_.GetCurrentTime(),
                                        std::memory_order_relaxed);
}

void System::ThreadMain(int thread_index) {
  CHECK(nullptr == this_thread_data_);
  this_thread_data_ = &perthread_data_[static_cast<size_t>(thread_index)];
  this_thread_data_->thread_index = thread_index;

  auto msrv = CreateService<IdleService>(this, "IdleService");
  msrv->handle(Handle(idle_service_index_));
  {
    auto sctxt = CreateServiceContext<IdleService, kNormalStackSize>(
        msrv.get(), &UserThreadServiceMain);

    if (!sctxt)
      return;

    sctxt->status = ServiceStatus::kReady;
    msrv->SetContext(std::move(sctxt));
  }

  this_thread_data_->main_service.reset(msrv.release());
  this_thread_data_->current_service = this_thread_data_->main_service;
  this_thread_data_->prev_service.reset();

  ++worker_init_num_;
  jump_fcontext(&this_thread_data_->ucontext,
                this_thread_data_->current_service->context()->ucontext,
                reinterpret_cast<intptr_t>(static_cast<void *>(this)), true);

  if (this_thread_data_) {
    this_thread_data_->current_service.reset();
    this_thread_data_->prev_service.reset();
    this_thread_data_->main_service.reset();
    this_thread_data_->local_ready_queue.clear();
  }

  LOG_INFO << "ThreadMain Stop";
}

void System::UserThreadServiceMain(intptr_t ptr) {
  System *sys = nullptr;
  {
    sys = reinterpret_cast<System *>(ptr);
    ServicePtr cur_srv = sys->CurrentService();
    cur_srv->OnServiceStart();
    sys->OnResume(cur_srv, sys->this_thread_data_->prev_service);
    sys->this_thread_data_->prev_service.reset();

    sys->SetServiceStatus(cur_srv.get(), ServiceStatus::kRunning);
    {  // FIXME: remove dynamic_cast
      auto *srv = dynamic_cast<UserThreadService *>(cur_srv.get());
      CHECK(srv);
      srv->Main();
    }

    cur_srv->OnServiceStop();
    LOG_TRACE << cur_srv->name() << " stopping";

    const bool server_stoped = cur_srv->handle().index() == sys->idle_service_index_;
    cur_srv->context()->stopping = true;
    sys->SetServiceStatus(cur_srv.get(), ServiceStatus::kDead);
    std::string sname = cur_srv->name();
    LOG_TRACE << "RetireService " << sname << " refcount " << cur_srv.use_count() - 1;

    cur_srv->context()->msg_queue.empty();
    sys->RemoveService(cur_srv->handle());
    cur_srv.reset();
    if (server_stoped) {
      ServiceContext::ContextType save_context;

      jump_fcontext(&save_context, sys->this_thread_data_->ucontext, 0, false);

      CHECK(false);
    }
  }

  assert(sys);
  sys->Schedule();  // never resume
  CHECK(false);
}

void System::MessageDrivenServiceMain(intptr_t ptr) {
  System *sys = nullptr;
  {
    sys = reinterpret_cast<System *>(ptr);

    ServicePtr cur_srv = sys->CurrentService();
    cur_srv->OnServiceStart();
    sys->OnResume(cur_srv, sys->this_thread_data_->prev_service);
    sys->this_thread_data_->prev_service.reset();

    auto *srv_context = cur_srv->context();
    MessageDrivenService *msg_driven_srv_ptr =
        dynamic_cast<MessageDrivenService *>(cur_srv.get());

    CHECK(msg_driven_srv_ptr);
    sys->SetServiceStatus(cur_srv.get(), ServiceStatus::kRunning);
    MessagePtr msg;
    std::unique_lock<std::mutex> unique_lock(srv_context->mutex);
    while (true) {
      CHECK(unique_lock.owns_lock());
      if (!srv_context->msg_queue.empty()) {
        msg = std::move(srv_context->msg_queue.front());
        srv_context->msg_queue.pop_front();
        assert(msg);

        unique_lock.unlock();
        msg_driven_srv_ptr->HandleMessage(msg);
        msg.reset();
        unique_lock.lock();
      } else {
        assert(unique_lock.owns_lock());
        if (srv_context->stopping.load()) {
          break;
        }

        if (!srv_context->wakeup_signal) {
          ServiceEvent revent = sys->Wait_Locked(
              cur_srv.get(), ServiceEvent::kMessage | ServiceEvent::kServiceStop,
              &unique_lock);
          (void)revent;
          CHECK(unique_lock.owns_lock());
          CHECK(revent & (ServiceEvent::kMessage | ServiceEvent::kServiceStop));
          // CHECK(srv_context->wakeup_signal == false);
        } else {
          srv_context->wakeup_signal = false;
          CHECK(unique_lock.owns_lock());
        }
      }
    }

    cur_srv->OnServiceStop();
    CHECK(srv_context->msg_queue.empty());
    LOG_TRACE << cur_srv->name() << " stopping";
    // locked
    CHECK(unique_lock.owns_lock());
    CHECK(srv_context->stopping);

    sys->SetServiceStatus(cur_srv.get(), ServiceStatus::kDead);
    std::string sname = cur_srv->name();
    LOG_TRACE << "RetireService " << sname << " refcount " << cur_srv.use_count() - 1;
    sys->RemoveService(cur_srv->handle());
    cur_srv.reset();
    unique_lock.unlock();
  }

  sys->Schedule();  // never resume
  CHECK(false);
}

Status System::SendMessage(const MessagePtr &msg) {
  auto const h = msg->destination();
  assert(h);
  auto dest_srv = GrabService(h);

  if (dest_srv) {
    auto srv_context = dest_srv->context();
    std::unique_lock<std::mutex> unique_lock(srv_context->mutex);
    if (srv_context->stopping.load(std::memory_order_relaxed) ||
        GetServiceStatus(dest_srv.get()) == ServiceStatus::kCreated) {
      unique_lock.unlock();
      return Status(kNotFound);
    }

    srv_context->msg_queue.push_back(msg);
    if (srv_context->wait_events & ServiceEvent::kMessage) {
      Wakeup_Locked(dest_srv, ServiceEvent::kMessage);
    }
    unique_lock.unlock();
    return Status::OK();
  } else {
    return Status(kNotFound);
  }
}

Status System::SendStringMessage(const Handle &dest_service, const std::string &text,
                                 const Message::Closure &done) {
  Handle self;
  if (this_thread_data_) {
    self = CurrentService()->handle();
  }

  auto msg = CreateSharedObject<StringMessage>(self, dest_service, done, text);
  return SendMessage(msg);
}

uint64_t System::ServiceSleepTime(const Service *srv) {
  if (srv->context()->status.load(std::memory_order_relaxed) == ServiceStatus::kBlocked) {
    return timer_srv_.ToMilliseconds(timer_srv_.DurationSince(
        srv->context()->blocked_time.load(std::memory_order_relaxed)));
  }

  return 0;
}

ServiceEvent System::Wait(ServiceEvent const events) {
  ServicePtr srv = CurrentService();
  std::unique_lock<std::mutex> unique_lock(srv->context()->mutex);
  return Wait_Locked(srv.get(), events, &unique_lock);
}

ServiceEvent System::Wait_Locked(Service *srv, ServiceEvent events,
                                 std::unique_lock<std::mutex> *unique_lock) {
  LOG_TRACE << "Wait: " << srv->name() << ",event: " << events;

  CHECK(unique_lock->owns_lock());
  CHECK(CurrentService().get() == srv);
  CHECK(srv && srv->context()->status == ServiceStatus::kRunning);
  srv->context()->wait_events = 0;

  if (srv->context()->stopping.load(std::memory_order_relaxed) &&
      (events & ServiceEvent::kServiceStop)) {
    return ServiceEvent::kServiceStop;
  }

  ServiceEvent revents;
  if (events & srv->context()->events) {
    LOG_TRACE << "Wait: wake up without switching, service " << srv->name()
              << ",wait events " << events;

    revents = srv->context()->events;
    srv->context()->events &= ~events;
    return revents & events;
  }

  while (true) {
    SetServiceStatus(srv, ServiceStatus::kBlocked);
    srv->context()->is_swaping_out = true;
    srv->context()->wait_events = events;
    unique_lock->unlock();
    bool res = Schedule();
    CHECK(res);
    unique_lock->lock();
    if (events & srv->context()->events) {
      revents = srv->context()->events;
      srv->context()->events &= ~events;
      return revents & events;
    } else {
      LOG_WARN << "Wait: waked up service " << srv->name() << " which is waiting for "
               << events << ",but waked up by "
               << ServiceEventToText(srv->context()->events);
    }
  }
}

bool System::WakeUp(const Handle &h, ServiceEvent e) {
  auto srv = GrabService(h);
  if (srv) {
    return WakeUp(srv, e);
  }

  return false;
}

bool System::WakeUp(const ServicePtr &srv, ServiceEvent e) {
  std::lock_guard<std::mutex> gl(srv->context()->mutex);
  return Wakeup_Locked(srv, e);
}

bool System::Wakeup_Locked(const ServicePtr &srv, ServiceEvent e) {
  LOG_TRACE << "wake up " << srv->name() << " with events " << e;

  //@note Wakeup may happen before Wait
  srv->context()->events |= e;

  if ((srv->context()->wait_events & e) == 0)
    return false;

  srv->context()->wakeup_signal = true;
  if (srv->context()->is_swaping_out)
    return true;

  if (ServiceIsBlocked(srv.get())) {
    SetServiceStatus(srv.get(), ServiceStatus::kReady);
    PutReadyService(srv);
  }

  return true;
}

Status System::SleepService(uint32_t milliseconds) {
  ServicePtr srv = CurrentService();
  assert(srv);
  auto h = srv->handle();

  std::unique_lock<std::mutex> ul(srv->context()->mutex);
  TimerHandle th = timer_srv_.AddTimer(
      milliseconds, [h, this]() mutable { this->WakeUp(h, ServiceEvent::kSleep); });

  ServiceEvent revents = Wait_Locked(
      srv.get(),
      ServiceEvent::kSleep | ServiceEvent::kServiceStop | ServiceEvent::kInterrupt, &ul);
  ul.unlock();
  if (revents & ServiceEvent::kSleep) {
    return Status::OK();
  } else {
    timer_srv_.DeleteTimer(th);
    LOG_INFO << "SleepService: " << srv->name() << " interrupted by events " << revents;
    return Status(kInterrupt);
  }
}

Status System::WaitIO(int fd, unsigned int io_events) {
  ServicePtr srv = CurrentService();
  LOG_TRACE << "WaitIO: " << srv->name() << " wait io events "
            << IOService::EpollEventText(fd, io_events);

  std::unique_lock<std::mutex> ul(srv->context()->mutex);
  srv->context()->io_events = 0;
  if (auto status = GetIOService()->Add(srv.get(), fd, io_events)) {
    ServiceEvent revents =
        Wait_Locked(srv.get(), ServiceEvent::kIO_Operation | ServiceEvent::kServiceStop |
                                   ServiceEvent::kInterrupt,
                    &ul);

    ul.unlock();
    if (revents & ServiceEvent::kIO_Operation) {
      if (srv->context()->io_events & io_events)
        return Status::OK();
      return Status(kFailed, "WaitIO: io error");
    } else {
      GetIOService()->Remove(srv.get(), fd);
      LOG_INFO << "WaitIO: " << srv->name() << " interrupted by events " << revents;
      return Status(kInterrupt);
    }
  } else {
    return status;
  }
}

void System::OnIOReady(ServicePtr &srv, int fd, unsigned int io_events) {
  LOG_TRACE << "OnIOReady:service " << srv->name() << " wait events "
            << ServiceEventToText(srv->context()->wait_events) << ",fd " << fd
            << ",io events " << IOService::EpollEventText(fd, io_events);
  std::lock_guard<std::mutex> gl(srv->context()->mutex);
  if (srv->context()->wait_events & ServiceEvent::kIO_Operation) {
    srv->context()->io_events = io_events;
    Wakeup_Locked(srv, ServiceEvent::kIO_Operation);
  }
}

Status System::WakeupIfWaitTimeout(const Handle &h, uint32_t max_sleeptime_ms) {
  auto s = GrabService(h);
  if (s)
    return WakeupIfWaitTimeout(s.get(), max_sleeptime_ms);

  return Status(kNotFound);
}

Status System::WakeupIfWaitTimeout(const Service *srv, uint32_t max_sleeptime_ms) {
  return AsyncCallMethod(wakeup_srv_handle_, &WakeupService::AddService, srv->handle(),
                        max_sleeptime_ms);
}

ServicePtr System::GetReadyService() {
  ServicePtr srv;
  if (!run_queue_.pop(&srv)) {
    assert(this_thread_data_);
    srv = this_thread_data_->main_service;
    SetServiceStatus(srv.get(), ServiceStatus::kReady);
  }
  return srv;
}

void System::PutReadyService(const ServicePtr &srv) {
  if (srv->handle().index() == idle_service_index_)
    return;

  run_queue_.push(srv);
}

void System::RebalanceReadyQueue() {}

}  // namespace mcast
