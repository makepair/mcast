#include <signal.h>
#include <sys/types.h>
#include <unistd.h>
#include <cstdlib>

#include "google/protobuf/message.h"

#include "Service.h"
#include "System.h"
#include "TcpConnection.h"
#include "TcpServer.h"
#include "util/Logging.h"

using namespace mcast;

static System sys;
static TcpServer tcp_server;

static void QuitHandler(int signo) {
  tcp_server.Stop();
  sys.Stop();
}

int main(int argc, char* argv[]) {
  GOOGLE_PROTOBUF_VERIFY_VERSION;

  if (argc != 3) {
    LOG_WARN << "Usage: echo_srv port threads";
    return -1;
  }

  uint16_t port = static_cast<uint16_t>(atoi(argv[1]));
  int threads = atoi(argv[2]);

  LOG_INFO << "echo_srv start port " << port << ",threads " << threads;

  sys.Start(threads);
  tcp_server.SetOnNewConnection([](TcpConnection* conn) {
    return [conn]() mutable {
      char buf[8192];
      while (true) {
        auto s = conn->ReadSome(buf, sizeof buf);
        if (s) {
          if (!conn->Write(buf, s.get())) {
            // LOG_WARN << "Write error:" << s.satus().ErrorString();
            return;
          }
        } else {
          // LOG_WARN << "ReadSome error:" << s.status().ErrorString();
          return;
        }
      }
    };
  });

  tcp_server.Start(&sys, port);
  signal(SIGINT, QuitHandler);
  signal(SIGTERM, QuitHandler);

  sys.WaitStop();
  LOG_INFO << "echo_srv stop";
  tcp_server.Stop();
  sys.Stop();
}
