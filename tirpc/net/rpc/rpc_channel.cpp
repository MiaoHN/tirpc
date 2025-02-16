#include "tirpc/net/rpc/rpc_channel.hpp"

#include <google/protobuf/descriptor.h>
#include <google/protobuf/message.h>
#include <google/protobuf/service.h>
#include <memory>
#include <utility>

#include "tirpc/common/error_code.hpp"
#include "tirpc/common/log.hpp"
#include "tirpc/common/msg_req.hpp"
#include "tirpc/coroutine/coroutine.hpp"
#include "tirpc/net/base/address.hpp"
#include "tirpc/net/rpc/rpc_codec.hpp"
#include "tirpc/net/rpc/rpc_controller.hpp"
#include "tirpc/net/rpc/rpc_data.hpp"
#include "tirpc/net/tcp/tcp_client.hpp"

namespace tirpc {

RpcChannel::RpcChannel(NetAddress::ptr addr) : addr_(std::move(addr)) {}

void RpcChannel::CallMethod(const google::protobuf::MethodDescriptor *method,
                            google::protobuf::RpcController *controller, const google::protobuf::Message *request,
                            google::protobuf::Message *response, google::protobuf::Closure *done) {
  TinyPbStruct pb_struct;
  auto *rpc_controller = dynamic_cast<RpcController *>(controller);
  if (rpc_controller == nullptr) {
    ErrorLog << "call failed. falid to dynamic cast RpcController";
    return;
  }

  TcpClient::ptr client = std::make_shared<TcpClient>(addr_);
  rpc_controller->SetLocalAddr(client->GetLocalAddr());
  rpc_controller->SetPeerAddr(client->GetPeerAddr());

  pb_struct.service_full_name_ = method->full_name();
  DebugLog << "call service_name = " << pb_struct.service_full_name_;
  if (!request->SerializeToString(&(pb_struct.pb_data_))) {
    ErrorLog << "serialize send package error";
    return;
  }

  if (!rpc_controller->MsgSeq().empty()) {
    pb_struct.msg_req_ = rpc_controller->MsgSeq();
  } else {
    // get current coroutine's msgno to set this request
    Runtime *run_time = GetCurrentRuntime();
    if (run_time != nullptr && !run_time->msg_no_.empty()) {
      pb_struct.msg_req_ = run_time->msg_no_;
      DebugLog << "get from RunTime succ, msgno = " << pb_struct.msg_req_;
    } else {
      pb_struct.msg_req_ = MsgReqUtil::GenMsgNumber();
      DebugLog << "get from RunTime error, generate new msgno = " << pb_struct.msg_req_;
    }
    rpc_controller->SetMsgReq(pb_struct.msg_req_);
  }

  AbstractCodeC::ptr codec = client->GetConnection()->GetCodec();
  codec->Encode(client->GetConnection()->GetOutBuffer(), &pb_struct);
  if (!pb_struct.encode_succ_) {
    rpc_controller->SetError(ERROR_FAILED_ENCODE, "encode tinypb data error");
    return;
  }

  client->SetTimeout(rpc_controller->Timeout());

  TinyPbStruct::pb_ptr res_data;
  int rt = client->SendAndRecvTinyPb(pb_struct.msg_req_, res_data);
  if (rt != 0) {
    rpc_controller->SetError(rt, client->GetErrInfo());
    ErrorLog << pb_struct.msg_req_ << "|call rpc occur client error, service_full_name=" << pb_struct.service_full_name_
             << ", error_code=" << rt << ", error_info = " << client->GetErrInfo();
    return;
  }

  if (!response->ParseFromString(res_data->pb_data_)) {
    rpc_controller->SetError(ERROR_FAILED_DESERIALIZE, "failed to deserialize data from server");
    ErrorLog << pb_struct.msg_req_ << "|failed to deserialize data";
    return;
  }
  if (res_data->err_code_ != 0) {
    ErrorLog << pb_struct.msg_req_ << "|server reply error_code=" << res_data->err_code_
             << ", err_info=" << res_data->err_info_;
    rpc_controller->SetError(res_data->err_code_, res_data->err_info_);
    return;
  }

  // excute callback function
  if (done != nullptr) {
    done->Run();
  }
}

}  // namespace tirpc