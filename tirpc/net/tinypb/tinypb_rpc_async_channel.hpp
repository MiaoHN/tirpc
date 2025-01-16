#pragma once

#include <google/protobuf/service.h>
#include <future>
#include <memory>

#include "tirpc/coroutine/coroutine.hpp"
#include "tirpc/net/address.hpp"
#include "tirpc/net/tcp/tcp_client.hpp"
#include "tirpc/net/tinypb/tinypb_data.hpp"
#include "tirpc/net/tinypb/tinypb_rpc_channel.hpp"
#include "tirpc/net/tinypb/tinypb_rpc_controller.hpp"

namespace tirpc {

class IOThread;

class TinyPbRpcAsyncChannel : public google::protobuf::RpcChannel,
                              public std::enable_shared_from_this<TinyPbRpcAsyncChannel> {
 public:
  using ptr = std::shared_ptr<TinyPbRpcAsyncChannel>;
  using con_ptr = std::shared_ptr<google::protobuf::RpcController>;
  using msg_ptr = std::shared_ptr<google::protobuf::Message>;
  using clo_ptr = std::shared_ptr<google::protobuf::Closure>;

  explicit TinyPbRpcAsyncChannel(const NetAddress::ptr &addr);

  ~TinyPbRpcAsyncChannel() override;

  void CallMethod(const google::protobuf::MethodDescriptor *method, google::protobuf::RpcController *controller,
                  const google::protobuf::Message *request, google::protobuf::Message *response,
                  google::protobuf::Closure *done) override;

  auto GetRpcChannel() -> TinyPbRpcChannel *;

  // must call saveCallee before CallMethod
  // in order to save shared_ptr count of req res controller
  void SaveCallee(const con_ptr &controller, const msg_ptr &req, const msg_ptr &res, const clo_ptr &closure);

  void Wait();

  void SetFinished(bool value);

  auto GetNeedResume() -> bool;

  auto GetIoThread() -> IOThread *;

  auto GetCurrentCoroutine() -> Coroutine *;

  auto GetControllerPtr() -> google::protobuf::RpcController *;

  auto GetRequestPtr() -> google::protobuf::Message *;

  auto GetResponsePtr() -> google::protobuf::Message *;

  auto GetClosurePtr() -> google::protobuf::Closure *;

 private:
  TinyPbRpcChannel::ptr rpc_channel_;
  Coroutine::ptr pending_cor_;
  Coroutine *current_cor_{nullptr};
  IOThread *current_iothread_{nullptr};
  bool is_finished_{false};
  bool need_resume_{false};
  bool is_pre_set_{false};

 private:
  con_ptr controller_;
  msg_ptr req_;
  msg_ptr res_;
  clo_ptr closure_;
};

}  // namespace tirpc
