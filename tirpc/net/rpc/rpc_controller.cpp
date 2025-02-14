#include "tirpc/net/rpc/rpc_controller.hpp"

#include <google/protobuf/service.h>
#include <google/protobuf/stubs/callback.h>

#include <utility>

namespace tirpc {

void RpcController::Reset() {}

auto RpcController::Failed() const -> bool { return is_failed_; }

auto RpcController::ErrorText() const -> std::string { return error_info_; }

void RpcController::StartCancel() {}

void RpcController::SetFailed(const std::string &reason) {
  is_failed_ = true;
  error_info_ = reason;
}

auto RpcController::IsCanceled() const -> bool { return false; }

void RpcController::NotifyOnCancel(google::protobuf::Closure *callback) {}

void RpcController::SetErrorCode(const int error_code) { error_code_ = error_code; }

auto RpcController::ErrorCode() const -> int { return error_code_; }

auto RpcController::MsgSeq() const -> const std::string & { return msg_req_; }

void RpcController::SetMsgReq(const std::string &msg_req) { msg_req_ = msg_req; }

void RpcController::SetError(const int err_code, const std::string &err_info) {
  SetFailed(err_info);
  SetErrorCode(err_code);
}

void RpcController::SetPeerAddr(NetAddress::ptr addr) { peer_addr_ = std::move(addr); }

void RpcController::SetLocalAddr(NetAddress::ptr addr) { local_addr_ = std::move(addr); }

auto RpcController::PeerAddr() -> NetAddress::ptr { return peer_addr_; }

auto RpcController::LocalAddr() -> NetAddress::ptr { return local_addr_; }

void RpcController::SetTimeout(const int timeout) { timeout_ = timeout; }

auto RpcController::Timeout() const -> int { return timeout_; }

void RpcController::SetMethodName(const std::string &name) { method_name_ = name; }

auto RpcController::GetMethodName() -> std::string { return method_name_; }

void RpcController::SetMethodFullName(const std::string &name) { full_name_ = name; }

auto RpcController::GetMethodFullName() -> std::string { return full_name_; }

}  // namespace tirpc