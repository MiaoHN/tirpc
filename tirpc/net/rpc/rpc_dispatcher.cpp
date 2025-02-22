#include "tirpc/net/rpc/rpc_dispatcher.hpp"

#include <google/protobuf/descriptor.h>
#include <google/protobuf/message.h>
#include <google/protobuf/service.h>

#include "tirpc/common/error_code.hpp"
#include "tirpc/common/msg_req.hpp"
#include "tirpc/net/rpc/rpc_closure.hpp"
#include "tirpc/net/rpc/rpc_codec.hpp"
#include "tirpc/net/rpc/rpc_controller.hpp"
#include "tirpc/net/rpc/rpc_data.hpp"
#include "tirpc/net/tcp/abstract_dispatcher.hpp"
#include "tirpc/net/tcp/tcp_connection.hpp"

namespace tirpc {

class TcpBuffer;

void RpcDispatcher::Dispatch(AbstractData *data, TcpConnection *conn) {
  auto *tmp = dynamic_cast<TinyPbStruct *>(data);
  if (tmp == nullptr) {
    LOG_ERROR << "dynamic_cast error";
    return;
  }

  auto runtime = Coroutine::GetCurrentCoroutine()->GetRuntime();
  runtime->msg_no_ = tmp->msg_seq_;
  SetCurrentRuntime(runtime);

  LOG_DEBUG << "begin to dispatch client tinypb request, msgno=" << tmp->msg_seq_;

  std::string service_name;
  std::string method_name;

  TinyPbStruct reply_pk;
  reply_pk.service_full_name_ = tmp->service_full_name_;
  reply_pk.msg_seq_ = tmp->msg_seq_;
  if (reply_pk.msg_seq_.empty()) {
    reply_pk.msg_seq_ = MsgReqUtil::GenMsgNumber();
  }

  if (!ParseServiceFullName(tmp->service_full_name_, service_name, method_name)) {
    std::stringstream ss;
    ss << reply_pk.msg_seq_ << "|parse service name " << tmp->service_full_name_ << "error";
    LOG_ERROR << ss.str();
    reply_pk.err_code_ = ERROR_PARSE_SERVICE_NAME;
    ss.str("");
    ss << "cannot parse service_name:[" << tmp->service_full_name_ << "]";
    reply_pk.err_info_ = ss.str();
    conn->GetCodec()->Encode(conn->GetOutBuffer(), dynamic_cast<AbstractData *>(&reply_pk));
    return;
  }

  runtime->interface_name_ = tmp->service_full_name_;
  auto it = service_map_.find(service_name);
  if (it == service_map_.end() || !((*it).second)) {
    std::stringstream ss;
    ss << reply_pk.msg_seq_ << "|not found service_name:[" << service_name << "]";
    LOG_ERROR << ss.str();
    reply_pk.err_code_ = ERROR_SERVICE_NOT_FOUND;
    ss.str("");
    ss << "not found service_name:[" << service_name << "]";
    reply_pk.err_info_ = ss.str();
    conn->GetCodec()->Encode(conn->GetOutBuffer(), dynamic_cast<AbstractData *>(&reply_pk));
    return;
  }

  service_ptr service = (*it).second;
  const google::protobuf::MethodDescriptor *method = service->GetDescriptor()->FindMethodByName(method_name);
  if (method == nullptr) {
    std::stringstream ss;
    ss << reply_pk.msg_seq_ << "|not found method_name:[" << method_name << "]";
    LOG_ERROR << ss.str();
    reply_pk.err_code_ = ERROR_METHOD_NOT_FOUND;
    ss.str("");
    ss << "not found method_name:[" << method_name << "]";
    reply_pk.err_info_ = ss.str();
    conn->GetCodec()->Encode(conn->GetOutBuffer(), dynamic_cast<AbstractData *>(&reply_pk));
    return;
  }

  std::unique_ptr<google::protobuf::Message> request(service->GetRequestPrototype(method).New());
  LOG_DEBUG << reply_pk.msg_seq_ << "|request.name = " << request->GetDescriptor()->full_name();

  if (!request->ParseFromString(tmp->pb_data_)) {
    std::stringstream ss;
    ss << reply_pk.msg_seq_ << "|faild to parse request data, request.name:[" << request->GetDescriptor()->full_name()
       << "]";
    LOG_ERROR << ss.str();
    reply_pk.err_code_ = ERROR_FAILED_SERIALIZE;
    ss.str("");
    ss << "faild to parse request data, request.name:[" << request->GetDescriptor()->full_name() << "]";
    reply_pk.err_info_ = ss.str();
    conn->GetCodec()->Encode(conn->GetOutBuffer(), dynamic_cast<AbstractData *>(&reply_pk));
    return;
  }

  std::unique_ptr<google::protobuf::Message> response(service->GetResponsePrototype(method).New());
  LOG_DEBUG << reply_pk.msg_seq_ << "|response.name = " << response->GetDescriptor()->full_name();

  RpcController rpc_controller;
  rpc_controller.SetMsgSeq(reply_pk.msg_seq_);
  rpc_controller.SetMethodName(method_name);
  rpc_controller.SetMethodFullName(tmp->service_full_name_);

  std::function<void()> reply_package_func = []() {};
  RpcClosure closure(reply_package_func);
  service->CallMethod(method, &rpc_controller, request.get(), response.get(), &closure);

  LOG_INFO << "Called successfully, now send reply package";

  if (!(response->SerializeToString(&(reply_pk.pb_data_)))) {
    reply_pk.pb_data_ = "";
    std::stringstream ss;
    ss << reply_pk.msg_seq_ << "|reply error! encode reply package error";
    LOG_ERROR << ss.str();
    reply_pk.err_code_ = ERROR_FAILED_SERIALIZE;
    ss.str("");
    ss << "failed to serilize relpy data";
    reply_pk.err_info_ = ss.str();
  }

  conn->GetCodec()->Encode(conn->GetOutBuffer(), dynamic_cast<AbstractData *>(&reply_pk));
  LOG_DEBUG << "end dispatch client tinypb request, msgno=" << tmp->msg_seq_;
}

auto RpcDispatcher::ParseServiceFullName(const std::string &full_name, std::string &service_name,
                                         std::string &method_name) -> bool {
  if (full_name.empty()) {
    LOG_ERROR << "service_full_name empty";
    return false;
  }
  std::size_t i = full_name.find('.');
  if (i == std::string::npos) {
    LOG_ERROR << "not found [.]";
    return false;
  }

  service_name = full_name.substr(0, i);
  LOG_DEBUG << "service_name = " << service_name;
  method_name = full_name.substr(i + 1, full_name.length() - i - 1);
  LOG_DEBUG << "method_name = " << method_name;

  return true;
}

void RpcDispatcher::RegisterService(service_ptr service) {
  std::string service_name = service->GetDescriptor()->full_name();
  service_map_[service_name] = service;
  LOG_INFO << "Successfully register service [" << service_name << "]!";
}

}  // namespace tirpc