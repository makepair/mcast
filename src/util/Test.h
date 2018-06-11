#ifndef CASTOR_TEST_H_
#define CASTOR_TEST_H_

#include <chrono>
#include <condition_variable>
#include <mutex>
#include <numeric>

#include "Logging.h"
#include "Result.h"
#include "Status.h"
#include "socketops.h"

#include "gtest/gtest.h"

namespace mcast {
namespace test {

struct Test_Task {
  void Wait() {
    std::unique_lock<std::mutex> lg(mutex);
    var.wait(lg, [this] { return done; });
  }

  void Done() {
    std::unique_lock<std::mutex> lg(mutex);
    done = true;
    var.notify_all();
  }

  void Reset() {
    std::unique_lock<std::mutex> lg(mutex);
    done = false;
  }

 private:
  std::mutex mutex;
  std::condition_variable var;
  bool done = false;
};

class Timer {
 public:
  static constexpr int kMillisecondsPerSecond = 1000;
  static constexpr int kMicrosecondsPerSecond = kMillisecondsPerSecond * 1000;
  static constexpr int kNonosecondsPerSecond = kMicrosecondsPerSecond * 1000;

  void Start() { start_ = std::chrono::high_resolution_clock::now(); }

  uint64_t Elapsed() {
    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::nanoseconds diff = end - start_;
    return static_cast<uint64_t>(diff.count());
  }

  static double ToSeconds(uint64_t c) {
    return static_cast<double>(c) / kNonosecondsPerSecond;
  }

  static uint64_t ToMilliseconds(uint64_t c) { return c / 1000000; }

 private:
  decltype(std::chrono::high_resolution_clock::now()) start_;
};

}  // namespace test
}  // namespace mcast

#endif  // CASTOR_TEST_H_
