#pragma once

#include <semaphore.h>
#include <atomic>
#include <functional>
#include <map>
#include <memory>

#include "tirpc/coroutine/coroutine.hpp"
#include "tirpc/net/reactor.hpp"
#include "tirpc/net/tcp/tcp_connection_time_wheel.hpp"

namespace tirpc {

class TcpServer;

class IOThread {
 public:
  using ptr = std::shared_ptr<IOThread>;
  IOThread();

  ~IOThread();

  auto GetReactor() -> Reactor *;

  auto GetPthreadId() -> pthread_t;

  void SetThreadIndex(int index);

  auto GetThreadIndex() -> int;

  auto GetStartSemaphore() -> sem_t *;

 public:
  static auto GetCurrentIOThread() -> IOThread *;

 private:
  static auto Main(void *arg) -> void *;

 private:
  Reactor *reactor_{nullptr};
  pthread_t thread_{0};
  pid_t tid_{-1};
  TimerEvent::ptr timer_event_{nullptr};
  int index_{-1};

  sem_t init_semaphore_;

  sem_t start_semaphore_;
};

class IOThreadPool {
 public:
  using ptr = std::shared_ptr<IOThreadPool>;

  explicit IOThreadPool(int size);

  void Start();

  auto GetIoThread() -> IOThread *;

  auto GetIoThreadPoolSize() -> int;

  void BroadcastTask(std::function<void()> cb);

  void AddTaskByIndex(int index, std::function<void()> cb);

  void AddCoroutineToRandomThread(Coroutine::ptr cor, bool self = false);

  // add a coroutine to random thread in io thread pool
  // self = false, means random thread cann't be current thread
  // please free cor, or causes memory leak
  // call returnCoroutine(cor) to free coroutine
  auto AddCoroutineToRandomThread(std::function<void()> cb, bool self = false) -> Coroutine::ptr;

  auto AddCoroutineToThreadByIndex(int index, std::function<void()> cb, bool self = false) -> Coroutine::ptr;

  void AddCoroutineToEachThread(std::function<void()> cb);

 private:
  int size_{0};

  std::atomic<int> index_{-1};

  std::vector<IOThread::ptr> io_threads_;
};

}  // namespace tirpc