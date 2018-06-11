#include "TcpConnection.h"

#include "IOService.h"
#include "System.h"
#include "util/Logging.h"
#include "util/socketops.h"

namespace mcast {

Status TcpConnection::Read(void *pbuffer, size_t const n) {
  assert(sockfd_ >= 0);

  char *buf = reinterpret_cast<char *>(pbuffer);
  size_t read_left = n;
  while (read_left > 0) {
    // nonblocking
    auto r = net::tcp::Recv(sockfd_, buf + (n - read_left), read_left);
    if (r) {
      read_left -= r.get();
    } else if (r.status().IsAgain()) {
      auto s = srv_->WaitInput(sockfd_);
      if (!s)
        return s;
    } else if (r.status().IsInterrupt()) {
      continue;
    } else {
      return r.status();
    }
  }

  assert(read_left == 0);
  return Status::OK();
}

Result<size_t> TcpConnection::ReadSome(void *pbuffer,
                                       size_t const buffer_size) {
  assert(sockfd_ >= 0);

  if (buffer_size == 0)
    return Result<size_t>(0);

  while (true) {
    auto r = net::tcp::Recv(sockfd_, pbuffer, buffer_size);  // nonblocking
    if (r) {
      return r;
    } else if (r.status().IsAgain()) {
      auto s = srv_->WaitInput(sockfd_);
      if (!s)
        return Result<size_t>(s);
    } else if (r.status().IsInterrupt()) {
      continue;
    } else {
      return r;
    }
  }
}

Status TcpConnection::Write(const void *pbuffer, size_t const n) {
  assert(sockfd_ >= 0);

  const char *buf = reinterpret_cast<const char *>(pbuffer);
  size_t write_left = n;
  while (write_left > 0) {
    // nonblocking
    auto r = net::tcp::Send(sockfd_, buf + (n - write_left), write_left,
                            MSG_NOSIGNAL);
    if (r) {
      write_left -= r.get();
    } else if (r.status().IsAgain()) {
      auto s = srv_->WaitOutput(sockfd_);
      if (!s)
        return s;
    } else if (r.status().IsInterrupt()) {
      continue;
    } else {
      return r.status();
    }
  }

  assert(write_left == 0);
  return Status::OK();
}

}  // namespace mcast
