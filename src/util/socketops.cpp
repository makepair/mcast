#include "socketops.h"

#include <errno.h>
#include <fcntl.h>
#include <strings.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include "Logging.h"
#include "util.h"

namespace mcast {

namespace net {

static_assert(EAGAIN == EWOULDBLOCK, "");

InetAddress::InetAddress() {
  ::bzero(&addr_, sizeof addr_);
}

InetAddress::InetAddress(struct sockaddr_in &addr) : addr_(addr) {}

bool InetAddress::SetInetAddress(const char *ip, uint16_t port) {
  ::bzero(&addr_, sizeof addr_);
  if (ip != NULL) {
    if (::inet_pton(AF_INET, ip, &addr_.sin_addr) != 1)
      return false;
  } else {
    addr_.sin_addr.s_addr = htonl(INADDR_ANY);
  }

  addr_.sin_family = AF_INET;
  addr_.sin_port = htons(port);
  return true;
}

std::string InetAddress::GetIp() const {
  char buf[64];
  buf[0] = '\0';
  ::inet_ntop(AF_INET, &addr_.sin_addr, buf,
              static_cast<socklen_t>(sizeof addr_));

  return buf;
}

uint16_t InetAddress::GetIpPort() const {
  return ntohs(addr_.sin_port);
}

uint32_t InetAddress::GetIpNetEndian() const {
  return addr_.sin_addr.s_addr;
}

uint16_t InetAddress::GetIpPortNetEndian() const {
  return addr_.sin_port;
}

std::ostream &operator<<(std::ostream &os, const InetAddress &addr) {
  os << addr.GetIp() << ':' << addr.GetIpPort();
  return os;
}

bool SetNonBlocking(int fd) {
  int opts;
  if ((opts = fcntl(fd, F_GETFL)) < 0)
    return false;
  opts = opts | O_NONBLOCK;
  if (fcntl(fd, F_SETFL, opts) < 0)
    return false;

  return true;
}

bool SetReuseAddr(int sockfd) {
  int sockopt = 1;
  socklen_t sockopt_len = sizeof sockopt;
  if (-1 ==
      setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &sockopt, sockopt_len)) {
    return false;
  }

