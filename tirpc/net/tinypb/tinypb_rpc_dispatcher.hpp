#pragma once

#include <google/protobuf/descriptor.h>
#include <google/protobuf/message.h>
#include <google/protobuf/service.h>
#include <map>
#include <memory>

#include "tirpc/net/abstract_dispatcher.hpp"
#include "tirpc/net/tinypb/tinypb_data.hpp"

namespace tirpc {

class TinyPbRpcDispacther : public AbstractDispatcher {
 public:
  // typedef std::shared_ptr<TinyPbRpcDispacther> ptr;
  using service_ptr = std::shared_ptr<google::protobuf::Service>;

  TinyPbRpcDispacther() = default;
  ~TinyPbRpcDispacther() override = default;

  void Dispatch(AbstractData *data, TcpConnection *conn) override;

  auto ParseServiceFullName(const std::string &full_name, std::string &service_name, std::string &method_name) -> bool;

  void RegisterService(const service_ptr &service);

 public:
  // all services should be registerd on there before progress start
  // key: service_name
  std::map<std::string, service_ptr> service_map_;
};

}  // namespace tirpc
