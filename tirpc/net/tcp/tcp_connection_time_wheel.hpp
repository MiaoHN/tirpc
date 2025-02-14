#pragma once

#include <queue>
#include <vector>

#include "tirpc/net/base/reactor.hpp"
#include "tirpc/net/tcp/abstract_slot.hpp"
#include "tirpc/net/base/timer.hpp"

namespace tirpc {

class TcpConnection;

class TcpTimeWheel {
 public:
  using ptr = std::shared_ptr<TcpTimeWheel>;

  using TcpConnectionSlot = AbstractSlot<TcpConnection>;

  TcpTimeWheel(Reactor *reactor, int bucket_count, int invetal = 10);

  ~TcpTimeWheel();

  void Fresh(TcpConnectionSlot::ptr slot);

  void LoopFunc();

 private:
  Reactor *reactor_{nullptr};
  int bucket_count_{0};
  int interval_{0};  // second

  TimerEvent::ptr event_;
  std::queue<std::vector<TcpConnectionSlot::ptr>> wheel_;
};

}  // namespace tirpc
