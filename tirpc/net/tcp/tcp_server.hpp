#pragma once

#include <google/protobuf/service.h>
#include <map>

#include "tirpc/net/abstract_codec.hpp"
#include "tirpc/net/abstract_dispatcher.hpp"
#include "tirpc/net/address.hpp"
#include "tirpc/net/fd_event.hpp"
#include "tirpc/net/http/http_dispatcher.hpp"
#include "tirpc/net/http/http_servlet.hpp"
#include "tirpc/net/reactor.hpp"
#include "tirpc/net/tcp/io_thread.hpp"
#include "tirpc/net/tcp/tcp_connection.hpp"
#include "tirpc/net/tcp/tcp_connection_time_wheel.hpp"
#include "tirpc/net/timer.hpp"

namespace tirpc {

class TcpAcceptor {
 public:
  using ptr = std::shared_ptr<TcpAcceptor>;
  explicit TcpAcceptor(NetAddress::ptr net_addr);

  void Init();

  auto ToAccept() -> int;

  ~TcpAcceptor();

  auto GetPeerAddr() -> NetAddress::ptr { return peer_addr_; }

  auto GeLocalAddr() -> NetAddress::ptr { return local_addr_; }

 private:
  int family_{-1};
  int fd_{-1};

  NetAddress::ptr local_addr_{nullptr};
  NetAddress::ptr peer_addr_{nullptr};
};

class TcpServer {
 public:
  using ptr = std::shared_ptr<TcpServer>;

  explicit TcpServer(NetAddress::ptr addr, ProtocalType type = TinyPb_Protocal);

  ~TcpServer();

  void Start();

  void AddCoroutine(const Coroutine::ptr &cor);

  auto RegisterService(std::shared_ptr<google::protobuf::Service> &service) -> bool;

  auto RegisterHttpServlet(const std::string &url_path, const HttpServlet::ptr &servlet) -> bool;

  auto AddClient(IOThread *io_thread, int fd) -> TcpConnection::ptr;

  void FreshTcpConnection(TcpTimeWheel::TcpConnectionSlot::ptr slot);

 public:
  auto GetDispatcher() -> AbstractDispatcher::ptr;

  auto GetCodec() -> AbstractCodeC::ptr;

  auto GetPeerAddr() -> NetAddress::ptr;

  auto GetLocalAddr() -> NetAddress::ptr;

  auto GetIoThreadPool() -> IOThreadPool::ptr;

  auto GetTimeWheel() -> TcpTimeWheel::ptr;

 private:
  void MainAcceptCorFunc();

  void ClearClientTimerFunc();

 private:
  NetAddress::ptr addr_;

  TcpAcceptor::ptr acceptor_;

  int tcp_counts_{0};

  Reactor *main_reactor_{nullptr};

  bool is_stop_accept_{false};

  Coroutine::ptr accept_cor_;

  AbstractDispatcher::ptr dispatcher_;

  AbstractCodeC::ptr codec_;

  IOThreadPool::ptr io_pool_;

  ProtocalType protocal_type_{TinyPb_Protocal};

  TcpTimeWheel::ptr time_wheel_;

  std::map<int, std::shared_ptr<TcpConnection>> clients_;

  TimerEvent::ptr clear_clent_timer_event_{nullptr};
};

}  // namespace tirpc
