#ifndef CAST_TCPSERVER_H_
#define CAST_TCPSERVER_H_

#include <unistd.h>

#include <functional>
#include <list>
#include <memory>

#include "IOService.h"
#include "Service.h"
#include "TcpConnection.h"

namespace mcast {

class System;

class TcpConnectionWatcher;

class TcpServer {
 public:
  typedef std::function<void()> RetType;
  typedef std::function<RetType(TcpConnection *)> OnNewConnectionCallback;

  bool Start(System *sys, const std::string &host_port_str,
             uint32_t connection_idle_timeout_ms = 0);
  bool Start(System *sys, uint16_t port, uint32_t connection_idle_timeout_ms = 0);
  void Stop();

  void SetOnNewConnection(const OnNewConnectionCallback &cb) {
    on_new_connection_callback_ = cb;
  }

 private:
  System *sys_ = nullptr;
  OnNewConnectionCallback on_new_connection_callback_;
  Service::Handle acceptor_handle_;
  uint32_t connection_idle_timeout_ms_ = 0;
};

}  // namespace mcast

#endif  // CAST_TCPSERVER_H_
