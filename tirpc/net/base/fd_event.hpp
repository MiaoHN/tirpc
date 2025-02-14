#pragma once

#include <sys/epoll.h>
#include <memory>
#include "tirpc/common/mutex.hpp"
#include "tirpc/coroutine/coroutine.hpp"

namespace tirpc {

enum IOEvent {
  READ = EPOLLIN,
  WRITE = EPOLLOUT,
};

class Reactor;

class FdEvent : public std::enable_shared_from_this<FdEvent> {
 public:
  using ptr = std::shared_ptr<FdEvent>;

  explicit FdEvent(Reactor *reactor, int fd = -1);

  explicit FdEvent(int fd);

  virtual ~FdEvent();

  void HandleEvent(int flag);

  void SetCallBack(IOEvent flag, std::function<void()> cb);

  auto GetCallBack(IOEvent flag) const -> std::function<void()>;

  void AddListenEvents(IOEvent event);

  void DelListenEvents(IOEvent event);

  void UpdateToReactor();

  void UnregisterFromReactor();

  auto GetFd() const -> int;

  void SetFd(int fd);

  auto GetListenEvents() const -> int;

  auto GetReactor() const -> Reactor *;

  void SetReactor(Reactor *r);

  void SetNonBlock();

  auto IsNonBlock() -> bool;

  void SetCoroutine(Coroutine *cor);

  auto GetCoroutine() -> Coroutine *;

  void ClearCoroutine();

 public:
  Mutex mutex_;

 protected:
  int fd_{-1};

  std::function<void()> read_callback_;
  std::function<void()> write_callback_;

  int listen_events_{0};

  Reactor *reactor_{nullptr};

  Coroutine *cor_{nullptr};
};

class FdEventContainer {
 public:
  explicit FdEventContainer(int size);

  auto GetFdEvent(int fd) -> FdEvent::ptr;

 public:
  static auto GetFdContainer() -> FdEventContainer *;

 private:
  RWMutex mutex_;
  std::vector<FdEvent::ptr> fds_;
};

}  // namespace tirpc