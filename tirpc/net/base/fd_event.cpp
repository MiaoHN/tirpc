#include "tirpc/net/base/fd_event.hpp"

#include <fcntl.h>
#include <sys/epoll.h>

#include "tirpc/common/log.hpp"
#include "tirpc/common/mutex.hpp"
#include "tirpc/net/base/reactor.hpp"

namespace tirpc {

static FdEventContainer *g_fd_container = nullptr;

FdEvent::FdEvent(Reactor *reactor, int fd) : fd_(fd), reactor_(reactor) {
  if (reactor == nullptr) {
    ErrorLog << "create reactor first";
  }
}

FdEvent::FdEvent(int fd) : fd_(fd) {}

FdEvent::~FdEvent() = default;

void FdEvent::HandleEvent(int flag) {
  if (flag == READ) {
    read_callback_();
  }

  if (flag == WRITE) {
    write_callback_();
  }

  ErrorLog << "unknow event";
}

void FdEvent::SetCallBack(IOEvent flag, std::function<void()> cb) {
  if (flag == READ) {
    read_callback_ = cb;
  } else if (flag == WRITE) {
    write_callback_ = cb;
  } else {
    ErrorLog << "unknow event";
  }
}

auto FdEvent::GetCallBack(IOEvent flag) const -> std::function<void()> {
  if (flag == READ) {
    return read_callback_;
  }
  if (flag == WRITE) {
    return write_callback_;
  }
  ErrorLog << "unknow event";
  return nullptr;
}

void FdEvent::AddListenEvents(IOEvent event) {
  if (listen_events_ & event) {
    DebugLog << "already has this event, skip";
    return;
  }
  listen_events_ |= event;
  UpdateToReactor();
}

void FdEvent::DelListenEvents(IOEvent event) {
  if ((listen_events_ & event) != 0U) {
    DebugLog << "delete succ";
    listen_events_ &= ~event;
    UpdateToReactor();
    return;
  }
  DebugLog << "this event not exist, skip";
}

void FdEvent::UpdateToReactor() {
  epoll_event event;
  event.events = listen_events_;
  event.data.ptr = this;

  if (reactor_ == nullptr) {
    reactor_ = Reactor::GetReactor();
  }
  reactor_->AddEvent(fd_, event);
}

void FdEvent::UnregisterFromReactor() {
  if (reactor_ == nullptr) {
    reactor_ = Reactor::GetReactor();
  }
  reactor_->DelEvent(fd_);
  listen_events_ = 0;
  read_callback_ = nullptr;
  write_callback_ = nullptr;
}

auto FdEvent::GetFd() const -> int { return fd_; }

void FdEvent::SetFd(int fd) { fd_ = fd; }

auto FdEvent::GetListenEvents() const -> int { return listen_events_; }

auto FdEvent::GetReactor() const -> Reactor * { return reactor_; }

void FdEvent::SetReactor(Reactor *r) { reactor_ = r; }

void FdEvent::SetNonBlock() {
  if (fd_ == -1) {
    ErrorLog << "fd is -1";
    return;
  }

  int flag = fcntl(fd_, F_GETFL, 0);
  if (flag & O_NONBLOCK) {
    DebugLog << "already nonblock";
    return;
  }

  fcntl(fd_, F_SETFL, flag | O_NONBLOCK);
  flag = fcntl(fd_, F_GETFL, 0);
  if (flag & O_NONBLOCK) {
    DebugLog << "set nonblock succ";
  } else {
    ErrorLog << "set nonblock fail";
  }
}

auto FdEvent::IsNonBlock() -> bool {
  if (fd_ == -1) {
    ErrorLog << "fd is -1";
    return false;
  }

  int flag = fcntl(fd_, F_GETFL, 0);
  return (flag & O_NONBLOCK) != 0;
}

void FdEvent::SetCoroutine(Coroutine *cor) { cor_ = cor; }

auto FdEvent::GetCoroutine() -> Coroutine * { return cor_; }

void FdEvent::ClearCoroutine() { cor_ = nullptr; }

auto FdEventContainer::GetFdEvent(int fd) -> FdEvent::ptr {
  RWMutex::ReadLocker rlock(mutex_);
  if (fd < static_cast<int>(fds_.size())) {
    FdEvent::ptr ptr = fds_[fd];
    rlock.Unlock();
    return ptr;
  }
  rlock.Unlock();

  RWMutex::WriteLocker wlock(mutex_);
  int n = static_cast<int>(fd * 1.5);
  for (int i = fds_.size(); i < n; ++i) {
    fds_.push_back(std::make_shared<FdEvent>(i));
  }
  FdEvent::ptr ptr = fds_[fd];
  wlock.Unlock();
  return ptr;
}

FdEventContainer::FdEventContainer(int size) {
  for (int i = 0; i < size; ++i) {
    fds_.push_back(std::make_shared<FdEvent>(i));
  }
}

auto FdEventContainer::GetFdContainer() -> FdEventContainer * {
  if (g_fd_container == nullptr) {
    g_fd_container = new FdEventContainer(1000);
  }
  return g_fd_container;
}

}  // namespace tirpc
