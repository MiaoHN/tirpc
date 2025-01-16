#pragma once

#include <google/protobuf/stubs/callback.h>
#include <functional>
#include <memory>
#include <utility>

namespace tirpc {

class TinyPbRpcClosure : public google::protobuf::Closure {
 public:
  using ptr = std::shared_ptr<TinyPbRpcClosure>;
  explicit TinyPbRpcClosure(std::function<void()> cb) : cb_(std::move(cb)) {}

  ~TinyPbRpcClosure() override = default;

  void Run() override {
    if (cb_) {
      cb_();
    }
  }

 private:
  std::function<void()> cb_{nullptr};
};

}  // namespace tirpc
