#include <cstdlib>

#include "google/protobuf/message.h"

#include "RpcChannel.h"
#include "RpcClosure.h"
#include "RpcController.h"
#include "echo_service.pb.h"

using namespace mcast;

int main(int argc, char* argv[]) {
  GOOGLE_PROTOBUF_VERIFY_VERSION;

  if (argc != 3) {
    LOG_WARN << "Usage: rpc_cli host_ip port";
    return -1;
  }

  char* addr = argv[1];
  uint16_t port = static_cast<uint16_t>(atoi(argv[2]));

  RpcChannel channel;
  channel.SetConnectParameter(addr, port);
  rpc::EchoService::Stub stub(&channel);

  ::rpc::EchoRequest req;
  ::rpc::EchoResponse res;
  req.set_text("hello, world");
  LOG_INFO << "hello, world";

  RpcController controller;
  stub.Echo(&controller, &req, &res, &NullClosure::Instance());
  if (res.text() != req.text()) {
    LOG_WARN << "request failed, " << controller.ErrorText();
  } else {
    LOG_INFO << res.text() << " OK";
  }
}
