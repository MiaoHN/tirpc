#include "tirpc/net/base/reactor.hpp"

#include <sys/epoll.h>
#include <sys/eventfd.h>
#include <sys/socket.h>
#include <algorithm>
#include <cassert>
#include <cstring>

#include "tirpc/common/config.hpp"
#include "tirpc/common/log.hpp"
#include "tirpc/common/mutex.hpp"
#include "tirpc/coroutine/coroutine.hpp"
#include "tirpc/coroutine/coroutine_hook.hpp"
#include "tirpc/net/base/fd_event.hpp"
#include "tirpc/net/base/timer.hpp"

extern read_fun_ptr_t g_sys_read_fun;    // sys read func
extern write_fun_ptr_t g_sys_write_fun;  // sys write func

namespace tirpc {

static ConfigVar<int>::ptr g_iothread_num = Config::Lookup("iothread_num", 1, "IO thread number");
static ConfigVar<bool>::ptr g_use_lock_free = Config::Lookup("use_lock_free", false, "wheather to use lock free queue");

static thread_local Reactor *t_reactor_ptr = nullptr;

static thread_local int t_max_epoll_timeout = 10000;  // ms

static CoroutineTaskQueue *t_coroutine_task_queue = nullptr;

Reactor::Reactor() {
  // one thread can't create more than one reactor object!!
  // assert(t_reactor_ptr == nullptr);
  if (t_reactor_ptr != nullptr) {
    LOG_ERROR << "this thread has already create a reactor";
    Exit(0);
  }

  tid_ = tirpc::GetTid();

  LOG_DEBUG << "thread[" << tid_ << "] succ create a reactor object";
  t_reactor_ptr = this;

  if ((epfd_ = epoll_create(1)) <= 0) {
    LOG_ERROR << "start server error. epoll_create error, sys error=" << strerror(errno);
    Exit(0);
  } else {
    LOG_DEBUG << "epfd_ = " << epfd_;
  }
  // assert(epfd_ > 0);

  if ((wakeup_fd_ = eventfd(0, EFD_NONBLOCK)) <= 0) {
    LOG_ERROR << "start server error. event_fd error, sys error=" << strerror(errno);
    Exit(0);
  }
  LOG_DEBUG << "wakefd = " << wakeup_fd_;
  // assert(wakeup_fd_ > 0);
  AddWakeupFd();
}

Reactor::~Reactor() {
  LOG_DEBUG << "~Reactor";
  close(epfd_);
  if (timer_ != nullptr) {
    delete timer_;
    timer_ = nullptr;
  }
  t_reactor_ptr = nullptr;
}

auto Reactor::GetReactor() -> Reactor * {
  if (t_reactor_ptr == nullptr) {
    LOG_DEBUG << "Create new Reactor";
    t_reactor_ptr = new Reactor();
  }
  // LOG_DEBUG << "t_reactor_ptr = " << t_reactor_ptr;
  return t_reactor_ptr;
}

// call by other threads, need lock
void Reactor::AddEvent(int fd, epoll_event event, bool is_wakeup /*=true*/) {
  if (fd == -1) {
    LOG_ERROR << "add error. fd invalid, fd = -1";
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
    LOG_ERROR << "add error. fd invalid, fd = -1";
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
    LOG_ERROR << "write wakeupfd[" << wakeup_fd_ << "] error";
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
    LOG_ERROR << "epoo_ctl error, fd[" << wakeup_fd_ << "], errno=" << errno << ", err=" << strerror(errno);
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
    LOG_ERROR << "epoo_ctl error, fd[" << fd << "], sys errinfo = " << strerror(errno);
    return;
  }
  if (is_add) {
    fds_.push_back(fd);
  }
  LOG_DEBUG << "epoll_ctl add succ, fd[" << fd << "]";
}

// need't mutex, only this thread call
void Reactor::DelEventInLoopThread(int fd) {
  assert(IsLoopThread());

  auto it = find(fds_.begin(), fds_.end(), fd);
  if (it == fds_.end()) {
    LOG_DEBUG << "fd[" << fd << "] not in this loop";
    return;
  }
  int op = EPOLL_CTL_DEL;

  if ((epoll_ctl(epfd_, op, fd, nullptr)) != 0) {
    LOG_ERROR << "epoo_ctl error, fd[" << fd << "], sys errinfo = " << strerror(errno);
  }

  fds_.erase(it);
  LOG_DEBUG << "del succ, fd[" << fd << "]";
}

void Reactor::Loop() {
  assert(IsLoopThread());
  if (is_looping_) {
    // LOG_DEBUG << "this reactor is looping!";
    return;
  }

  is_looping_ = true;
  stop_ = false;

  while (!stop_) {
    const int max_events = 10;
    epoll_event re_events[max_events + 1];

    // 先处理完队列中的任务（只有 IOThread 做）
    if (type_ == SubReactor) {
      if (CoroutineTaskQueue::GetCoroutineTaskQueue()->Empty(thread_idx_)) {
        auto steal_tasks = CoroutineTaskQueue::GetCoroutineTaskQueue()->Steal(thread_idx_, 16);
        for (auto &task : steal_tasks) {
          if (task != nullptr) {
            task->SetReactor(this);
            Coroutine::Resume(task->GetCoroutine());
          }
        }
      } else {
        while (true) {
          auto tasks = CoroutineTaskQueue::GetCoroutineTaskQueue()->PopSome(thread_idx_, 16);
          if (tasks.empty()) {
            break;
          }
          for (auto &task : tasks) {
            task->SetReactor(this);
            Coroutine::Resume(task->GetCoroutine());
          }
        }
      }
    }

    // 执行 pending_tasks_ 中的任务
    Mutex::Locker lock1(mutex_);
    std::vector<std::function<void()>> tmp_tasks;
    tmp_tasks.swap(pending_tasks_);
    lock1.Unlock();

    for (auto &tmp_task : tmp_tasks) {
      if (tmp_task) {
        tmp_task();
      }
    }

    // epoll 等待事件
    int rt = epoll_wait(epfd_, re_events, max_events, t_max_epoll_timeout);

    if (rt < 0) {
      LOG_ERROR << "epoll_wait error, skip, errno=" << strerror(errno);
      continue;
    }

    for (int i = 0; i < rt; ++i) {
      epoll_event event = re_events[i];

      if (event.data.fd == wakeup_fd_ && (event.events & READ)) {
        // Wakeup 事件，将缓冲中的内容读完即可
        char buf[8];
        while (true) {
          if ((g_sys_read_fun(wakeup_fd_, buf, 8) == -1) && errno == EAGAIN) {
            break;
          }
        }
        continue;
      }
      auto *ptr = static_cast<FdEvent *>(event.data.ptr);

      if (ptr == nullptr) {
        continue;
      }

      int fd = ptr->GetFd();

      // 错误事件
      if (!(event.events & EPOLLIN) && !(event.events & EPOLLOUT)) {
        LOG_ERROR << "socket [" << fd << "] occur other unknow event:[" << event.events
                  << "], need unregister this socket";
        DelEventInLoopThread(fd);
        continue;
      }

      // 协程事件
      if (ptr->GetCoroutine() != nullptr) {
        if (type_ == SubReactor) {
          DelEventInLoopThread(fd);
          ptr->SetReactor(nullptr);
          CoroutineTaskQueue::GetCoroutineTaskQueue()->Push(thread_idx_, ptr);
        } else {
          // main reactor, just resume this coroutine. it is accept coroutine. and Main Reactor only have this
          // coroutine
          Coroutine::Resume(ptr->GetCoroutine());
        }
        continue;
      }

      // 定时事件

      if (fd == timer_fd_) {
        ptr->GetCallBack(READ)();
        continue;
      }

      std::function<void()> read_cb;
      std::function<void()> write_cb;
      read_cb = ptr->GetCallBack(READ);
      write_cb = ptr->GetCallBack(WRITE);

      if ((event.events & EPOLLIN) != 0U) {
        Mutex::Locker lock(mutex_);
        pending_tasks_.push_back(read_cb);
      }
      if ((event.events & EPOLLOUT) != 0U) {
        Mutex::Locker lock2(mutex_);
        pending_tasks_.push_back(write_cb);
      }
    }

    // 遍历完 re_events

    std::map<int, epoll_event> tmp_add;
    std::vector<int> tmp_del;

    {
      Mutex::Locker lock3(mutex_);
      tmp_add.swap(pending_add_fds_);
      pending_add_fds_.clear();

      tmp_del.swap(pending_del_fds_);
      pending_del_fds_.clear();
    }
    for (auto &i : tmp_add) {
      AddEventInLoopThread(i.first, i.second);
    }
    for (int &i : tmp_del) {
      DelEventInLoopThread(i);
    }
  }
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

CoroutineTaskQueue::CoroutineTaskQueue() {
  tasks_.resize(g_iothread_num->GetValue());
  mutexs_.resize(g_iothread_num->GetValue());
}

auto CoroutineTaskQueue::GetCoroutineTaskQueue() -> CoroutineTaskQueue * {
  if (t_coroutine_task_queue != nullptr) {
    return t_coroutine_task_queue;
  }
  t_coroutine_task_queue = new CoroutineTaskQueue();
  return t_coroutine_task_queue;
}

void CoroutineTaskQueue::Push(int thread_idx, FdEvent *cor) {
  Mutex::Locker lock(mutexs_[thread_idx]);
  tasks_[thread_idx].push(cor);
  lock.Unlock();
}

auto CoroutineTaskQueue::PopSome(int thread_idx, int pop_cnt) -> std::vector<FdEvent *> {
  std::vector<FdEvent *> vec;
  int cnt = 0;
  Mutex::Locker lock(mutexs_[thread_idx]);
  while (!tasks_[thread_idx].empty() && cnt != pop_cnt) {
    vec.push_back(tasks_[thread_idx].front());
    tasks_[thread_idx].pop();
    cnt++;
  }
  lock.Unlock();

  return vec;
}

bool CoroutineTaskQueue::Empty(int thread_idx) {
  Mutex::Locker lock(mutexs_[thread_idx]);
  return tasks_[thread_idx].empty();
}

std::vector<FdEvent *> CoroutineTaskQueue::Steal(int thread_idx, int steal_cnt) {
  std::vector<FdEvent *> steal_tasks;

  int cnt = 0;
  for (int i = 0; i < static_cast<int>(tasks_.size()); i++) {
    if (cnt == steal_cnt) break;
    if (i == thread_idx) continue;
    Mutex::Locker lock(mutexs_[i]);
    while (!tasks_[i].empty() && cnt < steal_cnt) {
      auto task = tasks_[i].front();
      tasks_[i].pop();
      steal_tasks.push_back(task);
      cnt++;
    }
  }
  return steal_tasks;
}

}  // namespace tirpc
