#ifndef CAST_THREADPOOL_H_
#define CAST_THREADPOOL_H_

#include <atomic>
#include <functional>
#include <mutex>
#include <queue>
#include <vector>

#include "Noncopyable.h"
#include "Thread.h"
#include "ThreadSafeQueue.h"

namespace mcast {

class SingleWorkQueuePolicy {
 public:
  typedef std::function<void()> Task;

  void Initialize(int thread_num) {}

  bool Take(Task *t) {
    return work_queue_.pop(t);
  }

  void Add(const Task &t) {
    work_queue_.push(t);
  }

  void Add(Task &&t) {
    work_queue_.push(std::move(t));
  }

  void BeforeThreadRun(int threadIndex) {}

 private:
  ThreadSafeQueue<Task> work_queue_;
};

class WorkStealingQueuePolicy {
 public:
  typedef std::function<void()> Task;

  void Initialize(int thread_num) {
    per_thread_task_queue_.resize(thread_num);
  }

  bool Take(Task *t) {
    return TakeFromLocal(t) || TakeFromGlobal(t) || TakeFromOtherThread(t);
  }

  void Add(const Task &t) {
    if (s_this_thread_task_queue == nullptr) {
      global_task_queue_.push(t);
    } else {
      s_this_thread_task_queue->push(t);
    }
  }

  void Add(Task &&t) {
    if (s_this_thread_task_queue == nullptr) {
      global_task_queue_.push(t);
    } else {
      s_this_thread_task_queue->push(t);
    }
  }

  void BeforeThreadRun(int thread_index) {
    assert(static_cast<size_t>(thread_index) < per_thread_task_queue_.size());
    s_this_thread_task_queue = &per_thread_task_queue_[thread_index];
  }

 private:
  bool TakeFromLocal(Task *t) {
    return s_this_thread_task_queue && s_this_thread_task_queue->pop(t);
  }

  bool TakeFromGlobal(Task *t) {
    return global_task_queue_.pop(t);
  }

  bool TakeFromOtherThread(Task *t) {
    for (auto &q : per_thread_task_queue_) {
      if (q.pop(t))
        return true;
    }
    return false;
  }

  static thread_local ThreadSafeQueue<Task> *s_this_thread_task_queue;
  std::vector<ThreadSafeQueue<Task>> per_thread_task_queue_;

  ThreadSafeQueue<Task> global_task_queue_;
};

template <typename WorkQueuePolicy>
class ThreadPool : Noncopyable {
 public:
  typedef typename WorkQueuePolicy::Task Task;

  typedef std::function<void(int)> onThreadStartCallback;

  void Start(int thread_num = 0);
  void Stop();

  void Submit(const Task &t) {
    policy_.Add(t);
  }

  void Submit(Task &&t) {
    policy_.Add(std::move(t));
  }

  static int ThreadNum(int thread_num_hint) {
    int num = thread_num_hint <= 0 ? std::thread::hardware_concurrency()
                                   : thread_num_hint;
    return num <= 0 ? 4 : num;
  }

  int ThreadNum() const {
    return thread_num_;
  }

  void SetOnThreadStartCallback(const onThreadStartCallback &cb) {
    on_thread_start_callback_ = cb;
  }

 private:
  void worker(int thread_index);

  // std::mutex mutex_;
  WorkQueuePolicy policy_;
  std::vector<Thread> threads_;
  int thread_num_ = 0;

  onThreadStartCallback on_thread_start_callback_;
};

template <typename WorkQueuePolicy>
void ThreadPool<WorkQueuePolicy>::Start(int thread_num_hint) {
  int const thread_num = ThreadNum(thread_num_hint);

  policy_.Initialize(thread_num);
  for (int i = 0; i < thread_num; ++i) {
    threads_.push_back(Thread().Run(std::bind(&ThreadPool::worker, this, i)));
  }

  thread_num_ = thread_num;
}

template <typename WorkQueuePolicy>
void ThreadPool<WorkQueuePolicy>::Stop() {
  // std::lock_guard<std::mutex> lg(mutex_);
  for (auto &t : threads_) {
    t.Interrupt();
    if (t.Joinable())
      t.Join();
  }

  threads_.clear();
}

template <typename WorkQueuePolicy>
void ThreadPool<WorkQueuePolicy>::worker(int thread_index) {
  Task task;
  policy_.BeforeThreadRun(thread_index);
  if (on_thread_start_callback_)
    on_thread_start_callback_(thread_index);

  while (!this_thread::IsInterrupted()) {
    if (policy_.Take(&task)) {
      task();
    } else {
      this_thread::Yield();
    }
  }
}

}  // namespace mcast

#endif  // CAST_THREADPOOL_H_