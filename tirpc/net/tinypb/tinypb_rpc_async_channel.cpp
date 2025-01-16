#include "tirpc/net/tinypb/tinypb_rpc_async_channel.hpp"

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
#include "tirpc/net/address.hpp"
#include "tirpc/net/tcp/io_thread.hpp"
#include "tirpc/net/tcp/tcp_client.hpp"
#include "tirpc/net/tinypb/tinypb_codec.hpp"
#include "tirpc/net/tinypb/tinypb_data.hpp"
#include "tirpc/net/tinypb/tinypb_rpc_channel.hpp"
#include "tirpc/net/tinypb/tinypb_rpc_controller.hpp"

namespace tirpc {

TinyPbRpcAsyncChannel::TinyPbRpcAsyncChannel(const NetAddress::ptr &addr) {
  rpc_channel_ = std::make_shared<TinyPbRpcChannel>(addr);
  current_iothread_ = IOThread::GetCurrentIOThread();
  current_cor_ = Coroutine::GetCurrentCoroutine();
}

TinyPbRpcAsyncChannel::~TinyPbRpcAsyncChannel() {
  // DebugLog << "~TinyPbRpcAsyncChannel(), return coroutine";
  GetCoroutinePool()->ReturnCoroutine(pending_cor_);
}

auto TinyPbRpcAsyncChannel::GetRpcChannel() -> TinyPbRpcChannel * { return rpc_channel_.get(); }

void TinyPbRpcAsyncChannel::SaveCallee(const con_ptr &controller, const msg_ptr &req, const msg_ptr &res,
                                       const clo_ptr &closure) {
  controller_ = controller;
  req_ = req;
  res_ = res;
  closure_ = closure;
  is_pre_set_ = true;
}

void TinyPbRpcAsyncChannel::CallMethod(const google::protobuf::MethodDescriptor *method,
                                       google::protobuf::RpcController *controller,
                                       const google::protobuf::Message *request, google::protobuf::Message *response,
                                       google::protobuf::Closure *done) {
  auto *rpc_controller = dynamic_cast<TinyPbRpcController *>(controller);
  if (!is_pre_set_) {
    ErrorLog << "Error! must call [saveCallee()] function before [CallMethod()]";
    auto *rpc_controller = dynamic_cast<TinyPbRpcController *>(controller);
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

  std::shared_ptr<TinyPbRpcAsyncChannel> s_ptr = shared_from_this();

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

void TinyPbRpcAsyncChannel::Wait() {
  need_resume_ = true;
  if (is_finished_) {
    return;
  }
  Coroutine::Yield();
}

void TinyPbRpcAsyncChannel::SetFinished(bool value) { is_finished_ = true; }

auto TinyPbRpcAsyncChannel::GetIoThread() -> IOThread * { return current_iothread_; }

auto TinyPbRpcAsyncChannel::GetCurrentCoroutine() -> Coroutine * { return current_cor_; }

auto TinyPbRpcAsyncChannel::GetNeedResume() -> bool { return need_resume_; }

auto TinyPbRpcAsyncChannel::GetControllerPtr() -> google::protobuf::RpcController * { return controller_.get(); }

auto TinyPbRpcAsyncChannel::GetRequestPtr() -> google::protobuf::Message * { return req_.get(); }

auto TinyPbRpcAsyncChannel::GetResponsePtr() -> google::protobuf::Message * { return res_.get(); }

auto TinyPbRpcAsyncChannel::GetClosurePtr() -> google::protobuf::Closure * { return closure_.get(); }

}  // namespace tirpc