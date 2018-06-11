#include "RpcServer.h"

#include "System.h"
#include "util/Logging.h"

namespace mcast {

using namespace google;

class InternalRpcService : public MethodCallService {
 public:
  InternalRpcService(System* sys, const std::string& name, RpcServicePtr s)
      : MethodCallService(sys, name), service_(s) {}

  void CallMethod(const protobuf::MethodDescriptor* method,
                  protobuf::RpcController* controller, const protobuf::Message* request,
                  protobuf::Message* response, const CallClosure& call_closure) {
    assert(!controller->Failed());
    // call protobuf method
    RpcClosure* rpc_closure(new RpcClosure(call_closure));
    service_->CallMethod(method, controller, request, response, rpc_closure);
  }

 private:
  RpcServicePtr service_;
};

bool RpcServer::GetRpcServiceItem(const std::string& name, Item* item) {
  std::shared_lock<std::shared_timed_mutex> lg(mutex_);
  auto it = rpc_services_.find(name);
  if (rpc_services_.end() != it) {
    *item = it->second;
    return true;
  } else {
    return false;
  }
}

Service::Handle RpcServer::AddService(RpcServicePtr rpc_srv) {
  auto name = rpc_srv->GetDescriptor()->name();
  LOG_INFO << "RpcServer AddService " << name;

  auto h = sys_->LaunchService<InternalRpcService>("InternalRpcService", rpc_srv);
  if (h) {
    Item item{h, rpc_srv};
    std::lock_guard<std::shared_timed_mutex> lg(mutex_);
    rpc_services_.emplace(name, item);
  } else {
    LOG_WARN << "RpcServer AddService " << name << " failed";
  }

  return h;
}

std::unique_ptr<protobuf::Message> RpcServer::CallServiceMethod(
    const rpc::RpcRequest& rpc_request, protobuf::RpcController* controller) {
  const std::string& service_name = rpc_request.service();
  const std::string& method_name = rpc_request.method();
  std::unique_ptr<protobuf::Message> response;

  std::string buf;
  Item item;
  if (!GetRpcServiceItem(service_name, &item)) {
    controller->SetFailed("can't find service" + service_name);
    return response;
  }

  controller->Reset();
  auto service = item.rcp_service;
  auto handle = item.srv_handle;
  assert(service);
  auto method_descriptor = service->GetDescriptor()->FindMethodByName(method_name);
  if (nullptr == method_descriptor) {
    controller->SetFailed("can't find method" + method_name);
    return response;
  }

  std::unique_ptr<protobuf::Message> request(
      service->GetRequestPrototype(method_descriptor).New());

  if (request->ParseFromString(rpc_request.argument_bytes())) {
    response.reset(service->GetResponsePrototype(method_descriptor).New());
    auto s = sys_->CallMethodWithClosure(handle, &InternalRpcService::CallMethod,
                                         method_descriptor, controller, request.get(),
                                         response.get());

    if (!s) {
      response.reset();
      controller->SetFailed(s.ErrorText());
    }
    return response;
  } else {
    controller->SetFailed("ParseFromString error");
    return response;
  }
}

bool RpcServer::Start(const std::string& host_port_str,
                      uint32_t connection_idle_timeout_ms) {
  if (!codec_) {
    codec_.reset(new DefaultRpcCodec());
  }

  tcp_server_.SetOnNewConnection([this](TcpConnection* conn) {
    return [this, conn]() mutable {
      std::string buffer;
      buffer.reserve(32);
      std::unique_ptr<RpcController> controller(new RpcController());

      while (!conn->service()->IsStopping()) {
        controller->Reset();
        std::unique_ptr<rpc::RpcRequest> rpc_request(
            rpc::RpcRequest::default_instance().New());
        auto status = codec_->ReadRequestMessage(conn, rpc_request.get());
        if (!status) {
          LOG_INFO << status.ErrorText();
          return;
        }
        std::unique_ptr<protobuf::Message> response(
            CallServiceMethod(*rpc_request, controller.get()));

        if (controller->Failed() || !response) {
          LOG_INFO << "CallServiceMethod failed: " << controller->ErrorText();
          return;
        }

        buffer.resize(response->ByteSizeLong());
        if (response->SerializeToArray(&buffer[0], static_cast<int>(buffer.size()))) {
          std::unique_ptr<rpc::RpcResponse> rpc_response(
              rpc::RpcResponse::default_instance().New());
          rpc_response->set_response_message(buffer);
          status = codec_->WriteResponeMessage(conn, *rpc_response);
          if (!status) {
            LOG_INFO << "WriteResponeMessage failed: " << status.ErrorText();
            return;
          }
        }
      }
    };
  });

  return tcp_server_.Start(sys_, host_port_str, connection_idle_timeout_ms);
}

bool RpcServer::Start(uint16_t port, uint32_t connection_idle_timeout_ms) {
  std::string host_port_str = "*:";
  host_port_str += std::to_string(port);
  return Start(host_port_str, connection_idle_timeout_ms);
}

void RpcServer::Stop() {
  tcp_server_.Stop();
}

}  // namespace mcast
