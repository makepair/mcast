#include "ThreadPool.h"

namespace mcast {

thread_local ThreadSafeQueue<WorkStealingQueuePolicy::Task>
    *WorkStealingQueuePolicy::s_this_thread_task_queue = nullptr;

}  // namespace mcast