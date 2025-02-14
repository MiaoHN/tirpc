#pragma once

#include <memory>
#include <queue>
#include <vector>

#include "tirpc/common/log.hpp"
#include "tirpc/common/mutex.hpp"
#include "tirpc/coroutine/coroutine.hpp"
#include "tirpc/net/base/address.hpp"
#include "tirpc/net/base/fd_event.hpp"
#include "tirpc/net/base/reactor.hpp"
#include "tirpc/net/http/http_request.hpp"
#include "tirpc/net/rpc/rpc_codec.hpp"
#include "tirpc/net/tcp/abstract_codec.hpp"
#include "tirpc/net/tcp/abstract_data.hpp"
#include "tirpc/net/tcp/abstract_slot.hpp"
#include "tirpc/net/tcp/io_thread.hpp"
#include "tirpc/net/tcp/tcp_buffer.hpp"
#include "tirpc/net/tcp/tcp_connection_time_wheel.hpp"

namespace tirpc {

class TcpServer;
class TcpClient;
class IOThread;

enum TcpConnectionState {
  NotConnected = 1,  // can do io
  Connected = 2,     // can do io
  HalfClosing = 3,   // server call shutdown, write half close. can read,but can't write
  Closed = 4,        // can't do io
};

class TcpConnection : public std::enable_shared_from_this<TcpConnection> {
 public:
  using ptr = std::shared_ptr<TcpConnection>;

  TcpConnection(tirpc::TcpServer *tcp_svr, tirpc::IOThread *io_thread, int fd, int buff_size,
                NetAddress::ptr peer_addr);

  TcpConnection(tirpc::TcpClient *tcp_cli, tirpc::Reactor *reactor, int fd, int buff_size, NetAddress::ptr peer_addr);

  void SetUpClient();

  void SetUpServer();

  ~TcpConnection();

  void InitBuffer(int size);

  enum ConnectionType {
    ServerConnection = 1,  // owned by tcp_server
    ClientConnection = 2,  // owned by tcp_client
  };

 public:
  void ShutdownConnection();

  auto GetState() -> TcpConnectionState;

  void SetState(const TcpConnectionState &state);

  auto GetInBuffer() -> TcpBuffer *;

  auto GetOutBuffer() -> TcpBuffer *;

  auto GetCodec() const -> AbstractCodeC::ptr;

  auto GetResPackageData(const std::string &msg_req, TinyPbStruct::pb_ptr &pb_struct) -> bool;

  void RegisterToTimeWheel();

  auto GetCoroutine() -> Coroutine::ptr;

 public:
  void MainServerLoopCorFunc();

  void Input();

  void Execute();

  void Output();

  void SetOverTimeFlag(bool value);

  auto GetOverTimerFlag() -> bool;

  void InitServer();

 private:
  void ClearClient();

 private:
  TcpServer *tcp_svr_{nullptr};
  TcpClient *tcp_cli_{nullptr};
  IOThread *io_thread_{nullptr};
  Reactor *reactor_{nullptr};

  int fd_{-1};
  TcpConnectionState state_{TcpConnectionState::Connected};
  ConnectionType connection_type_{ServerConnection};

  NetAddress::ptr peer_addr_;

  TcpBuffer::ptr read_buffer_;
  TcpBuffer::ptr write_buffer_;

  Coroutine::ptr loop_cor_;

  AbstractCodeC::ptr codec_;

  FdEvent::ptr fd_event_;

  bool stop_{false};

  bool is_over_time_{false};

  std::map<std::string, std::shared_ptr<TinyPbStruct>> reply_datas_;

  std::weak_ptr<AbstractSlot<TcpConnection>> weak_slot_;

  RWMutex mutex_;
};

}  // namespace tirpc