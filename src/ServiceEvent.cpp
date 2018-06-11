#include "ServiceEvent.h"

namespace mcast {

static const char* s_service_event_texts[] = {
    "NoneEvent", "ServiceStart", "Signal", "Interrupt", "Message",    "Request",
    "Response",  "IO_Operation", "Sleep",  "Timeout",   "ServiceStop"};

std::string ServiceEventToText(unsigned x) {
  std::string str = "[";
  for (unsigned i = 1; x > 0 && i < static_cast<unsigned>(ServiceEvent::kCount); ++i) {
    if (x & 0x1) {
      if (str.size() > 1)
        str += '|';

      str += s_service_event_texts[i];
    }
    x >>= 1;
  }

  return !str.empty() ? (str += ']') : "[NoneEvent]";
}

}  // namespace mcast
