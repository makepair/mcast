#ifndef CAST_SOCSKETOPS_H_
#define CAST_SOCSKETOPS_H_

#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <sys/types.h>

#include <stdint.h>
#include <iostream>

#include "Result.h"
#include "Status.h"

namespace mcast {
namespace net {

class InetAddress {
 public:
  InetAddress();
  explicit InetAddress(struct sockaddr_in &addr);
  bool SetInetAddress(const std::string &addr, uint16_t port) {
    if (!addr.empty())
      return SetInetAddress(addr.c_str(), port);
    else
      return SetInetAddress(NULL, port);
  }

  bool SetInetAddress(const char *addr, uint16_t port);

  std::string GetIp() const;
  uint16_t GetIpPort() const;

  uint32_t GetIpNetEndian() const;
  uint16_t GetIpPortNetEndian() const;

  void set_sockaddr(struct sockaddr_in &addr) {
    addr_ = addr;
  }

  const struct sockaddr_in &sockaddr() const {
    return addr_;
  }

 private:
  struct sockaddr_in addr_;
};

std::ostream &operator<<(std::ostream &os, const InetAddress &addr);

bool SetNonBlocking(int sockfd);
bool SetReuseAddr(int sockfd);

namespace tcp {

Result<int> Socket();

Status Bind(int sockfd, const char *ip, uint16_t port);
Status Bind(int sockfd, const InetAddress &addr);

Status Listen(int sockfd);
Result<int> Accept(int sockfd);
Status Accept2(int listenfd, int *connfd, InetAddress *addr);

Status Connect(int sockfd, const char *ip, uint16_t port);
Status Connect(int sockfd, const InetAddress &addr);

Result<size_t> Recv(int sockfd, void *buf, size_t len, int flags = 0);
Result<size_t> Send(int sockfd, const void *buf, size_t len, int flags = 0);

bool SetNoDelay(int sockfd, bool bl = true);
bool ShutdownWrite(int sockfd);

Status GetLocalAddress(int sockfd, InetAddress *addr);
Status GetPeerAddress(int sockfd, InetAddress *addr);

}  // namespace tcp
}  // namespace net
}  // namespace mcast

#endif  // CAST_SOCSKETOPS_H_
