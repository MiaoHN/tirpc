#pragma once

#include <google/protobuf/stubs/callback.h>
#include <functional>
#include <memory>
#include <utility>

namespace tirpc {

/**
 * @brief 用于封装一个回调函数，在 RPC 调用完成时执行
 *
 */
class RpcClosure : public google::protobuf::Closure {
 public:
  using ptr = std::shared_ptr<RpcClosure>;
  explicit RpcClosure(std::function<void()> cb) : cb_(std::move(cb)) {}

  ~RpcClosure() override = default;

  void Run() override {
    if (cb_) {
      cb_();
    }
  }

 private:
  std::function<void()> cb_{nullptr};
};

}  // namespace tirpc
