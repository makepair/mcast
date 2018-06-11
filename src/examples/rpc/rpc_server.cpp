#include <signal.h>
#include <sys/types.h>
#include <unistd.h>
#include <cstdlib>

#include "google/protobuf/message.h"

#include "RpcServer.h"
#include "Service.h"
#include "System.h"
#include "echo_service.pb.h"
#include "util/Logging.h"

using namespace mcast;

static System sys;
static RpcServer rpc_server(&sys);

static void QuitHandler(int signo) {
  rpc_server.Stop();
  sys.Stop();
}

class RpcEchoService : public rpc::EchoService {
 public:
  void Echo(::google::protobuf::RpcController* controller,
            const ::rpc::EchoRequest* request, ::rpc::EchoResponse* response,
            ::google::protobuf::Closure* done) override {
    response->set_text(request->text());

    // call Run to closure this method and wake up the caller
    done->Run();
  }
};

int main(int argc, char* argv[]) {
  GOOGLE_PROTOBUF_VERIFY_VERSION;

  if (argc != 3) {
    LOG_WARN << "Usage: rpc_srv port threads";
    return -1;
  }

  uint16_t port = static_cast<uint16_t>(atoi(argv[1]));
  int threads = atoi(argv[2]);

  LOG_INFO << "rpc_srv start port " << port << ",threads " << threads;

  sys.Start(threads);
  rpc_server.AddService(RpcServicePtr(new RpcEchoService()));
  rpc_server.Start(port);

  signal(SIGINT, QuitHandler);
  signal(SIGTERM, QuitHandler);

  sys.WaitStop();
  LOG_INFO << "rpc_srv stop";
  rpc_server.Stop();
  sys.Stop();
}
