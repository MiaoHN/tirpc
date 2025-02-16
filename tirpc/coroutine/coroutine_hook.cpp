#include "tirpc/coroutine/coroutine_hook.hpp"

#include <dlfcn.h>
#include <fcntl.h>
#include <unistd.h>
#include <cassert>
#include <cstring>

#include "tirpc/common/config.hpp"
#include "tirpc/common/log.hpp"
#include "tirpc/coroutine/coroutine.hpp"

#include "tirpc/net/base/fd_event.hpp"
#include "tirpc/net/base/reactor.hpp"
#include "tirpc/net/base/timer.hpp"

#define HOOK_SYS_FUNC(name) name##_fun_ptr_t g_sys_##name##_fun = (name##_fun_ptr_t)dlsym(RTLD_NEXT, #name);

HOOK_SYS_FUNC(accept);
HOOK_SYS_FUNC(recv);
HOOK_SYS_FUNC(send);
HOOK_SYS_FUNC(read);
HOOK_SYS_FUNC(write);
HOOK_SYS_FUNC(connect);
HOOK_SYS_FUNC(sleep);

// static int g_hook_enable = false;

// static int g_max_timeout = 75000;

namespace tirpc {

extern tirpc::Config::ptr g_rpc_config;

static bool g_hook = true;

void SetHook(bool value) { g_hook = value; }

void toEpoll(tirpc::FdEvent::ptr fd_event, int events) {
  tirpc::Coroutine *cur_cor = tirpc::Coroutine::GetCurrentCoroutine();
  if (events & tirpc::IOEvent::READ) {
    DebugLog << "fd:[" << fd_event->GetFd() << "], register read event to epoll";
    // fd_event->setCallBack(tirpc::IOEvent::READ,
    // 	[cur_cor, fd_event]() {
    // 		tirpc::Coroutine::Resume(cur_cor);
    // 	}
    // );
    fd_event->SetCoroutine(cur_cor);
    fd_event->AddListenEvents(tirpc::IOEvent::READ);
  }
  if (events & tirpc::IOEvent::WRITE) {
    DebugLog << "fd:[" << fd_event->GetFd() << "], register write event to epoll";
    // fd_event->setCallBack(tirpc::IOEvent::WRITE,
    // 	[cur_cor]() {
    // 		tirpc::Coroutine::Resume(cur_cor);
    // 	}
    // );
    fd_event->SetCoroutine(cur_cor);
    fd_event->AddListenEvents(tirpc::IOEvent::WRITE);
  }
  // fd_event->updateToReactor();
}

ssize_t recv_hook(int fd, void *buf, size_t count, int flag) {
  DebugLog << "this is hook recv";
  if (tirpc::Coroutine::IsMainCoroutine()) {
    DebugLog << "hook disable, call sys recv func";
    return g_sys_recv_fun(fd, buf, count, flag);
  }

  tirpc::Reactor::GetReactor();
  // assert(reactor != nullptr);

  tirpc::FdEvent::ptr fd_event = tirpc::FdEventContainer::GetFdContainer()->GetFdEvent(fd);
  if (fd_event->GetReactor() == nullptr) {
    fd_event->SetReactor(tirpc::Reactor::GetReactor());
  }

  // if (fd_event->isNonBlock()) {
  // DebugLog << "user set nonblock, call sys func";
  // return g_sys_read_fun(fd, buf, count);
  // }

  fd_event->SetNonBlock();

  // must fitst register read event on epoll
  // because reactor should always care read event when a connection sockfd was created
  // so if first call sys read, and read return success, this fucntion will not register read event and return
  // for this connection sockfd, reactor will never care read event
  ssize_t n = g_sys_recv_fun(fd, buf, count, flag);
  if (n > 0) {
    return n;
  }

  toEpoll(fd_event, tirpc::IOEvent::READ);

  DebugLog << "recv func to yield";
  tirpc::Coroutine::Yield();

  fd_event->DelListenEvents(tirpc::IOEvent::READ);
  fd_event->ClearCoroutine();
  // fd_event->updateToReactor();

  DebugLog << "recv func yield back, now to call sys recv";
  return g_sys_recv_fun(fd, buf, count, flag);
}

ssize_t send_hook(int fd, const void *buf, size_t count, int flag) {
  DebugLog << "this is hook send";
  if (tirpc::Coroutine::IsMainCoroutine()) {
    DebugLog << "hook disable, call sys send func";
    return g_sys_send_fun(fd, buf, count, flag);
  }
  tirpc::Reactor::GetReactor();
  // assert(reactor != nullptr);

  tirpc::FdEvent::ptr fd_event = tirpc::FdEventContainer::GetFdContainer()->GetFdEvent(fd);
  if (fd_event->GetReactor() == nullptr) {
    fd_event->SetReactor(tirpc::Reactor::GetReactor());
  }

  fd_event->SetNonBlock();

  ssize_t n = g_sys_send_fun(fd, buf, count, flag);
  if (n > 0) {
    return n;
  }

  toEpoll(fd_event, tirpc::IOEvent::WRITE);

  DebugLog << "send func to yield";
  tirpc::Coroutine::Yield();

  fd_event->DelListenEvents(tirpc::IOEvent::WRITE);
  fd_event->ClearCoroutine();
  // fd_event->updateToReactor();

  DebugLog << "send func yield back, now to call sys send";
  return g_sys_send_fun(fd, buf, count, flag);
}

ssize_t read_hook(int fd, void *buf, size_t count) {
  DebugLog << "this is hook read";
  if (tirpc::Coroutine::IsMainCoroutine()) {
    DebugLog << "hook disable, call sys read func";
    return g_sys_read_fun(fd, buf, count);
  }

  tirpc::Reactor::GetReactor();
  // assert(reactor != nullptr);

  tirpc::FdEvent::ptr fd_event = tirpc::FdEventContainer::GetFdContainer()->GetFdEvent(fd);
  if (fd_event->GetReactor() == nullptr) {
    fd_event->SetReactor(tirpc::Reactor::GetReactor());
  }

  // if (fd_event->isNonBlock()) {
  // DebugLog << "user set nonblock, call sys func";
  // return g_sys_read_fun(fd, buf, count);
  // }

  fd_event->SetNonBlock();

  // must fitst register read event on epoll
  // because reactor should always care read event when a connection sockfd was created
  // so if first call sys read, and read return success, this fucntion will not register read event and return
  // for this connection sockfd, reactor will never care read event
  ssize_t n = g_sys_read_fun(fd, buf, count);
  if (n > 0) {
    return n;
  }

  toEpoll(fd_event, tirpc::IOEvent::READ);

  DebugLog << "read func to yield";
  tirpc::Coroutine::Yield();

  fd_event->DelListenEvents(tirpc::IOEvent::READ);
  fd_event->ClearCoroutine();
  // fd_event->updateToReactor();

  DebugLog << "read func yield back, now to call sys read";
  return g_sys_read_fun(fd, buf, count);
}

int accept_hook(int sockfd, struct sockaddr *addr, socklen_t *addrlen) {
  DebugLog << "this is hook accept";
  if (tirpc::Coroutine::IsMainCoroutine()) {
    DebugLog << "hook disable, call sys accept func";
    return g_sys_accept_fun(sockfd, addr, addrlen);
  }
  tirpc::Reactor::GetReactor();
  // assert(reactor != nullptr);

  tirpc::FdEvent::ptr fd_event = tirpc::FdEventContainer::GetFdContainer()->GetFdEvent(sockfd);
  if (fd_event->GetReactor() == nullptr) {
    fd_event->SetReactor(tirpc::Reactor::GetReactor());
  }

  // if (fd_event->isNonBlock()) {
  // DebugLog << "user set nonblock, call sys func";
  // return g_sys_accept_fun(sockfd, addr, addrlen);
  // }

  fd_event->SetNonBlock();

  int n = g_sys_accept_fun(sockfd, addr, addrlen);
  if (n > 0) {
    return n;
  }

  toEpoll(fd_event, tirpc::IOEvent::READ);

  DebugLog << "accept func to yield";
  tirpc::Coroutine::Yield();

  fd_event->DelListenEvents(tirpc::IOEvent::READ);
  fd_event->ClearCoroutine();

  DebugLog << "accept func yield back, now to call sys accept";
  return g_sys_accept_fun(sockfd, addr, addrlen);
}

ssize_t write_hook(int fd, const void *buf, size_t count) {
  DebugLog << "this is hook write";
  if (tirpc::Coroutine::IsMainCoroutine()) {
    DebugLog << "hook disable, call sys write func";
    return g_sys_write_fun(fd, buf, count);
  }
  tirpc::Reactor::GetReactor();
  // assert(reactor != nullptr);

  tirpc::FdEvent::ptr fd_event = tirpc::FdEventContainer::GetFdContainer()->GetFdEvent(fd);
  if (fd_event->GetReactor() == nullptr) {
    fd_event->SetReactor(tirpc::Reactor::GetReactor());
  }

  // if (fd_event->isNonBlock()) {
  // DebugLog << "user set nonblock, call sys func";
  // return g_sys_write_fun(fd, buf, count);
  // }

  fd_event->SetNonBlock();

  ssize_t n = g_sys_write_fun(fd, buf, count);
  if (n > 0) {
    return n;
  }

  toEpoll(fd_event, tirpc::IOEvent::WRITE);

  DebugLog << "write func to yield";
  tirpc::Coroutine::Yield();

  fd_event->DelListenEvents(tirpc::IOEvent::WRITE);
  fd_event->ClearCoroutine();
  // fd_event->updateToReactor();

  DebugLog << "write func yield back, now to call sys write";
  return g_sys_write_fun(fd, buf, count);
}

int connect_hook(int sockfd, const struct sockaddr *addr, socklen_t addrlen) {
  DebugLog << "this is hook connect";
  if (tirpc::Coroutine::IsMainCoroutine()) {
    DebugLog << "hook disable, call sys connect func";
    return g_sys_connect_fun(sockfd, addr, addrlen);
  }
  tirpc::Reactor *reactor = tirpc::Reactor::GetReactor();
  // assert(reactor != nullptr);

  tirpc::FdEvent::ptr fd_event = tirpc::FdEventContainer::GetFdContainer()->GetFdEvent(sockfd);
  if (fd_event->GetReactor() == nullptr) {
    fd_event->SetReactor(reactor);
  }
  tirpc::Coroutine *cur_cor = tirpc::Coroutine::GetCurrentCoroutine();

  // if (fd_event->isNonBlock()) {
  // DebugLog << "user set nonblock, call sys func";
  // return g_sys_connect_fun(sockfd, addr, addrlen);
  // }

  fd_event->SetNonBlock();
  int n = g_sys_connect_fun(sockfd, addr, addrlen);
  if (n == 0) {
    DebugLog << "direct connect succ, return";
    return n;
  } else if (errno != EINPROGRESS) {
    DebugLog << "connect error and errno is't EINPROGRESS, errno=" << errno << ",error=" << strerror(errno);
    return n;
  }

  DebugLog << "errno == EINPROGRESS";

  toEpoll(fd_event, tirpc::IOEvent::WRITE);

  bool is_timeout = false;  // 是否超时

  // 超时函数句柄
  auto timeout_cb = [&is_timeout, cur_cor]() {
    // 设置超时标志，然后唤醒协程
    is_timeout = true;
    tirpc::Coroutine::Resume(cur_cor);
  };

  tirpc::TimerEvent::ptr event =
      std::make_shared<tirpc::TimerEvent>(g_rpc_config->max_connect_timeout_, false, timeout_cb);

  tirpc::Timer *timer = reactor->GetTimer();
  timer->AddTimerEvent(event);

  tirpc::Coroutine::Yield();

  // write事件需要删除，因为连接成功后后面会重新监听该fd的写事件。
  fd_event->DelListenEvents(tirpc::IOEvent::WRITE);
  fd_event->ClearCoroutine();
  // fd_event->updateToReactor();

  // 定时器也需要删除
  timer->DelTimerEvent(event);

  n = g_sys_connect_fun(sockfd, addr, addrlen);
  if ((n < 0 && errno == EISCONN) || n == 0) {
    DebugLog << "connect succ";
    return 0;
  }

  if (is_timeout) {
    ErrorLog << "connect error,  timeout[ " << g_rpc_config->max_connect_timeout_ << "ms]";
    errno = ETIMEDOUT;
  }

  DebugLog << "connect error and errno=" << errno << ", error=" << strerror(errno);
  return -1;
}

unsigned int sleep_hook(unsigned int seconds) {
  DebugLog << "this is hook sleep";
  if (tirpc::Coroutine::IsMainCoroutine()) {
    DebugLog << "hook disable, call sys sleep func";
    return g_sys_sleep_fun(seconds);
  }

  tirpc::Coroutine *cur_cor = tirpc::Coroutine::GetCurrentCoroutine();

  bool is_timeout = false;
  auto timeout_cb = [cur_cor, &is_timeout]() {
    DebugLog << "onTime, now resume sleep cor";
    is_timeout = true;
    // 设置超时标志，然后唤醒协程
    tirpc::Coroutine::Resume(cur_cor);
  };

  tirpc::TimerEvent::ptr event = std::make_shared<tirpc::TimerEvent>(1000 * seconds, false, timeout_cb);

  tirpc::Reactor::GetReactor()->GetTimer()->AddTimerEvent(event);

  DebugLog << "now to yield sleep";
  // beacuse read or wirte maybe resume this coroutine, so when this cor be resumed, must check is timeout, otherwise
  // should yield again
  while (!is_timeout) {
    tirpc::Coroutine::Yield();
  }

  // 定时器也需要删除
  // tirpc::Reactor::GetReactor()->getTimer()->delTimerEvent(event);

  return 0;
}

}  // namespace tirpc

