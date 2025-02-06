#include "tirpc/net/rpc/rpc_controller.hpp"

#include <google/protobuf/service.h>
#include <google/protobuf/stubs/callback.h>

#include <utility>

namespace tirpc {

void TinyPbRpcController::Reset() {}

auto TinyPbRpcController::Failed() const -> bool { return is_failed_; }

auto TinyPbRpcController::ErrorText() const -> std::string { return error_info_; }

void TinyPbRpcController::StartCancel() {}

void TinyPbRpcController::SetFailed(const std::string &reason) {
  is_failed_ = true;
  error_info_ = reason;
}

auto TinyPbRpcController::IsCanceled() const -> bool { return false; }

void TinyPbRpcController::NotifyOnCancel(google::protobuf::Closure *callback) {}

void TinyPbRpcController::SetErrorCode(const int error_code) { error_code_ = error_code; }

auto TinyPbRpcController::ErrorCode() const -> int { return error_code_; }

auto TinyPbRpcController::MsgSeq() const -> const std::string & { return msg_req_; }

void TinyPbRpcController::SetMsgReq(const std::string &msg_req) { msg_req_ = msg_req; }

void TinyPbRpcController::SetError(const int err_code, const std::string &err_info) {
  SetFailed(err_info);
  SetErrorCode(err_code);
}

void TinyPbRpcController::SetPeerAddr(NetAddress::ptr addr) { peer_addr_ = std::move(addr); }

void TinyPbRpcController::SetLocalAddr(NetAddress::ptr addr) { local_addr_ = std::move(addr); }
auto TinyPbRpcController::PeerAddr() -> NetAddress::ptr { return peer_addr_; }

auto TinyPbRpcController::LocalAddr() -> NetAddress::ptr { return local_addr_; }

void TinyPbRpcController::SetTimeout(const int timeout) { timeout_ = timeout; }
auto TinyPbRpcController::Timeout() const -> int { return timeout_; }

void TinyPbRpcController::SetMethodName(const std::string &name) { method_name_ = name; }

auto TinyPbRpcController::GetMethodName() -> std::string { return method_name_; }

void TinyPbRpcController::SetMethodFullName(const std::string &name) { full_name_ = name; }

auto TinyPbRpcController::GetMethodFullName() -> std::string { return full_name_; }

}  // namespace tirpc