  return true;
}

namespace tcp {

Result<int> Socket() {
  auto s = socket(AF_INET, SOCK_STREAM | SOCK_CLOEXEC, 0);
  if (s != -1)
    return Result<int>(s);

  return Result<int>(Status(kFailed, ERRNO_TEXT));
}

Status Bind(int sockfd, const char *ip, uint16_t port) {
  InetAddress addr;
  if (addr.SetInetAddress(ip, port))
    return Bind(sockfd, addr);

  return Status(kInvailArgument);
}

Status Bind(int fd, const InetAddress &inet_addr) {
  struct sockaddr_in addr = inet_addr.sockaddr();
  auto r = bind(fd, reinterpret_cast<struct sockaddr *>(&addr), sizeof addr);
  if (r == 0)
    return Status::OK();

  return Status(kFailed, ERRNO_TEXT);
}

Status Listen(int fd) {
  auto r = listen(fd, 32);
  if (r == 0)
    return Status::OK();

  return Status(kFailed, ERRNO_TEXT);
}

Result<int> Accept(int sockfd) {
  typedef Result<int> ResultT;

  int fd = accept(sockfd, NULL, NULL);
  if (fd >= 0) {
    return ResultT(fd);
  }

  const int err = errno;
  switch (err) {
    case EAGAIN:  // case EWOULDBLOCK:
      return ResultT(Status(kAgain));
    default:
      return ResultT(Status(kFailed, ERRNO_TEXT));
  }
}

Status Accept2(int listenfd, int *connfd, InetAddress *inet_addr) {
  struct sockaddr_in addr;
  ::bzero(&addr, sizeof addr);
  socklen_t len = sizeof addr;
  int fd = accept(listenfd, reinterpret_cast<struct sockaddr *>(&addr), &len);
  if (fd >= 0) {
    *connfd = fd;
    inet_addr->set_sockaddr(addr);
    return Status();
  }

  const int err = errno;
  switch (err) {
    case EAGAIN:  // case EWOULDBLOCK:
      return Status(kAgain);
    default:
      return Status(kFailed, ERRNO_TEXT);
  }
}

Status Connect(int sockfd, const char *ip, uint16_t port) {
  InetAddress addr;
  if (addr.SetInetAddress(ip, port))
    return Connect(sockfd, addr);

  return Status(kInvailArgument);
}

Status Connect(int sockfd, const InetAddress &inet_addr) {
  struct sockaddr_in addr = inet_addr.sockaddr();
  int r =
      connect(sockfd, reinterpret_cast<struct sockaddr *>(&addr), sizeof addr);
  if (0 == r)
    return Status::OK();

  const int err = errno;
  switch (err) {
    case EISCONN:
      return Status::OK();
      break;
    case EAGAIN:
    case EINPROGRESS:
      return Status(kAgain);
      break;
    case EINTR:
      return Status(kInterrupt);
      break;
    default:
      return Status(kFailed, ERRNO_TEXT);
  }
}

bool ShutdownWrite(int sockfd) {
  return 0 == ::shutdown(sockfd, SHUT_WR);
}

Result<size_t> Recv(int fd, void *buf, size_t len, int flags) {
  typedef Result<size_t> ResultT;

  ssize_t r = recv(fd, buf, len, flags);
  if (r > 0) {
    return ResultT(static_cast<size_t>(r));
  }
  if (r == 0) {
    return ResultT(Status(kEof));
  }

  const int err = errno;
  switch (err) {
    case EAGAIN:  // case EWOULDBLOCK:
      return ResultT(Status(kAgain));
    case EINTR:
      return ResultT(Status(kInterrupt));
    default:
      return ResultT(Status(kFailed, ERRNO_TEXT));
  }
}

Result<size_t> Send(int fd, const void *buf, size_t len, int flags) {
  typedef Result<size_t> ResultT;

  ssize_t r = send(fd, buf, len, flags);
  if (r >= 0) {
    return Result<size_t>(static_cast<size_t>(r));
  }

  const int err = errno;
  switch (err) {
    case EAGAIN:  // case EWOULDBLOCK:
      return ResultT(Status(kAgain));
    case EINTR:
      return ResultT(Status(kInterrupt));
    default:
      return ResultT(Status(kFailed, ERRNO_TEXT));
  }
}

bool SetNoDelay(int sockfd, bool bl) {
  int sockopt = bl;
  socklen_t sockopt_len = sizeof sockopt;
  return 0 ==
         setsockopt(sockfd, IPPROTO_TCP, TCP_NODELAY, &sockopt, sockopt_len);
}

Status GetLocalAddress(int sockfd, InetAddress *inet_addr) {
  struct sockaddr_in addr;
  bzero(&addr, sizeof addr);
  socklen_t len = static_cast<socklen_t>(sizeof addr);
  if (::getsockname(sockfd, reinterpret_cast<struct sockaddr *>(&addr), &len) <
      0) {
    return Status(kFailed, ERRNO_TEXT);
  }

  inet_addr->set_sockaddr(addr);
  return Status();
}

Status GetPeerAddress(int sockfd, InetAddress *inet_addr) {
  struct sockaddr_in addr;
  bzero(&addr, sizeof addr);
  socklen_t len = static_cast<socklen_t>(sizeof addr);
  if (::getpeername(sockfd, reinterpret_cast<struct sockaddr *>(&addr), &len) <
      0) {
    return Status(kFailed, ERRNO_TEXT);
  }

  inet_addr->set_sockaddr(addr);
  return Status();
}

}  // namespace tcp
}  // namespace net
}  // namespace mcast
