#ifndef CAST_TCPCONNECTOR_H_
#define CAST_TCPCONNECTOR_H_

#include "Service.h"
#include "TcpConnection.h"
#include "util/Result.h"

namespace mcast {

class TcpConnector {
 public:
  static Result<TcpConnectionBasePtr> Connect(Service *srv,
                                              const std::string &addr,
                                              uint16_t port);
};

}  // namespace mcast

#endif  // CAST_TCPCONNECTOR_H_
