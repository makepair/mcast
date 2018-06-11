#include "Thread.h"

namespace mcast {

thread_local ThreadImpl* Thread::s_self{nullptr};

thread_local AtThreadExitCaller Thread::s_at_thread_exit;

}  // namespace mcast