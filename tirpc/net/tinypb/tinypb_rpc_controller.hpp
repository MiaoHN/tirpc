#pragma once

#include <google/protobuf/service.h>
#include <google/protobuf/stubs/callback.h>
#include <cstdio>
#include <memory>

#include "tirpc/net/address.hpp"

namespace tirpc {

class TinyPbRpcController : public google::protobuf::RpcController {
 public:
  using ptr = std::shared_ptr<TinyPbRpcController>;
  // Client-side methods ---------------------------------------------

  TinyPbRpcController() = default;

  ~TinyPbRpcController() override = default;

  void Reset() override;

  auto Failed() const -> bool override;

  // Server-side methods ---------------------------------------------

  auto ErrorText() const -> std::string override;

  void StartCancel() override;

  void SetFailed(const std::string &reason) override;

  auto IsCanceled() const -> bool override;

  void NotifyOnCancel(google::protobuf::Closure *callback) override;

  // common methods

  auto ErrorCode() const -> int;

  void SetErrorCode(int error_code);

  auto MsgSeq() const -> const std::string &;

  void SetMsgReq(const std::string &msg_req);

  void SetError(int err_code, const std::string &err_info);

  void SetPeerAddr(NetAddress::ptr addr);

  void SetLocalAddr(NetAddress::ptr addr);

  auto PeerAddr() -> NetAddress::ptr;

  auto LocalAddr() -> NetAddress::ptr;

  void SetTimeout(int timeout);

  auto Timeout() const -> int;

  void SetMethodName(const std::string &name);

  auto GetMethodName() -> std::string;

  void SetMethodFullName(const std::string &name);

  auto GetMethodFullName() -> std::string;

 private:
  int error_code_{0};       // error_code, identify one specific error
  std::string error_info_;  // error_info, details description of error
  std::string msg_req_;     // msg_req, identify once rpc request and response
  bool is_failed_{false};
  bool is_cancled_{false};
  NetAddress::ptr peer_addr_;
  NetAddress::ptr local_addr_;

  int timeout_{5000};        // max call rpc timeout
  std::string method_name_;  // method name
  std::string full_name_;    // full name, like server.method_name
};

}  // namespace tirpc
