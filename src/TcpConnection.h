#ifndef CAST_TCP_CONNECTION_H_
#define CAST_TCP_CONNECTION_H_

#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include <atomic>
#include <memory>

#include "IOService.h"
#include "Service.h"

#include "TcpConnectionBase.h"

namespace mcast {

class TcpConnection : public TcpConnectionBase {
 public:
  TcpConnection() = default;
  explicit TcpConnection(Service *srv, int fd)
      : TcpConnectionBase(fd), srv_(srv) {}

  // ~TcpConnection() override {}

  Status Read(void *pbuffer, size_t buffer_size) override;
  Result<size_t> ReadSome(void *pbuffer, size_t buffer_size) override;
  Status Write(const void *pbuffer, size_t buffer_size) override;

  Service *service() const { return srv_; }
  void service(Service *s) { srv_ = s; }

 private:
  Service *srv_ = nullptr;
};

typedef std::shared_ptr<TcpConnection> TcpConnectionPtr;

}  // namespace mcast

#endif  // CAST_TCP_CONNECTION_H_
