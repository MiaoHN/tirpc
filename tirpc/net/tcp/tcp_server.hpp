#pragma once

#include <google/protobuf/service.h>
#include <unordered_map>

#include "tirpc/net/base/address.hpp"
#include "tirpc/net/base/fd_event.hpp"
#include "tirpc/net/base/reactor.hpp"
#include "tirpc/net/base/timer.hpp"
#include "tirpc/net/tcp/abstract_codec.hpp"
#include "tirpc/net/tcp/abstract_dispatcher.hpp"
#include "tirpc/net/tcp/abstract_service_register.hpp"
#include "tirpc/net/tcp/io_thread.hpp"
#include "tirpc/net/tcp/tcp_connection.hpp"
#include "tirpc/net/tcp/tcp_connection_time_wheel.hpp"

namespace tirpc {

class TcpAcceptor {
 public:
  using ptr = std::shared_ptr<TcpAcceptor>;
  explicit TcpAcceptor(Address::ptr net_addr);

  void Init();

  auto ToAccept() -> int;

  ~TcpAcceptor();

  auto GetPeerAddr() -> Address::ptr { return peer_addr_; }

  auto GeLocalAddr() -> Address::ptr { return local_addr_; }

 private:
  int family_{-1};
  int fd_{-1};

  Address::ptr local_addr_{nullptr};
  Address::ptr peer_addr_{nullptr};
};

class TcpServer {
 public:
  using ptr = std::shared_ptr<TcpServer>;

  TcpServer();

  explicit TcpServer(Address::ptr addr);

  ~TcpServer();

  void Start();

  void AddCoroutine(Coroutine::ptr cor);

  auto AddClient(IOThread *io_thread, int fd) -> TcpConnection::ptr;

  void FreshTcpConnection(TcpTimeWheel::TcpConnectionSlot::ptr slot);

 public:
  auto GetDispatcher() -> AbstractDispatcher::ptr;

  auto GetCodec() -> AbstractCodeC::ptr;

  auto GetPeerAddr() -> Address::ptr;

  auto GetLocalAddr() -> Address::ptr;

  auto GetIoThreadPool() -> IOThreadPool::ptr;

  auto GetTimeWheel() -> TcpTimeWheel::ptr;

 private:
  void MainAcceptCorFunc();

  void ClearClientTimerFunc();

 protected:
  AbstractDispatcher::ptr dispatcher_;

  AbstractCodeC::ptr codec_;

  std::string start_info_{};

  AbstractServiceRegister::ptr register_;

  Address::ptr addr_;

 private:
  TcpAcceptor::ptr acceptor_;

  int tcp_counts_{0};

  Reactor *main_reactor_{nullptr};

  bool is_stop_accept_{false};

  Coroutine::ptr accept_cor_;

  IOThreadPool::ptr io_pool_;

  /**
   * @brief TCP 时间轮，用于管理 TcpConnection 的生存时间
   *
   */
  TcpTimeWheel::ptr time_wheel_;

  std::unordered_map<int, std::shared_ptr<TcpConnection>> clients_;

  TimerEvent::ptr clear_clent_timer_event_{nullptr};
};

}  // namespace tirpc
