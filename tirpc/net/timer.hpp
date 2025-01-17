#pragma once

#include "tirpc/common/log.hpp"
#include "tirpc/common/mutex.hpp"
#include "tirpc/net/fd_event.hpp"

#include <map>
#include <memory>
#include <utility>

namespace tirpc {

auto GetNowMs() -> int64_t;

class TimerEvent {
 public:
  using ptr = std::shared_ptr<TimerEvent>;

  TimerEvent(int64_t interval, bool is_repeated, std::function<void()> task)
      : interval_(interval), is_repeated_(is_repeated), task_(std::move(task)) {
    arrive_time_ = GetNowMs() + interval_;
    DebugLog << "timeevent will occur at " << arrive_time_;
  }

  void Reset() {
    arrive_time_ = GetNowMs() + interval_;
    is_canceled_ = false;
  }

  void Wake() { is_canceled_ = false; }

  void Cancel() { is_canceled_ = true; }

  void CancelRepeated() { is_repeated_ = false; }

 public:
  int64_t arrive_time_;
  int64_t interval_;

  bool is_repeated_{false};
  bool is_canceled_{false};

  std::function<void()> task_;
};

class Timer : public FdEvent {
 public:
  using ptr = std::shared_ptr<Timer>;

  explicit Timer(Reactor *reactor);

  ~Timer() override;

  void AddTimerEvent(TimerEvent::ptr event, bool need_reset = true);

  void DelTimerEvent(TimerEvent::ptr event);

  void ResetArriveTime();

  void OnTimer();

 private:
  std::multimap<int64_t, TimerEvent::ptr> pending_events_;

  RWMutex mutex_;
};

}  // namespace tirpc