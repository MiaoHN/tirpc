#include "tirpc/net/tcp/tcp_connection_time_wheel.hpp"

#include <queue>
#include <vector>

#include "tirpc/net/base/timer.hpp"
#include "tirpc/net/tcp/abstract_slot.hpp"

namespace tirpc {

TcpTimeWheel::TcpTimeWheel(Reactor *reactor, int bucket_count, int interval /*= 10*/)
    : reactor_(reactor), bucket_count_(bucket_count), interval_(interval) {
  for (int i = 0; i < bucket_count; ++i) {
    std::vector<TcpConnectionSlot::ptr> tmp;
    wheel_.push(tmp);
  }

  event_ = std::make_shared<TimerEvent>(interval_ * 1000, true, std::bind(&TcpTimeWheel::LoopFunc, this));
  reactor_->GetTimer()->AddTimerEvent(event_);
}

TcpTimeWheel::~TcpTimeWheel() { reactor_->GetTimer()->DelTimerEvent(event_); }

void TcpTimeWheel::LoopFunc() {
  // LOG_DEBUG << "pop src bucket";
  wheel_.pop();
  std::vector<TcpConnectionSlot::ptr> tmp;
  wheel_.push(tmp);
  // LOG_DEBUG << "push new bucket";
}

void TcpTimeWheel::Fresh(TcpConnectionSlot::ptr slot) {
  LOG_DEBUG << "fresh connection";
  wheel_.back().emplace_back(slot);
}

}  // namespace tirpc