#ifndef CAST_RPCSERVER_H_
#define CAST_RPCSERVER_H_

#include <cassert>
#include <memory>
#include <shared_mutex>
#include <string>
#include <unordered_map>

#include "google/protobuf/descriptor.h"
#include "google/protobuf/service.h"
#include "rpc.pb.h"

#include "RpcClosure.h"
#include "RpcCodec.h"
#include "RpcController.h"
#include "TcpServer.h"
#include "util/Status.h"

namespace mcast {

typedef google::protobuf::Service RpcService;
typedef std::shared_ptr<RpcService> RpcServicePtr;

class InternalRpcService;

class RpcServer {
 public:
  explicit RpcServer(System* sys) : sys_(sys) {}

  Service::Handle AddService(RpcServicePtr rpc_srv);

  bool Start(const std::string& host_port_str, uint32_t connection_idle_timeout_ms = 0);
  bool Start(uint16_t port, uint32_t connection_idle_timeout_ms = 0);
  void Stop();

  void SetRpcCodec(RpcCodecPtr codec) { codec_ = std::move(codec); }

 private:
  struct Item {
    Service::BasicHandle<InternalRpcService> srv_handle;
    RpcServicePtr rcp_service;
  };

  bool GetRpcServiceItem(const std::string& name, Item* item);

  std::unique_ptr<google::protobuf::Message> CallServiceMethod(
      const rpc::RpcRequest& rpc_request,
      google::protobuf::RpcController* controller);

  System* sys_ = nullptr;

  std::shared_timed_mutex mutex_;
  std::unordered_map<std::string, Item> rpc_services_;

  TcpServer tcp_server_;
  RpcCodecPtr codec_;
};

}  // namespace mcast

#endif
