#include "TcpConnector.h"
#include "System.h"
#include "util/Logging.h"
#include "util/socketops.h"

namespace mcast {

Result<TcpConnectionBasePtr> TcpConnector::Connect(Service *srv, const std::string &addr,
                                                   uint16_t port) {
  typedef Result<TcpConnectionBasePtr> Result;
  auto rsock = net::tcp::Socket();
  if (!rsock)
    return rsock.status();

  int fd = rsock.get();

  if (srv) {
    TcpConnectionPtr conn(
        System::CreateSharedObject<TcpConnection>(srv, fd));  // guard fd
    auto status = conn->SetNonBlocking();
    if (!status) {
      LOG_WARN << "SetNonBlocking failed";
      return status;
    }

    net::InetAddress peer_addr;
    if (!peer_addr.SetInetAddress(addr, port)) {
      LOG_WARN << "connect to a invail ip address " << peer_addr;
      return Status(kInvailArgument, "invail ip address");
    }

    LOG_TRACE << "try connecting to " << peer_addr << ",srv name " << srv->name();

    while (true) {
      status = net::tcp::Connect(fd, peer_addr);
      if (status) {
        net::InetAddress local_addr;
        if (net::tcp::GetLocalAddress(fd, &local_addr))
          conn->SetLocalAddress(local_addr);

        conn->SetPeerAddress(peer_addr);
        return Result(std::move(conn));
      } else if (status.IsAgain()) {
        status = srv->system()->WaitIO(fd, EPOLLOUT);
        if (status) {
          return Result(std::move(conn));
        } else {
          return status;
        }
      } else {
        LOG_INFO << "connect to " << peer_addr << " failed";
        return status;
      }
    }
    assert(false);
  } else {
    TcpConnectionBasePtr conn(
        System::CreateSharedObject<TcpConnectionBase>(fd));  // guard fd

    net::InetAddress peer_addr;
    if (!peer_addr.SetInetAddress(addr, port)) {
      LOG_WARN << "connect to a invail ip address " << peer_addr;
      return Status(kInvailArgument, "invail ip address");
    }

    LOG_TRACE << "try connecting to " << peer_addr;
    auto status = net::tcp::Connect(fd, addr.c_str(), port);
    if (status) {
      net::InetAddress local_addr;
      if (net::tcp::GetLocalAddress(fd, &local_addr))
        conn->SetLocalAddress(local_addr);

      return Result(std::move(conn));
    }
    return status;
  }
}

}  // namespace mcast
