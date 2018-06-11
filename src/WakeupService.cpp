#include "WakeupService.h"

#include "System.h"
#include "util/Logging.h"

namespace mcast {

static uint32_t min_interval_ms = 100;

void WakeupService::AddService(const Handle& srvh, uint32_t max_sleep_time_ms) {
  assert(handle());
  if (max_sleep_time_ms < min_interval_ms)
    max_sleep_time_ms = min_interval_ms;

  if (max_sleep_time_ms < interval_ms_)
    interval_ms_ = max_sleep_time_ms;

  conn_srvs_.push_back(Item{srvh, max_sleep_time_ms});

  if (timer_.expired()) {
    AddTimer();
  }
}

void WakeupService::AddTimer() {
  auto sys = system();
  auto selfh = self_handle_;
  auto tm = timer_;

  timer_ = this->system()->AddTimer(interval_ms_, [sys, selfh, tm]() mutable {
    auto r = sys->AsyncCallMethod(selfh, &WakeupService::Update);
    if (!r) {
      sys->RemoveTimer(tm);
      sys->StopService(selfh);
      LOG_WARN << "WakeupService AsyncCallMethod failed, " << r.ErrorText();
    }
  });
}

void WakeupService::Start() {
  self_handle_ = GetHandle<WakeupService>();
  assert(self_handle_);

  assert(timer_.expired());

  if (!conn_srvs_.empty()) {
    AddTimer();
  }
}

void WakeupService::Update() {
  for (auto it = conn_srvs_.begin(); it != conn_srvs_.end();) {
    auto srv = system()->GrabService(it->srv_handle);
    auto max_sleep_time_ms = it->max_sleep_time_ms;

    if (!srv) {
      conn_srvs_.erase(it++);
    } else if (system()->ServiceSleepTime(srv.get()) > max_sleep_time_ms) {
      LOG_INFO << "WakeUp service " << srv->name() << " "
                << srv->handle().index();

      auto revents = system()->WakeUp(srv, ServiceEvent::kInterrupt);
      if (revents & ServiceEvent::kServiceStop) {
        break;
      }
      conn_srvs_.erase(it++);
    } else {
      ++it;
    }
  }

  if (status_ && !IsStopping()) {
    AddTimer();
  }
}

}  // namespace mcast
