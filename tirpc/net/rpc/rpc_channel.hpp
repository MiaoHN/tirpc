#pragma once

#include <google/protobuf/service.h>
#include <memory>

#include "tirpc/net/address.hpp"
#include "tirpc/net/tcp/tcp_client.hpp"

namespace tirpc {

class TinyPbRpcChannel : public google::protobuf::RpcChannel {
 public:
  using ptr = std::shared_ptr<TinyPbRpcChannel>;
  explicit TinyPbRpcChannel(NetAddress::ptr addr);
  ~TinyPbRpcChannel() override = default;

  void CallMethod(const google::protobuf::MethodDescriptor *method, google::protobuf::RpcController *controller,
                  const google::protobuf::Message *request, google::protobuf::Message *response,
                  google::protobuf::Closure *done) override;

 private:
  NetAddress::ptr addr_;
  // TcpClient::ptr client_;
};

}  // namespace tirpc
