#pragma once

#include "tirpc/net/tcp/tcp_server.hpp"

namespace tirpc {

/**
 * @brief Rpc Server，当前支持 protobuf 协议
 *
 */
class RpcServer : public TcpServer {
 public:
  explicit RpcServer(NetAddress::ptr addr);

  auto RegisterService(std::shared_ptr<google::protobuf::Service> service) -> bool;
};

}  // namespace tirpc