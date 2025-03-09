#pragma once

#include <sys/epoll.h>
#include <atomic>
#include <functional>
#include <map>
#include <memory>
#include <vector>

#include "tirpc/common/concurrentqueue.hpp"
#include "tirpc/common/lock_free_queue.hpp"
#include "tirpc/common/mutex.hpp"
#include "tirpc/coroutine/coroutine.hpp"

namespace tirpc {

enum ReactorType {
  /// main reactor, only set this by main thread.
  MainReactor = 1,
  /// child reactor, every io thread has one reactor.
  SubReactor = 2,
};

class FdEvent;
class Timer;

class Reactor {
 public:
  using ptr = std::shared_ptr<Reactor>;

  explicit Reactor();

  ~Reactor();

  void AddEvent(int fd, epoll_event event, bool is_wakeup = true);

  void DelEvent(int fd, bool is_wakeup = true);

  void AddTask(std::function<void()> task, bool is_wakeup = true);

  void AddTask(std::vector<std::function<void()>> tasks, bool is_wakeup = true);

  void AddCoroutine(Coroutine::ptr cor, bool is_wakeup = true);

  void SetThreadIndex(int thread_idx) { thread_idx_ = thread_idx; }

  void Wakeup();

  void Loop();

  void Stop();

  auto GetTimer() -> Timer *;

  auto GetTid() -> pid_t;

  void SetReactorType(ReactorType type);

 public:
  static auto GetReactor() -> Reactor *;

 private:
  void AddWakeupFd();

  auto IsLoopThread() const -> bool;

  void AddEventInLoopThread(int fd, epoll_event event);

  void DelEventInLoopThread(int fd);

 private:
  int epfd_{-1};
  int wakeup_fd_{-1};
  int timer_fd_{-1};
  bool stop_{false};
  bool is_looping_{false};
  bool is_init_timer_{false};

  int thread_idx_{-1};

  pid_t tid_{0};

  Mutex mutex_;

  std::vector<int> fds_;

  std::atomic<int> fd_size_;

  std::map<int, epoll_event> pending_add_fds_;
  std::vector<int> pending_del_fds_;

  std::vector<std::function<void()>> pending_tasks_;

  Timer *timer_{nullptr};

  ReactorType type_{ReactorType::SubReactor};
};

using LockFreeTaskQueue = moodycamel::ConcurrentQueue<FdEvent *>;

auto GetLockFreeTaskQueue() -> LockFreeTaskQueue *;

class CoroutineTaskQueue {
 public:
  static auto GetCoroutineTaskQueue() -> CoroutineTaskQueue *;

  CoroutineTaskQueue();

  void Push(int thread_idx, FdEvent *fd);

  /**
   * @brief Try to pop a FdEvent, if empty, return nullptr
   *
   * @param thread_idx
   * @return FdEvent*
   */
  auto PopSome(int thread_idx, int pop_cnt) -> std::vector<FdEvent *>;

  bool Empty(int thread_idx);

  /**
   * @brief Steal some tasks from other threads.
   *
   * @param thread_idx
   * @return int task count steal successfully
   */
  std::vector<FdEvent *> Steal(int thread_idx, int steal_cnt);

 private:
  std::vector<std::queue<FdEvent *>> tasks_;
  std::vector<Mutex> mutexs_;
};

}  // namespace tirpc