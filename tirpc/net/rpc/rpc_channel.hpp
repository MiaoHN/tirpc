#pragma once

#include <google/protobuf/service.h>
#include <memory>

#include "tirpc/net/base/address.hpp"
#include "tirpc/net/tcp/tcp_client.hpp"

namespace tirpc {

/**
 * @brief 用于实现 RPC 通道的功能
 * 该类主要负责处理 RPC 调用的具体逻辑，包括与服务器的通信等
 *
 */
class RpcChannel : public google::protobuf::RpcChannel {
 public:
  using ptr = std::shared_ptr<RpcChannel>;
  explicit RpcChannel(NetAddress::ptr addr);
  ~RpcChannel() override = default;

  /**
   * @brief 重写的 CallMethod 函数，用于发起 RPC 调用。
   * @param method 要调用的方法的描述符。
   * @param controller RPC 控制器，用于控制 RPC 调用的流程和状态。
   * @param request 请求消息，包含调用方法所需的参数。
   * @param response 响应消息，用于存储服务器返回的结果。
   * @param done 回调闭包，在 RPC 调用完成时执行。
   */
  void CallMethod(const google::protobuf::MethodDescriptor *method, google::protobuf::RpcController *controller,
                  const google::protobuf::Message *request, google::protobuf::Message *response,
                  google::protobuf::Closure *done) override;

 private:
  NetAddress::ptr addr_;
  // TcpClient::ptr client_;
};

}  // namespace tirpc
