#ifndef CAST_RPCCHANNEL_H_
#define CAST_RPCCHANNEL_H_

#include "google/protobuf/descriptor.h"
#include "google/protobuf/service.h"

#include "RpcCodec.h"
#include "TcpConnection.h"

namespace mcast {

class Service;

class RpcChannel : public google::protobuf::RpcChannel {
 public:
  explicit RpcChannel(Service* srv = nullptr,
                      TcpConnectionPtr conn = TcpConnectionPtr{});

  bool SetConnectParameter(const std::string& host_port_str);
  void SetConnectParameter(const char* addr, uint16_t port);

  void SetCodec(RpcCodecPtr c) { codec_ = std::move(c); }

  virtual void CallMethod(const google::protobuf::MethodDescriptor* method,
                          google::protobuf::RpcController* controller,
                          const google::protobuf::Message* request,
                          google::protobuf::Message* response,
                          google::protobuf::Closure* done);

 private:
  Service* srv_ = nullptr;
  TcpConnectionBasePtr conn_;
  RpcCodecPtr codec_;

  std::string addr_;
  uint16_t port_;
};

}  // namespace mcast

#endif  // CAST_RPCCHANNEL_H_
