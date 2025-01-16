#include "tirpc/net/tcp/tcp_connection_time_wheel.hpp"

#include <queue>
#include <vector>

#include "tirpc/net/tcp/abstract_slot.hpp"
#include "tirpc/net/timer.hpp"

namespace tirpc {

TcpTimeWheel::TcpTimeWheel(Reactor *reactor, int bucket_count, int inteval /*= 10*/)
    : reactor_(reactor), bucket_count_(bucket_count), interval_(inteval) {
  for (int i = 0; i < bucket_count; ++i) {
    std::vector<TcpConnectionSlot::ptr> tmp;
    wheel_.push(tmp);
  }

  event_ = std::make_shared<TimerEvent>(interval_ * 1000, true, std::bind(&TcpTimeWheel::LoopFunc, this));
  reactor_->GetTimer()->AddTimerEvent(event_);
}

TcpTimeWheel::~TcpTimeWheel() { reactor_->GetTimer()->DelTimerEvent(event_); }

void TcpTimeWheel::LoopFunc() {
  // DebugLog << "pop src bucket";
  wheel_.pop();
  std::vector<TcpConnectionSlot::ptr> tmp;
  wheel_.push(tmp);
  // DebugLog << "push new bucket";
}

void TcpTimeWheel::Fresh(TcpConnectionSlot::ptr slot) {
  DebugLog << "fresh connection";
  wheel_.back().emplace_back(slot);
}

}  // namespace tirpc