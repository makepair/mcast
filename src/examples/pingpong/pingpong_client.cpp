#include <cstdlib>

#include "google/protobuf/message.h"

#include "Service.h"
#include "System.h"
#include "TcpConnection.h"
#include "TcpConnector.h"
#include "TcpServer.h"
#include "util/Logging.h"
#include "util/Thread.h"
#include "util/Timer.h"

using namespace mcast;

namespace {
char* addr = nullptr;
uint16_t port = 0;
int threads = 1;
int blocksize = 4096;
int sessions = 1;
std::string msg;
int seconds = 10;

std::atomic<size_t> bytes_written{0};
std::atomic<size_t> bytes_read{0};
std::atomic<size_t> num_connected{0};
}

class ClientService : public UserThreadService {
 public:
  using UserThreadService::UserThreadService;

  void Main() override {
    TcpConnectionBasePtr conn;
    size_t local_bytes_read = 0;
    size_t local_bytes_written = 0;

    char buf[16096];
    if (auto r = TcpConnector::Connect(this, addr, port)) {
      conn = r.get();
      ++num_connected;
      if (conn->Write(msg.c_str(), msg.size())) {
        local_bytes_written += msg.size();
        while (auto res = conn->ReadSome(buf, sizeof buf)) {
          local_bytes_read += res.get();
          if (conn->Write(buf, res.get())) {
            local_bytes_written += res.get();
          } else {
            break;
          }
        }
      }
    } else {
      LOG_WARN << "Connect failed," << addr << port;
    }

    bytes_written += local_bytes_written;
    bytes_read += local_bytes_read;
  }
};

int main(int argc, char* argv[]) {
  GOOGLE_PROTOBUF_VERIFY_VERSION;

  if (argc != 7) {
    LOG_WARN << "Usage: pingpong_cli host_ip port threads blocksize "
                "sessions time";
    return -1;
  }

  addr = argv[1];
  port = static_cast<uint16_t>(std::atoi(argv[2]));
  threads = std::atoi(argv[3]);
  blocksize = std::atoi(argv[4]);
  sessions = std::atoi(argv[5]);
  seconds = std::atoi(argv[6]);

  for (int i = 0; i < blocksize; ++i) {
    msg.push_back(static_cast<char>(i % 128));
  }

  LOG_INFO << "Running pingpong client: threads " << threads << " blocksize " << blocksize
           << " sessions " << sessions << " seconds " << seconds;

  System sys;
  sys.Start(threads);

  Timer timer;
  timer.Start();

  for (int i = 0; i < sessions; ++i) {
    if (!sys.LaunchService<ClientService>("ClientService")) {
      LOG_WARN << "LaunchService ClientService failed," << i;
    }
  }

  this_thread::SleepFor(std::chrono::seconds(seconds));
  double seds = timer.Elapsed().ToSeconds();
  LOG_WARN << seds;
  sys.Stop();

  const double M = 1024 * 1024;
  LOG_INFO << num_connected.load() << " connections connected";
  LOG_INFO << bytes_written.load() << " bytes written";
  LOG_INFO << bytes_read.load() << " bytes read";
  LOG_INFO << std::fixed << static_cast<double>(bytes_read.load()) / (seds * M)
           << " MiB/s";
}
