#ifndef CAST_IOSERVICE_H_
#define CAST_IOSERVICE_H_

#include <sys/epoll.h>

#include <functional>
#include <memory>
#include <string>

#include "Service.h"
#include "util/Status.h"

namespace mcast {

class System;

class IOService {
 public:
  ~IOService();

  Status Initialize(System *s);

  Status Add(const Service *srv, int fd, unsigned int events);
  Status Remove(const Service *srv, int fd);

  void Run();
  void Stop();

  static std::string EpollEventText(int fd, uint32_t e);

 private:
  int epoll_fd_ = -1;
  int exit_pipefd_[2];

  System *system_ = nullptr;
};

}  // namespace mcast

#endif  // CAST_IOSERVICE_H_