#include "RpcServer.h"

#include "gtest/gtest.h"

#include "echo_service.pb.h"

#include "RpcChannel.h"
#include "RpcClosure.h"
#include "RpcController.h"

#include "System.h"
#include "TcpConnector.h"
#include "util/Test.h"

using namespace mcast;
using namespace mcast::test;

static Test_Task test_task;

static uint16_t port = 18759;
static int thread_num = 2;

class RpcServerTest_EchoServiceImp : public rpc::EchoService {
 public:
  void Echo(::google::protobuf::RpcController* controller,
            const ::rpc::EchoRequest* request, ::rpc::EchoResponse* response,
            ::google::protobuf::Closure* done) override {
    response->set_text(request->text());

    LOG_INFO << "Rpc call echo";
    // call Run to wake up the caller
    done->Run();
  }
};

class RpcServerTest_ClientService : public UserThreadService {
 public:
  using UserThreadService::UserThreadService;

  ~RpcServerTest_ClientService() {
    LOG_TRACE << "~RpcServerTest_ClientService()";
  }

  void Main() override {
    Sleep(30);
    RpcChannel channel(this);
    channel.SetConnectParameter("127.0.0.1", port);
    rpc::EchoService::Stub stub(&channel);

    ::rpc::EchoRequest req;
    ::rpc::EchoResponse res;
    req.set_text("hello, world");

    RpcController controller;
    NullClosure done;
    stub.Echo(&controller, &req, &res, &done);
    if (controller.Failed()) {
      LOG_WARN << "controller.Failed: " << controller.ErrorText();
    }

    if (res.text() != req.text()) {
      LOG_WARN << "controller.Failed: " << controller.ErrorText();
    }
    test_task.Done();
  }
};

TEST(RpcServerTest, test) {
  System sys;
  ASSERT_TRUE(sys.Start(thread_num));

  RpcServer server(&sys);
  server.AddService(RpcServicePtr(new RpcServerTest_EchoServiceImp()));
  server.Start(port);

  sys.LaunchService<RpcServerTest_ClientService>("RpcServerTest_ClientService");
  test_task.Wait();

  RpcChannel channel;
  channel.SetConnectParameter("127.0.0.1", port);
  rpc::EchoService::Stub stub(&channel);

  ::rpc::EchoRequest req;
  ::rpc::EchoResponse res;
  req.set_text("hello, world2");

  RpcController controller;
  NullClosure done;
  stub.Echo(&controller, &req, &res, &done);
  if (res.text() != req.text()) {
    LOG_WARN << "controller.Failed: " << controller.ErrorText();
  }

  server.Stop();
  sys.Stop();
}
