#ifndef CASTOR_WAKEUPSERVICE_H_
#define CASTOR_WAKEUPSERVICE_H_

#include <stdint.h>

#include "Service.h"
#include "TimerService.h"
#include "util/Status.h"

namespace mcast {

class WakeupService : public MethodCallService {
 public:
  using MethodCallService::MethodCallService;
//  ~WakeupService() { LOG_TRACE << "~WakeupService()"; }

  void Start();
  void AddService(const Handle& srvh, uint32_t max_sleep_time_ms);

 private:
  void Update();
  void AddTimer();

  struct Item {
    Handle srv_handle;
    uint32_t max_sleep_time_ms;
  };

  uint32_t interval_ms_ = 30000;  // default: 30 seconds
  std::list<Item> conn_srvs_;
  BasicHandle<WakeupService> self_handle_;
  Status status_;
  TimerHandle timer_;
};

}  // namespace mcast
#endif  // CASTOR_WAKEUPSERVICE_H_
