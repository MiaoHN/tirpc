#include "tirpc/net/reactor.hpp"

#include <sys/epoll.h>
#include <sys/eventfd.h>
#include <sys/socket.h>
#include <algorithm>
#include <cassert>
#include <cstring>

#include "tirpc/common/log.hpp"
#include "tirpc/common/mutex.hpp"
#include "tirpc/coroutine/coroutine.hpp"
#include "tirpc/coroutine/coroutine_hook.hpp"
#include "tirpc/net/fd_event.hpp"
#include "tirpc/net/timer.hpp"

extern read_fun_ptr_t g_sys_read_fun;    // sys read func
extern write_fun_ptr_t g_sys_write_fun;  // sys write func

namespace tirpc {

static thread_local Reactor *t_reactor_ptr = nullptr;

static thread_local int t_max_epoll_timeout = 10000;  // ms

static CoroutineTaskQueue *t_couroutine_task_queue = nullptr;

Reactor::Reactor() {
  // one thread can't create more than one reactor object!!
  // assert(t_reactor_ptr == nullptr);
  if (t_reactor_ptr != nullptr) {
    ErrorLog << "this thread has already create a reactor";
    Exit(0);
  }

  tid_ = tirpc::GetTid();

  DebugLog << "thread[" << tid_ << "] succ create a reactor object";
  t_reactor_ptr = this;

  if ((epfd_ = epoll_create(1)) <= 0) {
    ErrorLog << "start server error. epoll_create error, sys error=" << strerror(errno);
    Exit(0);
  } else {
    DebugLog << "epfd_ = " << epfd_;
  }
  // assert(epfd_ > 0);

  if ((wakeup_fd_ = eventfd(0, EFD_NONBLOCK)) <= 0) {
    ErrorLog << "start server error. event_fd error, sys error=" << strerror(errno);
    Exit(0);
  }
  DebugLog << "wakefd = " << wakeup_fd_;
  // assert(wakeup_fd_ > 0);
  AddWakeupFd();
}

Reactor::~Reactor() {
  DebugLog << "~Reactor";
  close(epfd_);
  if (timer_ != nullptr) {
    delete timer_;
    timer_ = nullptr;
  }
  t_reactor_ptr = nullptr;
}

auto Reactor::GetReactor() -> Reactor * {
  if (t_reactor_ptr == nullptr) {
    DebugLog << "Create new Reactor";
    t_reactor_ptr = new Reactor();
  }
  // DebugLog << "t_reactor_ptr = " << t_reactor_ptr;
  return t_reactor_ptr;
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
void Reactor::DelEvent(int fd, bool is_wakeup /*=true*/) {
  if (fd == -1) {
    ErrorLog << "add error. fd invalid, fd = -1";
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
  if (g_sys_write_fun(wakeup_fd_, p, 8) != 8) {
    ErrorLog << "write wakeupfd[" << wakeup_fd_ << "] error";
  }
}

// tid_ only can be writed in Reactor::Reactor, so it needn't to lock
auto Reactor::IsLoopThread() const -> bool { return tid_ == tirpc::GetTid(); }

void Reactor::AddWakeupFd() {
  int op = EPOLL_CTL_ADD;
  epoll_event event;
  event.data.fd = wakeup_fd_;
  event.events = EPOLLIN;
  if ((epoll_ctl(epfd_, op, wakeup_fd_, &event)) != 0) {
    ErrorLog << "epoo_ctl error, fd[" << wakeup_fd_ << "], errno=" << errno << ", err=" << strerror(errno);
  }
  fds_.push_back(wakeup_fd_);
}

// need't mutex, only this thread call
void Reactor::AddEventInLoopThread(int fd, epoll_event event) {
  assert(IsLoopThread());

  int op = EPOLL_CTL_ADD;
  bool is_add = true;
  // int tmp_fd = event;
  auto it = find(fds_.begin(), fds_.end(), fd);
  if (it != fds_.end()) {
    is_add = false;
    op = EPOLL_CTL_MOD;
  }

  // epoll_event event;
  // event.data.ptr = fd_event.get();
  // event.events = fd_event->getListenEvents();

  if (epoll_ctl(epfd_, op, fd, &event) != 0) {
    ErrorLog << "epoo_ctl error, fd[" << fd << "], sys errinfo = " << strerror(errno);
    return;
  }
  if (is_add) {
    fds_.push_back(fd);
  }
  DebugLog << "epoll_ctl add succ, fd[" << fd << "]";
}

// need't mutex, only this thread call
void Reactor::DelEventInLoopThread(int fd) {
  assert(IsLoopThread());

  auto it = find(fds_.begin(), fds_.end(), fd);
  if (it == fds_.end()) {
    DebugLog << "fd[" << fd << "] not in this loop";
    return;
  }
  int op = EPOLL_CTL_DEL;

  if ((epoll_ctl(epfd_, op, fd, nullptr)) != 0) {
    ErrorLog << "epoo_ctl error, fd[" << fd << "], sys errinfo = " << strerror(errno);
  }

  fds_.erase(it);
  DebugLog << "del succ, fd[" << fd << "]";
}

void Reactor::Loop() {
  assert(IsLoopThread());
  if (is_looping_) {
    // DebugLog << "this reactor is looping!";
    return;
  }

  is_looping_ = true;
  stop_ = false;

  Coroutine *first_coroutine = nullptr;

  while (!stop_) {
    const int max_events = 10;
    epoll_event re_events[max_events + 1];

    if (first_coroutine != nullptr) {
      Coroutine::Resume(first_coroutine);
      first_coroutine = nullptr;
    }

    // main reactor need't to resume coroutine in global CoroutineTaskQueue, only io thread do this work
    if (type_ != MainReactor) {
      FdEvent *ptr = nullptr;
      // ptr->setReactor(NULL);
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

    // DebugLog << "task";
    // excute tasks
    Mutex::Locker lock(mutex_);
    std::vector<std::function<void()>> tmp_tasks;
    tmp_tasks.swap(pending_tasks_);
    lock.Unlock();

    for (auto &tmp_task : tmp_tasks) {
      // DebugLog << "begin to excute task[" << i << "]";
      if (tmp_task) {
        tmp_task();
      }
      // DebugLog << "end excute tasks[" << i << "]";
    }
    // DebugLog << "to epoll_wait";
    int rt = epoll_wait(epfd_, re_events, max_events, t_max_epoll_timeout);

    // DebugLog << "epoll_wait back";

    if (rt < 0) {
      ErrorLog << "epoll_wait error, skip, errno=" << strerror(errno);
    } else {
      // DebugLog << "epoll_wait back, rt = " << rt;
      for (int i = 0; i < rt; ++i) {
        epoll_event one_event = re_events[i];

        if (one_event.data.fd == wakeup_fd_ && ((one_event.events & READ) != 0U)) {
          // wakeup
          // DebugLog << "epoll wakeup, fd=[" << wakeup_fd_ << "]";
          char buf[8];
          while (true) {
            if ((g_sys_read_fun(wakeup_fd_, buf, 8) == -1) && errno == EAGAIN) {
              break;
            }
          }

        } else {
          auto *ptr = static_cast<FdEvent *>(one_event.data.ptr);
          if (ptr != nullptr) {
            int fd = ptr->GetFd();

            if (((one_event.events & EPOLLIN) == 0U) && ((one_event.events & EPOLLOUT) == 0U)) {
              ErrorLog << "socket [" << fd << "] occur other unknow event:[" << one_event.events
                       << "], need unregister this socket";
              DelEventInLoopThread(fd);
            } else {
              // if register coroutine, pengding coroutine to common coroutine_tasks
              if (ptr->GetCoroutine() != nullptr) {
                // the first one coroutine when epoll_wait back, just directly resume by this thread, not add to global
                // CoroutineTaskQueue because every operate CoroutineTaskQueue should add mutex lock
                if (first_coroutine == nullptr) {
                  first_coroutine = ptr->GetCoroutine();
                  continue;
                }
                if (type_ == SubReactor) {
                  DelEventInLoopThread(fd);
                  ptr->SetReactor(nullptr);
                  CoroutineTaskQueue::GetCoroutineTaskQueue()->Push(ptr);
                } else {
                  // main reactor, just resume this coroutine. it is accept coroutine. and Main Reactor only have this
                  // coroutine
                  Coroutine::Resume(ptr->GetCoroutine());
                  if (first_coroutine != nullptr) {
                    first_coroutine = nullptr;
                  }
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
        pending_add_fds_.clear();

        tmp_del.swap(pending_del_fds_);
        pending_del_fds_.clear();
      }
      for (auto &i : tmp_add) {
        // DebugLog << "fd[" << (*i).first <<"] need to add";
        AddEventInLoopThread(i.first, i.second);
      }
      for (int &i : tmp_del) {
        // DebugLog << "fd[" << (*i) <<"] need to del";
        DelEventInLoopThread(i);
      }
    }
  }
  DebugLog << "reactor loop end";
  is_looping_ = false;
}

void Reactor::Stop() {
  if (!stop_ && is_looping_) {
    stop_ = true;
    Wakeup();
  }
}

void Reactor::AddTask(std::function<void()> task, bool is_wakeup /*=true*/) {
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

void Reactor::AddCoroutine(Coroutine::ptr cor, bool is_wakeup /*=true*/) {
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

auto Reactor::GetTid() -> pid_t { return tid_; }

void Reactor::SetReactorType(ReactorType type) { type_ = type; }

auto CoroutineTaskQueue::GetCoroutineTaskQueue() -> CoroutineTaskQueue * {
  if (t_couroutine_task_queue != nullptr) {
    return t_couroutine_task_queue;
  }
  t_couroutine_task_queue = new CoroutineTaskQueue();
  return t_couroutine_task_queue;
}

void CoroutineTaskQueue::Push(FdEvent *cor) {
  Mutex::Locker lock(mutex_);
  tasks_.push(cor);
  lock.Unlock();
}

auto CoroutineTaskQueue::Pop() -> FdEvent * {
  FdEvent *re = nullptr;
  Mutex::Locker lock(mutex_);
  if (!tasks_.empty()) {
    re = tasks_.front();
    tasks_.pop();
  }
  lock.Unlock();

  return re;
}

}  // namespace tirpc
