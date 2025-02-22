#pragma once

#include <sys/epoll.h>
#include <atomic>
#include <functional>
#include <map>
#include <memory>
#include <vector>

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

using CoroutineTaskQueue = LockFreeQueue<FdEvent *>;

auto GetCoroutineTaskQueue() -> CoroutineTaskQueue *;

}  // namespace tirpc