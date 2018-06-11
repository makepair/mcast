#pragma once

#include <iostream>
#include <string>

namespace mcast {

struct ServiceEvent {
  const static unsigned kNoneEvent = 0;
  const static unsigned kServiceStart = 1U;
  const static unsigned kSignal = 1U << 1;
  const static unsigned kInterrupt = 1U << 2;
  const static unsigned kMessage = 1U << 3;
  const static unsigned kRequest = 1U << 4;
  const static unsigned kResponse = 1U << 5;
  const static unsigned kIO_Operation = 1U << 6;
  const static unsigned kSleep = 1U << 7;
  const static unsigned kTimeout = 1U << 8;
  const static unsigned kServiceStop = 1U << 9;
  const static unsigned kCount = 11;
  // warnning: change the s_service_event_texts

  unsigned events = kNoneEvent;

  ServiceEvent() = default;
  ServiceEvent(unsigned es) : events(es) {}

  operator unsigned() const {
    return events;
  }
};

extern std::string ServiceEventToText(unsigned e);

inline std::string ServiceEventToText(ServiceEvent e) {
  return ServiceEventToText(static_cast<unsigned>(e));
}

inline std::ostream& operator<<(std::ostream& os, ServiceEvent e) {
  os << ServiceEventToText(e);
  return os;
}

}  // namespace mcast
