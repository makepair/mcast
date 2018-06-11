#include "RpcChannel.h"

#include <string>

#include "Service.h"
#include "System.h"
#include "TcpConnector.h"

namespace mcast {

RpcChannel::RpcChannel(Service* srv, TcpConnectionPtr conn)
    : srv_(srv), conn_(std::move(conn)) {
  codec_.reset(new DefaultRpcCodec());
}

bool RpcChannel::SetConnectParameter(const std::string& host_port_str) {
  std::string host;
  uint16_t port = 0;
  if (!ParseIPAddress(host_port_str, &host, &port)) {
    return false;
  }

  SetConnectParameter(host.c_str(), port);
  return true;
}

void RpcChannel::SetConnectParameter(const char* addr, uint16_t port) {
  addr_ = addr;
  port_ = port;
  conn_.reset();
}

void RpcChannel::CallMethod(const google::protobuf::MethodDescriptor* method,
                            google::protobuf::RpcController* controller,
                            const google::protobuf::Message* request,
                            google::protobuf::Message* response,
                            google::protobuf::Closure* done) {
  if (!conn_) {
    Result<TcpConnectionBasePtr> res =
        TcpConnector::Connect(srv_, addr_, port_);
    if (!res) {
      controller->SetFailed(res.status().ErrorText());
      done->Run();
      return;
    }

    conn_ = res.get();
  }

  assert(conn_);
  rpc::RpcRequest rpc_req;
  rpc_req.set_service(method->service()->name());
  rpc_req.set_method(method->name());

  Status status;
  if (request->SerializeToString(rpc_req.mutable_argument_bytes())) {
    if ((status = codec_->WriteRequestMessage(conn_.get(), rpc_req))) {
      rpc::RpcResponse rpc_res;
      if ((status = codec_->ReadResponeMessage(conn_.get(), &rpc_res))) {
        if (response->ParseFromString(rpc_res.response_message())) {
          done->Run();
          return;
        }
      }
    }
  }

  conn_.reset();
  if (!status)
    controller->SetFailed(status.ErrorText());
  else
    controller->SetFailed("CallMethod SerializeToString failed");
  done->Run();
}

}  // namespace mcast
