#ifndef CAST_TCP_CONNECTION_BASE_H_
#define CAST_TCP_CONNECTION_BASE_H_

#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include <memory>

#include "util/Logging.h"
#include "util/Noncopyable.h"
#include "util/Result.h"
#include "util/Status.h"
#include "util/socketops.h"

namespace mcast {

class TcpConnectionBase : public Noncopyable {
 public:
  TcpConnectionBase() = default;
  explicit TcpConnectionBase(int fd) : sockfd_(fd) {}

  virtual ~TcpConnectionBase() {
    if (sockfd_ != -1) {
      close(sockfd_);
    }
  }

  virtual Status Read(void *pbuffer, size_t buffer_size);
  virtual Result<size_t> ReadSome(void *pbuffer, size_t buffer_size);
  virtual Status Write(const void *pbuffer, size_t buffer_size);

  void set_fd(int fd) {
    sockfd_ = fd;
  }
  int fd() const {
    return sockfd_;
  }

  Status SetNonBlocking() {
    assert(sockfd_ >= 0);
    auto res = net::SetNonBlocking(sockfd_);
    if (!res)
      return Status(kFailed, "SetNonBlocking failed");

    return Status::OK();
  }

  Status SetTcpNoDelay(bool bl = true) {
    assert(sockfd_ >= 0);
    auto res = net::tcp::SetNoDelay(sockfd_, bl);
    if (!res)
      return Status(kFailed, "SetTcpNoDelay failed");

    return Status::OK();
  }

  const net::InetAddress &GetLocalAddress() const {
    return local_addr_;
  }

  void SetLocalAddress(const net::InetAddress &addr) {
    local_addr_ = addr;
  }

  const net::InetAddress &GetPeerAddress() const {
    return peer_addr_;
  }

  void SetPeerAddress(const net::InetAddress &addr) {
    peer_addr_ = addr;
  }

 protected:
  int sockfd_ = -1;
  net::InetAddress local_addr_;
  net::InetAddress peer_addr_;
};

typedef std::shared_ptr<TcpConnectionBase> TcpConnectionBasePtr;

}  // namespace mcast

#endif
