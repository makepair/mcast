#ifndef CAST_SERVICECONTEXT_H_
#define CAST_SERVICECONTEXT_H_

#include <stdint.h>
#include <sys/mman.h>

#include <atomic>
#include <deque>
#include <memory>
#include <mutex>
#include <string>

#include "libcontext.h"

#include "Message.h"
#include "ServiceEvent.h"
#include "TimerService.h"
#include "config.h"
#include "util/Logging.h"
#include "util/Noncopyable.h"
#include "util/ThreadSafeQueue.h"

namespace mcast {

class System;
class Service;

enum class ServiceStatus { kCreated = 1, kBlocked, kReady, kRunning, kDead };

class ServiceContext : public Noncopyable {
  friend class System;

 protected:
  static constexpr int kStackAlignment = kPageSize;
  static constexpr int kStackAlignmentMask = (kStackAlignment - 1);

 public:
  typedef std::shared_ptr<ServiceContext> ServiceContextPtr;
  typedef fcontext_t ContextType;

  ServiceContext();
  virtual ~ServiceContext();

 protected:
  Service* service() {
    return srv;
  }

  static ServiceContextPtr Create(Service* s, System* sys, int stacksize,
                                  void (*serviceMain)(intptr_t));

  int last_thread_index_ = -1;
  std::mutex mutex;
  std::deque<MessagePtr> msg_queue;

  bool wakeup_signal = false;
  bool is_swaping_out = false;
  std::atomic<ServiceStatus> status{ServiceStatus::kCreated};
  std::atomic_bool stopping{false};

  unsigned int wait_events = ServiceEvent::kNoneEvent;
  unsigned int events = ServiceEvent::kNoneEvent;
  unsigned int io_events = 0;

  std::atomic<TimerService::Timestamp> blocked_time{0};
  std::atomic<TimerService::Timestamp> wakeup_time{0};

  int fd = -1;
  fcontext_t ucontext;

  Service* srv = nullptr;

  uint8_t* stack = nullptr;
  int stack_size = 0;

#ifdef VALGRIND
  int valg_ret = -1;
#endif
};

typedef ServiceContext::ServiceContextPtr ServiceContextPtr;

inline uint8_t* AlignStackTop(void* addr, unsigned int align) {
  uintptr_t mask = align - 1;
  uintptr_t ptr = reinterpret_cast<uintptr_t>(addr);
  return reinterpret_cast<uint8_t*>(ptr & ~mask);
}

}  // namespace mcast

#endif  // CAST_SERVICECONTEXT_H_
