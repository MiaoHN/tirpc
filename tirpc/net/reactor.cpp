#include "tirpc/net/reactor.hpp"

#include <sys/eventfd.h>
#include <sys/timerfd.h>
#include <algorithm>
#include <cassert>
#include <cstring>

#include "tirpc/common/log.hpp"
#include "tirpc/coroutine/coroutine.hpp"
#include "tirpc/coroutine/coroutine_hook.hpp"
#include "tirpc/net/fd_event.hpp"
#include "tirpc/net/timer.hpp"

extern read_fun_ptr_t g_sys_read_fun;    // sys read func
extern write_fun_ptr_t g_sys_write_fun;  // sys write func

namespace tirpc {

static thread_local Reactor *t_reactor = nullptr;

static thread_local int t_max_epoll_timeout = 10000;  // ms

static thread_local CoroutineTaskQueue *t_coroutine_task_queue = nullptr;

Reactor::Reactor() {
  if (t_reactor != nullptr) {
    ErrorLog << "reactor already exist";
    Exit(0);
  }

  tid_ = GetTid();

  DebugLog << "thread[" << tid_ << "] create reactor succ";

  t_reactor = this;

  if ((epfd_ = epoll_create(1)) <= 0) {
    ErrorLog << "start server error. epoll_create error, sys error=" << strerror(errno);
    Exit(0);
  } else {
    DebugLog << "epfd_ = " << epfd_;
  }

  if ((wakeup_fd_ = eventfd(0, EFD_NONBLOCK)) <= 0) {
    ErrorLog << "start server error. eventfd error, sys error=" << strerror(errno);
    Exit(0);
  }
  DebugLog << "wakeup_fd_ = " << wakeup_fd_;

  AddWakeupFd();
}

Reactor::~Reactor() {
  DebugLog << "~Reactor()";
  close(epfd_);
  close(wakeup_fd_);
  if (timer_ != nullptr) {
    delete timer_;
    timer_ = nullptr;
  }
  t_reactor = nullptr;
}

auto Reactor::GetReactor() -> Reactor * {
  if (t_reactor == nullptr) {
    DebugLog << "create new reactor";
    t_reactor = new Reactor();
  }
  return t_reactor;
}

// call by other threads, need lock
void Reactor::AddEvent(int fd, epoll_event event, bool is_wakeup /*=true*/) {
  if (fd == -1) {
    ErrorLog << "add error. fd invalid, fd = -1";
    return;
  }
  if (IsLoopThread()) {
    AddEventInLoopThread(fd, event);
    return;
  }
  {
    Mutex::Locker lock(mutex_);
    pending_add_fds_.insert(std::pair<int, epoll_event>(fd, event));
  }
  if (is_wakeup) {
    Wakeup();
  }
}

// call by other threads, need lock
void Reactor::DelEvent(int fd, bool is_wakeup) {
  if (fd == -1) {
    ErrorLog << "addr error. fd invalid, fd is -1";
    return;
  }

  if (IsLoopThread()) {
    DelEventInLoopThread(fd);
    return;
  }

  {
    Mutex::Locker lock(mutex_);
    pending_del_fds_.push_back(fd);
  }
  if (is_wakeup) {
    Wakeup();
  }
}

void Reactor::Wakeup() {
  if (!is_looping_) {
    return;
  }

  uint64_t tmp = 1;
  uint64_t *p = &tmp;
  if (g_sys_write_fun(wakeup_fd_, p, sizeof(uint64_t)) != sizeof(uint64_t)) {
    ErrorLog << "write wakeupfd[" << wakeup_fd_ << "] error, sys error=" << strerror(errno);
  }
}

auto Reactor::IsLoopThread() const -> bool { return tid_ == GetTid(); }

void Reactor::AddWakeupFd() {
  int op = EPOLL_CTL_ADD;
  epoll_event event;
  event.data.fd = wakeup_fd_;
  event.events = EPOLLIN;
  if ((epoll_ctl(epfd_, op, wakeup_fd_, &event)) != 0) {
    ErrorLog << "epoll_ctl error, fd[" << wakeup_fd_ << "], errno=" << errno << ", err=" << strerror(errno);
    Exit(0);
  }
  fds_.push_back(wakeup_fd_);
}

void Reactor::AddEventInLoopThread(int fd, epoll_event event) {
  assert(IsLoopThread());
  int op = EPOLL_CTL_ADD;
  bool is_add = true;
  auto it = find(fds_.begin(), fds_.end(), fd);
  if (it != fds_.end()) {
    is_add = false;
    op = EPOLL_CTL_MOD;
  }

  if (epoll_ctl(epfd_, op, fd, &event) != 0) {
    ErrorLog << "epoll_ctl error, fd[" << fd << "], errno=" << errno << ", err=" << strerror(errno);
    return;
  }
  if (is_add) {
    fds_.push_back(fd);
  }
  DebugLog << "epoll_ctl succ, fd[" << fd << "], op=" << op;
}

void Reactor::DelEventInLoopThread(int fd) {
  assert(IsLoopThread());

  auto it = find(fds_.begin(), fds_.end(), fd);
  if (it == fds_.end()) {
    ErrorLog << "fd[" << fd << "] not in this look";
    return;
  }
  int op = EPOLL_CTL_DEL;

  if (epoll_ctl(epfd_, op, fd, nullptr) != 0) {
    ErrorLog << "epoll_ctl error, fd[" << fd << "], errno=" << errno << ", err=" << strerror(errno);
  }

  fds_.erase(it);
  DebugLog << "del succ, fd[" << fd << "]";
}

void Reactor::Loop() {
  assert(IsLoopThread());

  if (is_looping_) {
    return;
  }

  is_looping_ = true;
  stop_ = false;

  Coroutine *first_cor = nullptr;

  while (!stop_) {
    const int max_events = 10;
    epoll_event re_events[max_events + 1];

    if (first_cor != nullptr) {
      Coroutine::Resume(first_cor);
      first_cor = nullptr;
    }

    // main reactor need't to resume coroutine in global coroutine task quene, only io threads do this work
    if (type_ != MainReactor) {
      FdEvent *ptr = nullptr;
      while (true) {
        ptr = CoroutineTaskQueue::GetCoroutineTaskQueue()->Pop();
        if (ptr != nullptr) {
          ptr->SetReactor(this);
          Coroutine::Resume(ptr->GetCoroutine());
        } else {
          break;
        }
      }
    }

    std::vector<std::function<void()>> tmp_tasks;
    {
      Mutex::Locker lock(mutex_);
      tmp_tasks.swap(pending_tasks_);
    }

    for (auto &tmp_task : tmp_tasks) {
      if (tmp_task) {
        tmp_task();
      }
    }

    int rt = epoll_wait(epfd_, re_events, max_events, t_max_epoll_timeout);

    if (rt < 0) {
      ErrorLog << "epoll_wait error, skip, errno=" << errno << ", err=" << strerror(errno);
      continue;
    }

    for (int i = 0; i < rt; ++i) {
      epoll_event one_event = re_events[i];

      if (one_event.data.fd == wakeup_fd_ && ((one_event.events & READ) != 0U)) {
        char buf[8];
        while (true) {
          if ((g_sys_read_fun(wakeup_fd_, buf, sizeof(buf)) == -1) && errno == EAGAIN) {
            break;
          }
        }
      } else {
        auto *ptr = static_cast<FdEvent *>(one_event.data.ptr);
        if (ptr != nullptr) {
          int fd = ptr->GetFd();

          if (((one_event.events & EPOLLIN) == 0U) && ((one_event.events & EPOLLOUT) == 0U)) {
            ErrorLog << "socket [" << fd << "] occur other unknown event[" << one_event.events
                     << "], need unregister this socket";
            DelEventInLoopThread(fd);
          } else {
            // if register coroutine, pending coroutine to common coroutine_tasks
            if (ptr->GetCoroutine() != nullptr) {
              // the first one coroutine when epoll_wait back, just directly resume by this thread, not add to global
              // CoroutineTaskQueue because every operate CoroutineTaskQueue should add mutex lock
              if (first_cor == nullptr) {
                first_cor = ptr->GetCoroutine();
                continue;
              }
              if (type_ == SubReactor) {
                DelEventInLoopThread(fd);
                ptr->SetReactor(nullptr);
                CoroutineTaskQueue::GetCoroutineTaskQueue()->Push(ptr);
              } else {
                Coroutine::Resume(ptr->GetCoroutine());

                first_cor = nullptr;
              }
            } else {
              std::function<void()> read_cb;
              std::function<void()> write_cb;
              read_cb = ptr->GetCallBack(READ);
              write_cb = ptr->GetCallBack(WRITE);
              // if timer event, direct excute
              if (fd == timer_fd_) {
                read_cb();
                continue;
              }
              if ((one_event.events & EPOLLIN) != 0U) {
                // DebugLog << "socket [" << fd << "] occur read event";
                Mutex::Locker lock(mutex_);
                pending_tasks_.push_back(read_cb);
              }
              if ((one_event.events & EPOLLOUT) != 0U) {
                // DebugLog << "socket [" << fd << "] occur write event";
                Mutex::Locker lock(mutex_);
                pending_tasks_.push_back(write_cb);
              }
            }
          }
        }
      }
    }

    std::map<int, epoll_event> tmp_add;
    std::vector<int> tmp_del;

    {
      Mutex::Locker lock(mutex_);
      tmp_add.swap(pending_add_fds_);
      tmp_del.swap(pending_del_fds_);
      pending_add_fds_.clear();
      pending_del_fds_.clear();
    }

    for (const auto &it : tmp_add) {
      AddEventInLoopThread(it.first, it.second);
    }

    for (const auto &it : tmp_del) {
      DelEventInLoopThread(it);
    }
  }
  DebugLog << "reactor loop exit";
  is_looping_ = false;
}

void Reactor::Stop() {
  if (!stop_ && is_looping_) {
    stop_ = true;
    Wakeup();
  }
}

void Reactor::AddTask(std::function<void()> &&task, bool is_wakeup /*=true*/) {
  {
    Mutex::Locker lock(mutex_);
    pending_tasks_.push_back(task);
  }
  if (is_wakeup) {
    Wakeup();
  }
}

void Reactor::AddTask(std::vector<std::function<void()>> task, bool is_wakeup /* =true*/) {
  if (task.empty()) {
    return;
  }

  {
    Mutex::Locker lock(mutex_);
    pending_tasks_.insert(pending_tasks_.end(), task.begin(), task.end());
  }
  if (is_wakeup) {
    Wakeup();
  }
}

void Reactor::AddCoroutine(const Coroutine::ptr &cor, bool is_wakeup /*=true*/) {
  auto func = [cor]() { Coroutine::Resume(cor.get()); };
  AddTask(func, is_wakeup);
}

auto Reactor::GetTimer() -> Timer * {
  if (timer_ == nullptr) {
    timer_ = new Timer(this);
    timer_fd_ = timer_->GetFd();
  }
  return timer_;
}

auto Reactor::GetTid() const -> pid_t { return tid_; }

void Reactor::SetReactorType(ReactorType type) { type_ = type; }

auto CoroutineTaskQueue::GetCoroutineTaskQueue() -> CoroutineTaskQueue * {
  if (t_coroutine_task_queue != nullptr) {
    return t_coroutine_task_queue;
  }
  t_coroutine_task_queue = new CoroutineTaskQueue();
  return t_coroutine_task_queue;
}

void CoroutineTaskQueue::Push(FdEvent *fd) {
  Mutex::Locker lock(mutex_);
  tasks_.push(fd);
}

auto CoroutineTaskQueue::Pop() -> FdEvent * {
  FdEvent *re = nullptr;
  Mutex::Locker lock(mutex_);
  if (!tasks_.empty()) {
    re = tasks_.front();
    tasks_.pop();
  }

  return re;
}

}  // namespace tirpc