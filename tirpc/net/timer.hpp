#pragma once

#include "tirpc/common/log.hpp"
#include "tirpc/common/mutex.hpp"
#include "tirpc/net/fd_event.hpp"

#include <map>
#include <memory>
#include <utility>

namespace tirpc {

auto GetNowMs() -> int64_t;

/**
 * @brief 定时事件封装，记录任务是否重复、是否取消，以及创建时间等信息
 *
 */
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

/**
 * @brief 定时器，用于处理定时任务
 * Timer::OnTimer 被设置为 read_callback_
 *
 */
class Timer : public FdEvent {
 public:
  using ptr = std::shared_ptr<Timer>;

  explicit Timer(Reactor *reactor);

  ~Timer() override;

  void AddTimerEvent(TimerEvent::ptr event, bool need_reset = true);

  void DelTimerEvent(TimerEvent::ptr event);

  void ResetArriveTime();

  /**
   * @brief 处理定时器事件，当定时器触发时被调用。
   * 该函数会读取定时器文件描述符的数据，找出所有已到期且未被取消的定时器事件，
   * 处理这些事件（包括重复执行的事件），重置定时器的到达时间，并执行到期事件的任务。
   */
  void OnTimer();

 private:
  std::multimap<int64_t, TimerEvent::ptr> pending_events_;

  RWMutex mutex_;
};

}  // namespace tirpc