#include "Service.h"

#include "System.h"

namespace mcast {

Service::~Service() {}

void Service::Yield() {
  system()->Schedule();
}

Status Service::WaitInput(int fd) {
  return system()->WaitInput(fd);
}

Status Service::WaitOutput(int fd) {
  return system()->WaitOutput(fd);
}

void Service::Stop() {
  system()->StopService(this->handle());
}

bool Service::IsStopping() {
  return system()->ServiceIsStopping(this);
}

Status Service::Sleep(uint32_t milliseconds) {
  return system()->SleepService(milliseconds);
}

Status Service::WaitSignal() {
  return system()->WaitSignal();
}

}  // namespace mcast
