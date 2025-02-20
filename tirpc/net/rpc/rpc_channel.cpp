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

RpcChannel::RpcChannel(Address::ptr addr) {
  addrs_.clear();
  addrs_.push_back(addr);
  origin_addrs_ = addrs_;
  load_balancer_ = LoadBalance::queryStrategy(LoadBalanceCategory::Random);
}

RpcChannel::RpcChannel(std::vector<Address::ptr> addrs, LoadBalanceCategory load_balance)
    : addrs_(addrs), origin_addrs_(addrs) {
  load_balancer_ = LoadBalance::queryStrategy(load_balance);
}

void RpcChannel::CallMethod(const google::protobuf::MethodDescriptor *method,
                            google::protobuf::RpcController *controller, const google::protobuf::Message *request,
                            google::protobuf::Message *response, google::protobuf::Closure *done) {
  TinyPbStruct pb_struct;
  auto *rpc_controller = dynamic_cast<RpcController *>(controller);
  if (!rpc_controller) {
    ErrorLog << "call failed. falid to dynamic cast RpcController";
    if (done) {
      done->Run();
    }
    return;
  }

  pb_struct.service_full_name_ = method->full_name();
  DebugLog << "call service_name = " << pb_struct.service_full_name_;
  if (!request->SerializeToString(&(pb_struct.pb_data_))) {
    ErrorLog << "serialize send package error";
    if (done) {
      done->Run();
    }
    return;
  }

  if (!rpc_controller->MsgSeq().empty()) {
    pb_struct.msg_seq_ = rpc_controller->MsgSeq();
  } else {
    // get current coroutine's msgno to set this request
    Runtime *run_time = GetCurrentRuntime();
    if (run_time != nullptr && !run_time->msg_no_.empty()) {
      pb_struct.msg_seq_ = run_time->msg_no_;
      DebugLog << "get from RunTime succ, msgno = " << pb_struct.msg_seq_;
    } else {
      pb_struct.msg_seq_ = MsgReqUtil::GenMsgNumber();
      DebugLog << "get from RunTime error, generate new msgno = " << pb_struct.msg_seq_;
    }
    rpc_controller->SetMsgSeq(pb_struct.msg_seq_);
  }

  int max_retry = rpc_controller->GetMaxRetry();

  Address::ptr addr;
  TinyPbStruct::pb_ptr res_data;
  int64_t end_call = GetNowMs() + rpc_controller->Timeout();

  for (int retry_times = 0; retry_times <= max_retry; retry_times++) {
    if (addrs_.empty()) {
      addrs_ = origin_addrs_;
      if (addrs_.empty()) {
        rpc_controller->SetError(ERROR_SERVICE_NOT_FOUND, "not found address of service");
        ErrorLog << pb_struct.msg_seq_
                 << "|call rpc occur client error, serviceFullName=" << pb_struct.service_full_name_
                 << ", error_code=" << ERROR_SERVICE_NOT_FOUND << ", errorInfo = not found address of service";
        if (done) {
          done->Run();
        }
        return;
      }
    }

    addr = load_balancer_->select(addrs_, pb_struct);
    TcpClient::ptr client = std::make_shared<TcpClient>(addr);
    rpc_controller->SetLocalAddr(client->GetLocalAddr());
    rpc_controller->SetPeerAddr(client->GetPeerAddr());

    AbstractCodeC::ptr codec = client->GetConnection()->GetCodec();
    codec->Encode(client->GetConnection()->GetOutBuffer(), &pb_struct);
    if (!pb_struct.encode_succ_) {
      rpc_controller->SetError(ERROR_FAILED_ENCODE, "encode tinypb data error");
      if (done) {
        done->Run();
      }
      return;
    }

    int64_t res_time = end_call - GetNowMs();
    client->SetTimeout(res_time);

    int ret = client->SendAndRecvTinyPb(pb_struct.msg_seq_, res_data);
    if (ret == 0) {
      break;
    } else if (ret != ERROR_RPC_CALL_TIMEOUT) {
      auto it = addrs_.begin();
      for (; it != addrs_.end(); it++) {
        if ((*it)->ToString() == addr->ToString()) {
          break;
        }
      }
      if (it != addrs_.end()) {
        addrs_.erase(it);
      }
      ErrorLog << pb_struct.msg_seq_ << "|call rpc occur client error, serviceFullName=" << pb_struct.service_full_name_
               << ", error_code=" << ret << ", errorInfo = " << client->GetErrInfo() << ", to retry......";
      continue;
    } else {
      rpc_controller->SetError(ret, client->GetErrInfo());
      ErrorLog << pb_struct.msg_seq_ << "|call rpc occur client error, serviceFullName=" << pb_struct.service_full_name_
               << ", error_code=" << ret << ", errorInfo = " << client->GetErrInfo();
      if (done) {
        done->Run();
      }
      return;
    }
  }

  if (!response->ParseFromString(res_data->pb_data_)) {
    rpc_controller->SetError(ERROR_FAILED_DESERIALIZE, "failed to deserialize data from server");
    ErrorLog << pb_struct.msg_seq_ << "|failed to deserialize data";
    return;
  }
  if (res_data->err_code_ != 0) {
    ErrorLog << pb_struct.msg_seq_ << "|server reply error_code=" << res_data->err_code_
             << ", err_info=" << res_data->err_info_;
    // rpc_controller->SetError(res_data->err_code_, res_data->err_info_);
    if (done) {
      done->Run();
    }
    return;
  }

  InfoLog << "Method called successfully, addr = [" << addr->ToString() << "]";

  // excute callback function
  if (done) {
    done->Run();
  }
}

}  // namespace tirpc