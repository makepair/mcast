#ifndef CAST_ACCEPTOR_H_
#define CAST_ACCEPTOR_H_

#include <unistd.h>

#include <atomic>
#include <functional>
#include <memory>
#include <mutex>

#include "IOService.h"
#include "Service.h"
#include "util/Logging.h"
#include "util/socketops.h"

namespace mcast {

class Acceptor : public Noncopyable {
 public:
  typedef std::function<void(int connection_fd,
                             const net::InetAddress &peer_addr)>
      OnNewConnectionCallback;

  Acceptor() = default;
  virtual ~Acceptor() {
    if (sockfd_ != -1) {
      close(sockfd_);
    }
  }

  bool Initialize(uint16_t port);
  bool Initialize(const std::string &host_port_str);

  void SetOnNewConnection(const OnNewConnectionCallback &cb) {
    on_new_conn_ = cb;
  }

  void StartAccept(Service *srv);

 private:
  int sockfd_ = -1;
  OnNewConnectionCallback on_new_conn_;
};

typedef std::unique_ptr<Acceptor> AcceptorPtr;

class AcceptorService : public UserThreadService {
 public:
  typedef std::function<void(int fd)> OnNewConnectionCallback;

  virtual ~AcceptorService() {
    LOG_TRACE << "~AcceptorService()";
  }

  explicit AcceptorService(System *sys, AcceptorPtr acceptor)
      : UserThreadService(sys, "AcceptorService"),
        acceptor_(std::move(acceptor)) {}

  virtual void Main() override {
    acceptor_->StartAccept(this);
  }

 private:
  AcceptorPtr acceptor_;
};

}  // namespace mcast

#endif  // CAST_ACCEPTOR_H_
