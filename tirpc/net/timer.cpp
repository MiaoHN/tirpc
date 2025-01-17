
#include "tirpc/net/timer.hpp"

#include <asm-generic/errno-base.h>
#include <sys/time.h>
#include <sys/timerfd.h>
#include <cassert>
#include <cstdint>
#include <cstring>
#include <ctime>
#include <functional>
#include <map>
#include <vector>

#include "tirpc/common/log.hpp"
#include "tirpc/common/mutex.hpp"
#include "tirpc/coroutine/coroutine_hook.hpp"

extern read_fun_ptr_t g_sys_read_fun;

namespace tirpc {

auto GetNowMs() -> int64_t {
  struct timeval tv;
  gettimeofday(&tv, nullptr);
  return tv.tv_sec * 1000 + tv.tv_usec / 1000;
}

Timer::Timer(Reactor *reactor) : FdEvent(reactor) {
  fd_ = timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK | TFD_CLOEXEC);
  DebugLog << "timer fd = " << fd_;
  if (fd_ == -1) {
    ErrorLog << "timerfd_create error";
  }
  read_callback_ = std::bind(&Timer::OnTimer, this);
  AddListenEvents(READ);
}

Timer::~Timer() {
  UnregisterFromReactor();
  close(fd_);
}

void Timer::AddTimerEvent(TimerEvent::ptr event, bool need_reset) {
  RWMutex::WriteLocker lock(mutex_);
  bool is_reset = false;
  if (pending_events_.empty()) {
    is_reset = true;
  } else {
    auto it = pending_events_.begin();
    if (event->arrive_time_ < it->second->arrive_time_) {
      is_reset = true;
    }
  }
  pending_events_.emplace(event->arrive_time_, event);
  lock.Unlock();

  if (is_reset && need_reset) {
    DebugLog << "need reset timer";
    ResetArriveTime();
  }
}

void Timer::DelTimerEvent(TimerEvent::ptr event) {
  event->is_canceled_ = true;

  RWMutex::WriteLocker lock(mutex_);
  auto begin = pending_events_.lower_bound(event->arrive_time_);
  auto end = pending_events_.upper_bound(event->arrive_time_);
  auto it = begin;
  for (; it != end; it++) {
    if (it->second == event) {
      DebugLog << "find timer event, now delete it. src arrive time=" << event->arrive_time_;
      break;
    }
  }
  if (it != pending_events_.end()) {
    pending_events_.erase(it);
  }
  lock.Unlock();
  DebugLog << "del timer event succ, origin arrive time=" << event->arrive_time_;
}

void Timer::ResetArriveTime() {
  RWMutex::ReadLocker lock(mutex_);
  std::multimap<int64_t, TimerEvent::ptr> tmp = pending_events_;
  lock.Unlock();

  if (tmp.empty()) {
    DebugLog << "no timer event, return";
    return;
  }

  int64_t now = GetNowMs();
  auto it = tmp.rbegin();
  if (it->first < now) {
    DebugLog << "all timer events have expired, now=" << now << ", last=" << it->first;
    return;
  }
  int64_t interval = it->first - now;

  itimerspec new_value;
  bzero(&new_value, sizeof(new_value));

  timespec ts;
  bzero(&ts, sizeof(ts));

  ts.tv_sec = interval / 1000;
  ts.tv_nsec = (interval % 1000) * 1000000;
  new_value.it_value = ts;

  int rt = timerfd_settime(fd_, 0, &new_value, nullptr);

  if (rt != 0) {
    ErrorLog << "timerfd_settime error, interval=" << interval;
  }
}

void Timer::OnTimer() {
  char buf[8];
  while (true) {
    if ((g_sys_read_fun(fd_, buf, sizeof(buf)) == -1) && errno == EAGAIN) {
      break;
    }
  }

  int64_t now = GetNowMs();
  RWMutex::WriteLocker lock(mutex_);
  auto it = pending_events_.begin();
  std::vector<TimerEvent::ptr> tmps;
  std::vector<std::pair<int64_t, std::function<void()>>> tasks;
  for (; it != pending_events_.end(); ++it) {
    if (it->first <= now && !(it->second->is_canceled_)) {
      tmps.push_back(it->second);
      tasks.emplace_back(it->second->arrive_time_, it->second->task_);
    } else {
      break;
    }
  }

  pending_events_.erase(pending_events_.begin(), it);
  lock.Unlock();

  for (auto &tmp : tmps) {
    if (tmp->is_repeated_) {
      tmp->Reset();
      AddTimerEvent(tmp, false);
    }
  }

  ResetArriveTime();

  for (auto &i : tasks) {
    i.second();
  }
}

}  // namespace tirpc