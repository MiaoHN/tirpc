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
    ErrorLog << "dynamic_cast error";
    return;
  }
  Coroutine::GetCurrentCoroutine()->GetRuntime()->msg_no_ = tmp->msg_req_;
  SetCurrentRuntime(Coroutine::GetCurrentCoroutine()->GetRuntime());

  DebugLog << "begin to dispatch client tinypb request, msgno=" << tmp->msg_req_;

  std::string service_name;
  std::string method_name;

  TinyPbStruct reply_pk;
  reply_pk.service_full_name_ = tmp->service_full_name_;
  reply_pk.msg_req_ = tmp->msg_req_;
  if (reply_pk.msg_req_.empty()) {
    reply_pk.msg_req_ = MsgReqUtil::GenMsgNumber();
  }

  if (!ParseServiceFullName(tmp->service_full_name_, service_name, method_name)) {
    ErrorLog << reply_pk.msg_req_ << "|parse service name " << tmp->service_full_name_ << "error";

    reply_pk.err_code_ = ERROR_PARSE_SERVICE_NAME;
    std::stringstream ss;
    ss << "cannot parse service_name:[" << tmp->service_full_name_ << "]";
    reply_pk.err_info_ = ss.str();
    conn->GetCodec()->Encode(conn->GetOutBuffer(), dynamic_cast<AbstractData *>(&reply_pk));
    return;
  }

  Coroutine::GetCurrentCoroutine()->GetRuntime()->interface_name_ = tmp->service_full_name_;
  auto it = service_map_.find(service_name);
  if (it == service_map_.end() || !((*it).second)) {
    reply_pk.err_code_ = ERROR_SERVICE_NOT_FOUND;
    std::stringstream ss;
    ss << "not found service_name:[" << service_name << "]";
    ErrorLog << reply_pk.msg_req_ << "|" << ss.str();
    reply_pk.err_info_ = ss.str();

    conn->GetCodec()->Encode(conn->GetOutBuffer(), dynamic_cast<AbstractData *>(&reply_pk));

    DebugLog << "end dispatch client tinypb request, msgno=" << tmp->msg_req_;
    return;
  }

  service_ptr service = (*it).second;

  const google::protobuf::MethodDescriptor *method = service->GetDescriptor()->FindMethodByName(method_name);
  if (method == nullptr) {
    reply_pk.err_code_ = ERROR_METHOD_NOT_FOUND;
    std::stringstream ss;
    ss << "not found method_name:[" << method_name << "]";
    ErrorLog << reply_pk.msg_req_ << "|" << ss.str();
    reply_pk.err_info_ = ss.str();
    conn->GetCodec()->Encode(conn->GetOutBuffer(), dynamic_cast<AbstractData *>(&reply_pk));
    return;
  }

  google::protobuf::Message *request = service->GetRequestPrototype(method).New();
  DebugLog << reply_pk.msg_req_ << "|request.name = " << request->GetDescriptor()->full_name();

  if (!request->ParseFromString(tmp->pb_data_)) {
    reply_pk.err_code_ = ERROR_FAILED_SERIALIZE;
    std::stringstream ss;
    ss << "faild to parse request data, request.name:[" << request->GetDescriptor()->full_name() << "]";
    reply_pk.err_info_ = ss.str();
    ErrorLog << reply_pk.msg_req_ << "|" << ss.str();
    delete request;
    conn->GetCodec()->Encode(conn->GetOutBuffer(), dynamic_cast<AbstractData *>(&reply_pk));
    return;
  }

  google::protobuf::Message *response = service->GetResponsePrototype(method).New();

  DebugLog << reply_pk.msg_req_ << "|response.name = " << response->GetDescriptor()->full_name();

  RpcController rpc_controller;
  rpc_controller.SetMsgReq(reply_pk.msg_req_);
  rpc_controller.SetMethodName(method_name);
  rpc_controller.SetMethodFullName(tmp->service_full_name_);

  std::function<void()> reply_package_func = []() {};

  RpcClosure closure(reply_package_func);
  service->CallMethod(method, &rpc_controller, request, response, &closure);

  InfoLog << "Called successfully, now send reply package";

  if (!(response->SerializeToString(&(reply_pk.pb_data_)))) {
    reply_pk.pb_data_ = "";
    ErrorLog << reply_pk.msg_req_ << "|reply error! encode reply package error";
    reply_pk.err_code_ = ERROR_FAILED_SERIALIZE;
    reply_pk.err_info_ = "failed to serilize relpy data";
  }

  delete request;
  delete response;

  conn->GetCodec()->Encode(conn->GetOutBuffer(), dynamic_cast<AbstractData *>(&reply_pk));
}

auto RpcDispatcher::ParseServiceFullName(const std::string &full_name, std::string &service_name,
                                         std::string &method_name) -> bool {
  if (full_name.empty()) {
    ErrorLog << "service_full_name empty";
    return false;
  }
  std::size_t i = full_name.find('.');
  if (i == std::string::npos) {
    ErrorLog << "not found [.]";
    return false;
  }

  service_name = full_name.substr(0, i);
  DebugLog << "service_name = " << service_name;
  method_name = full_name.substr(i + 1, full_name.length() - i - 1);
  DebugLog << "method_name = " << method_name;

  return true;
}

void RpcDispatcher::RegisterService(service_ptr service) {
  std::string service_name = service->GetDescriptor()->full_name();
  service_map_[service_name] = service;
  InfoLog << "Successfully register service [" << service_name << "]!";
}

}  // namespace tirpc