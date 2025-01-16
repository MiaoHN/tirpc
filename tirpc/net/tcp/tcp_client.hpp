#pragma once

#include <google/protobuf/service.h>
#include <memory>

#include "tirpc/coroutine/coroutine.hpp"
#include "tirpc/coroutine/coroutine_hook.hpp"
#include "tirpc/net/abstract_codec.hpp"
#include "tirpc/net/address.hpp"
#include "tirpc/net/reactor.hpp"
#include "tirpc/net/tcp/tcp_connection.hpp"

namespace tirpc {

//
// You should use TcpClient in a coroutine(not main coroutine)
//
class TcpClient {
 public:
  using ptr = std::shared_ptr<TcpClient>;

  explicit TcpClient(NetAddress::ptr &addr, ProtocalType type = TinyPb_Protocal);

  ~TcpClient();

  void Init();

  void ResetFd();

  auto SendAndRecvTinyPb(const std::string &msg_no, TinyPbStruct::pb_ptr &res) -> int;

  void Stop();

  auto GetConnection() -> TcpConnection *;

  void SetTimeout(const int v) { max_timeout_ = v; }

  void SetTryCounts(const int v) { try_counts_ = v; }

  auto GetErrInfo() -> const std::string & { return err_info_; }

  auto GetPeerAddr() const -> NetAddress::ptr { return peer_addr_; }

  auto GetLocalAddr() const -> NetAddress::ptr { return local_addr_; }

  auto GetCodeC() -> AbstractCodeC::ptr { return codec_; }

 private:
  int family_{0};
  int fd_{-1};
  int try_counts_{3};       // max try reconnect times
  int max_timeout_{10000};  // max connect timeout, ms
  bool is_stop_{false};
  std::string err_info_;  // error info of client

  NetAddress::ptr local_addr_{nullptr};
  NetAddress::ptr peer_addr_{nullptr};
  Reactor *reactor_{nullptr};
  TcpConnection::ptr connection_{nullptr};

  AbstractCodeC::ptr codec_{nullptr};

  bool connect_succ_{false};
};

}  // namespace tirpc
