#include "tirpc/net/rpc/rpc_async_channel.hpp"

#include <google/protobuf/descriptor.h>
#include <google/protobuf/message.h>
#include <google/protobuf/service.h>
#include <future>
#include <memory>

#include "tirpc/common/error_code.hpp"
#include "tirpc/common/log.hpp"
#include "tirpc/common/msg_req.hpp"
#include "tirpc/common/start.hpp"
#include "tirpc/coroutine/coroutine.hpp"
#include "tirpc/coroutine/coroutine_pool.hpp"
#include "tirpc/net/base/address.hpp"
#include "tirpc/net/rpc/rpc_channel.hpp"
#include "tirpc/net/rpc/rpc_controller.hpp"
#include "tirpc/net/tcp/io_thread.hpp"
#include "tirpc/net/tcp/tcp_client.hpp"
#include "tirpc/net/tinypb/tinypb_codec.hpp"
#include "tirpc/net/tinypb/tinypb_data.hpp"

namespace tirpc {

RpcAsyncChannel::RpcAsyncChannel(NetAddress::ptr addr) {
  rpc_channel_ = std::make_shared<tirpc::RpcChannel>(addr);
  current_iothread_ = IOThread::GetCurrentIOThread();
  current_cor_ = Coroutine::GetCurrentCoroutine();
}

RpcAsyncChannel::~RpcAsyncChannel() {
  // DebugLog << "~RpcAsyncChannel(), return coroutine";
  GetCoroutinePool()->ReturnCoroutine(pending_cor_);
}

auto RpcAsyncChannel::GetRpcChannel() -> RpcChannel * { return rpc_channel_.get(); }

void RpcAsyncChannel::SaveCallee(con_ptr controller, msg_ptr req, msg_ptr res, clo_ptr closure) {
  controller_ = controller;
  req_ = req;
  res_ = res;
  closure_ = closure;
  is_pre_set_ = true;
}

void RpcAsyncChannel::CallMethod(const google::protobuf::MethodDescriptor *method,
                                 google::protobuf::RpcController *controller, const google::protobuf::Message *request,
                                 google::protobuf::Message *response, google::protobuf::Closure *done) {
  auto *rpc_controller = dynamic_cast<RpcController *>(controller);
  if (!is_pre_set_) {
    ErrorLog << "Error! must call [saveCallee()] function before [CallMethod()]";
    auto *rpc_controller = dynamic_cast<RpcController *>(controller);
    rpc_controller->SetError(ERROR_NOT_SET_ASYNC_PRE_CALL,
                             "Error! must call [saveCallee()] function before [CallMethod()];");
    is_finished_ = true;
    return;
  }
  Runtime *run_time = GetCurrentRuntime();
  if (run_time != nullptr) {
    rpc_controller->SetMsgReq(run_time->msg_no_);
    DebugLog << "get from RunTime succ, msgno=" << run_time->msg_no_;
  } else {
    rpc_controller->SetMsgReq(MsgReqUtil::GenMsgNumber());
    DebugLog << "get from RunTime error, generate new msgno=" << rpc_controller->MsgSeq();
  }

  std::shared_ptr<RpcAsyncChannel> s_ptr = shared_from_this();

  auto cb = [s_ptr, method]() mutable {
    DebugLog << "now excute rpc call method by this thread";
    s_ptr->GetRpcChannel()->CallMethod(method, s_ptr->GetControllerPtr(), s_ptr->GetRequestPtr(),
                                       s_ptr->GetResponsePtr(), nullptr);

    DebugLog << "excute rpc call method by this thread finish";

    auto call_back = [s_ptr]() mutable {
      DebugLog << "async excute rpc call method back old thread";
      // callback function excute in origin thread
      if (s_ptr->GetClosurePtr() != nullptr) {
        s_ptr->GetClosurePtr()->Run();
      }
      s_ptr->SetFinished(true);

      if (s_ptr->GetNeedResume()) {
        DebugLog << "async excute rpc call method back old thread, need resume";
        Coroutine::Resume(s_ptr->GetCurrentCoroutine());
      }
      s_ptr.reset();
    };

    s_ptr->GetIoThread()->GetReactor()->AddTask(call_back, true);
    s_ptr.reset();
  };
  pending_cor_ = GetServer()->GetIoThreadPool()->AddCoroutineToRandomThread(cb, false);
}

void RpcAsyncChannel::Wait() {
  need_resume_ = true;
  if (is_finished_) {
    return;
  }
  Coroutine::Yield();
}

void RpcAsyncChannel::SetFinished(bool value) { is_finished_ = true; }

auto RpcAsyncChannel::GetIoThread() -> IOThread * { return current_iothread_; }

auto RpcAsyncChannel::GetCurrentCoroutine() -> Coroutine * { return current_cor_; }

auto RpcAsyncChannel::GetNeedResume() -> bool { return need_resume_; }

auto RpcAsyncChannel::GetControllerPtr() -> google::protobuf::RpcController * { return controller_.get(); }

auto RpcAsyncChannel::GetRequestPtr() -> google::protobuf::Message * { return req_.get(); }

auto RpcAsyncChannel::GetResponsePtr() -> google::protobuf::Message * { return res_.get(); }

auto RpcAsyncChannel::GetClosurePtr() -> google::protobuf::Closure * { return closure_.get(); }

}  // namespace tirpc