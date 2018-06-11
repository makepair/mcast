#include "IOService.h"

#include <fcntl.h>
#include <sys/epoll.h>
#include <unistd.h>

#include "Service.h"
#include "System.h"
#include "util/Logging.h"
#include "util/Thread.h"
#include "util/util.h"

namespace mcast {

Status IOService::Initialize(System *s) {
  exit_pipefd_[0] = exit_pipefd_[1] = -1;

  epoll_fd_ = epoll_create1(0);
  if (epoll_fd_ == -1) {
    return Status(kFailed, "epoll_create1 failed");
  }

  if (-1 == ::pipe(exit_pipefd_)) {
    close(epoll_fd_);
    epoll_fd_ = -1;
    return Status(kFailed, "pipe failed");
  }

  struct epoll_event ev;
  ev.data.u64 = static_cast<uint64_t>(-1);
  ev.events = EPOLLIN;
  if (epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, exit_pipefd_[0], &ev) == -1) {
    close(epoll_fd_);
    epoll_fd_ = -1;

    close(exit_pipefd_[0]);
    close(exit_pipefd_[1]);
    return Status(kFailed, "epoll_ctl add pipe failed");
  }

  system_ = s;
  return Status::OK();
}

void IOService::Stop() {
  auto r = write(exit_pipefd_[1], "1", 1);
  (void)r;
}

IOService::~IOService() {
  if (epoll_fd_ >= 0)
    close(epoll_fd_);

  if (exit_pipefd_[0] != -1) {
    close(exit_pipefd_[0]);
    close(exit_pipefd_[1]);
  }
}

std::string IOService::EpollEventText(int fd, uint32_t e) {
  std::string msg;
  msg += (e & EPOLLIN) ? "IN " : "";
  msg += (e & EPOLLOUT) ? "OUT " : "";
  msg += (e & EPOLLERR) ? "ERR " : "";
  msg += (e & EPOLLHUP) ? "HUP " : "";
  msg += (e & EPOLLRDHUP) ? "RDHUP " : "";

  return msg;
  // LOG_TRACE << "poll fd " << fd << ", events " << msg;
}

Status IOService::Add(const Service *srv, int fd, unsigned int events) {
  LOG_TRACE << "IOService::Add " << srv->name() << ",fd " << fd;

  assert(fd >= 0);
  assert(srv->system() == system_);
  assert(srv->handle().index());

  system_->SetServiceFD(srv, fd);
  struct epoll_event ev;
  ev.data.u64 = static_cast<uint64_t>(srv->handle().index());
  ev.events = events;
  if (epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, fd, &ev) == -1) {
    LOG_WARN << "epoll_ctl EPOLL_CTL_ADD error:" << ERRNO_TEXT;
    return Status(kFailed, ERRNO_TEXT);
  }
  return Status::OK();
}

Status IOService::Remove(const Service *srv, int fd) {
  LOG_TRACE << "IOService::Remove " << srv->name() << ",fd " << fd;
  assert(fd >= 0);
  assert(srv->system() == system_);
  assert(system_->GetServiceFD(srv) == fd);

  if (-1 == epoll_ctl(epoll_fd_, EPOLL_CTL_DEL, fd, NULL)) {
    LOG_INFO << "epoll_ctl EPOLL_CTL_DEL error:" << ERRNO_TEXT;
    return Status(kFailed, ERRNO_TEXT);
  }

  return Status::OK();
}

void IOService::Run() {
  const int max_events = 32;
  struct epoll_event events[max_events];

  while (!this_thread::IsInterrupted()) {
    int nfds = epoll_wait(epoll_fd_, events, max_events, -1);
    if (nfds == -1) {
      if (errno != EINTR) {
        LOG_WARN << "IOService stop, error " << ERRNO_TEXT;
        return;
      }
    }

    for (int i = 0; i < nfds; ++i) {
      if (static_cast<int64_t>(events[i].data.u64) < 0) {
        LOG_INFO << "IOService stop";
        return;
      }

      auto srv = system_->GrabService(ServiceHandle(
          static_cast<ServiceHandle::IndexType>(events[i].data.u64)));
      if (srv) {
        int fd = system_->GetServiceFD(srv.get());
        CHECK_GE(fd, 0);
        epoll_ctl(epoll_fd_, EPOLL_CTL_DEL, fd, NULL);
        system_->OnIOReady(srv, fd, events[i].events);
      }
    }
  }
}

}  // namespace mcast