extern "C" {

int accept(int sockfd, struct sockaddr *addr, socklen_t *addrlen) {
  if (!tirpc::g_hook) {
    return g_sys_accept_fun(sockfd, addr, addrlen);
  } else {
    return tirpc::accept_hook(sockfd, addr, addrlen);
  }
}

ssize_t recv(int fd, void *buf, size_t count, int flag) {
  if (!tirpc::g_hook) {
    return g_sys_recv_fun(fd, buf, count, flag);
  } else {
    return tirpc::send_hook(fd, buf, count, flag);
  }
}

ssize_t send(int fd, const void *buf, size_t count, int flag) {
  if (!tirpc::g_hook) {
    return g_sys_send_fun(fd, buf, count, flag);
  } else {
    return tirpc::send_hook(fd, buf, count, flag);
  }
}

ssize_t read(int fd, void *buf, size_t count) {
  if (!tirpc::g_hook) {
    return g_sys_read_fun(fd, buf, count);
  } else {
    return tirpc::read_hook(fd, buf, count);
  }
}

ssize_t write(int fd, const void *buf, size_t count) {
  if (!tirpc::g_hook) {
    return g_sys_write_fun(fd, buf, count);
  } else {
    return tirpc::write_hook(fd, buf, count);
  }
}

int connect(int sockfd, const struct sockaddr *addr, socklen_t addrlen) {
  if (!tirpc::g_hook) {
    return g_sys_connect_fun(sockfd, addr, addrlen);
  } else {
    return tirpc::connect_hook(sockfd, addr, addrlen);
  }
}

unsigned int sleep(unsigned int seconds) {
  if (!tirpc::g_hook) {
    return g_sys_sleep_fun(seconds);
  } else {
    return tirpc::sleep_hook(seconds);
  }
}
}
