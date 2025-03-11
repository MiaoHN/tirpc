#pragma once

#include <google/protobuf/descriptor.h>
#include <google/protobuf/message.h>
#include <google/protobuf/service.h>
#include <map>
#include <memory>

#include "tirpc/net/tcp/abstract_dispatcher.hpp"

namespace tirpc {

/**
 * @brief 用于处理 RPC 请求的分发
 *
 */
class RpcDispatcher : public AbstractDispatcher {
 public:
  // using ptr = std::shared_ptr<RpcDispatcher>;
  using service_ptr = std::shared_ptr<google::protobuf::Service>;

  RpcDispatcher() = default;
  ~RpcDispatcher() override = default;

  /**
   * @brief 分发 RPC 请求，根据请求数据和连接进行处理
   *
   * @param data
   * @param conn
   */
  void Dispatch(AbstractData *data, TcpConnection *conn) override;

  /**
   * @brief 解析服务全名，将其拆分为服务名和方法名
   *
   * @param full_name "abc.def"
   * @param service_name "abc"
   * @param method_name "def"
   * @return true
   * @return false
   */
  auto ParseServiceFullName(const std::string &full_name, std::string &service_name, std::string &method_name) -> bool;

  /**
   * @brief 注册服务
   *
   * @param service
   */
  void RegisterService(service_ptr service);

 public:
  // all services should be registerd on there before progress start
  // key: service_name
  std::map<std::string, service_ptr> service_map_;
};

}  // namespace tirpc
