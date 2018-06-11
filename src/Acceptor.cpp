#include "Acceptor.h"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <strings.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include "System.h"
#include "util/socketops.h"
#include "util/util.h"

namespace mcast {

struct FDGuard : public Noncopyable {
  explicit FDGuard(int *fd) : fd_(fd) {}

  ~FDGuard() {
    if (need_close_) {
      close(*fd_);
      *fd_ = -1;
    }
  }

  void Release() {
    need_close_ = false;
  }

 private:
  int *fd_;
  bool need_close_ = true;
};

bool Acceptor::Initialize(uint16_t port) {
  std::string host_port_str = "*:";
  host_port_str += std::to_string(port);
  return Initialize(host_port_str);
}

bool Acceptor::Initialize(const std::string &host_port_str) {
  std::string host;
  uint16_t port = 0;
  if (!ParseIPAddress(host_port_str, &host, &port)) {
    LOG_WARN << "ip address error " << host_port_str;
    return false;
  }

  assert(sockfd_ == -1);
  auto r = net::tcp::Socket();
  if (!r) {
    LOG_WARN << "net::tcp::Socket error " << port << "," << r.status().ErrorText();
    return false;
  }

  sockfd_ = r.get();
  assert(sockfd_ >= 0);

  FDGuard fd_guard(&sockfd_);
  if (!net::SetReuseAddr(sockfd_)) {
    LOG_WARN << "SetReuseAddr error " << port << "," << ERRNO_TEXT;
    return false;
  }

  net::tcp::SetNoDelay(sockfd_);

  if (!net::SetNonBlocking(sockfd_)) {
    LOG_WARN << "SetNonBlocking error " << port << "," << ERRNO_TEXT;
    return false;
  }

  const char *host_cstr = (host.empty() || host == "*") ? NULL : host.c_str();
  net::InetAddress addr;
  if (!addr.SetInetAddress(host_cstr, port)) {
    LOG_WARN << "bind ip address error " << addr;
    return false;
  }
  if (!net::tcp::Bind(sockfd_, addr)) {
    LOG_WARN << "bind port error " << port << "," << ERRNO_TEXT;
    return false;
  }

  if (!net::tcp::Listen(sockfd_)) {
    LOG_WARN << "Listen port error " << port << "," << ERRNO_TEXT;
    return false;
  }

  LOG_INFO << "Acceptor listen port: " << port;

  fd_guard.Release();
  return true;
}

void Acceptor::StartAccept(Service *srv) {
  LOG_INFO << srv->name() << " start accepting";

  assert(sockfd_ >= 0);
  if (sockfd_ == -1) {
    LOG_WARN << "StartAccept socket fd invail";
    return;
  }

  net::InetAddress inet_addr;
  while (!srv->IsStopping()) {
    int connfd = -1;
    auto status = net::tcp::Accept2(sockfd_, &connfd, &inet_addr);
    if (status) {
      assert(connfd >= 0);
      on_new_conn_(connfd, inet_addr);
    } else if (status.IsAgain()) {
      status = srv->system()->WaitInput(sockfd_);
      if (!status) {
        LOG_INFO << status.ErrorText();
        return;
      }
    } else if (status.IsAgain()) {
      continue;
    } else {
      LOG_WARN << status.ErrorText();
      return;
    }
  }

  LOG_INFO << "Acceptor stopping";
}

}  // namespace mcast
