#include "TcpServer.h"

#include <string>

#include "Acceptor.h"
#include "System.h"
#include "util/Logging.h"

namespace mcast {

class TcpConnectionService : public UserThreadService {
 public:
  explicit TcpConnectionService(System* sys)
      : UserThreadService(sys, "TcpConnectionService") {}

  virtual ~TcpConnectionService() {}

  virtual void Main() override {
    func_();
  }

  void SetFunc(const std::function<void()>& f) {
    func_ = f;
  }

  TcpConnection& connection() {
    return conn_;
  }

 private:
  std::function<void()> func_;
  TcpConnection conn_;
};

bool TcpServer::Start(System* sys, uint16_t port, uint32_t connection_idle_timeout_ms) {
  std::string host_port_str = "*:";
  host_port_str += std::to_string(port);
  return Start(sys, host_port_str, connection_idle_timeout_ms);
}

bool TcpServer::Start(System* sys, const std::string& host_port_str,
                      uint32_t connection_idle_timeout_ms) {
  LOG_INFO << "TcpServer start, host_port_str: " << host_port_str;
  if (!on_new_connection_callback_) {
    LOG_WARN << "TcpServer: not call SetOnNewConnection before start";
    return false;
  }

  auto acceptor = System::CreateObject<Acceptor>();
  if (!acceptor || !acceptor->Initialize(host_port_str)) {
    Stop();
    return false;
  }

  connection_idle_timeout_ms_ = connection_idle_timeout_ms;
  acceptor->SetOnNewConnection(
      [this, sys](int fd, const net::InetAddress& peer_addr) mutable {
        if (fd >= 0) {
          auto connsrv = System::CreateService<TcpConnectionService>(sys);

          auto& conn = connsrv->connection();
          conn.set_fd(fd);
          conn.service(connsrv.get());
          net::InetAddress local_addr;
          if (net::tcp::GetLocalAddress(fd, &local_addr))
            conn.SetLocalAddress(local_addr);

          conn.SetPeerAddress(peer_addr);

          LOG_INFO << "on new connection from " << peer_addr << " to " << local_addr;
          if (!conn.SetNonBlocking()) {
            return;
          }

          connsrv->SetFunc(on_new_connection_callback_(&conn));
          if (auto connh = sys->LaunchService(std::move(connsrv))) {
            if (connection_idle_timeout_ms_ > 0) {
              if (!sys->WakeupIfWaitTimeout(connh, connection_idle_timeout_ms_)) {
                sys->StopService(connh);
              }
            }
          }
        }
      });

  acceptor_handle_ = sys->LaunchService<AcceptorService>(std::move(acceptor));
  if (!acceptor_handle_) {
    LOG_WARN << "TcpServer launch service AcceptorService failed";
    Stop();
    return false;
  }

  sys_ = sys;
  return true;
}

void TcpServer::Stop() {
  if (acceptor_handle_) {
    sys_->StopService(acceptor_handle_);
  }
}

}  // namespace mcast
