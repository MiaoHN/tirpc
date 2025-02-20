#pragma once

#include <google/protobuf/service.h>
#include <string>
#include <unordered_set>

#include "tirpc/common/zk_util.hpp"
#include "tirpc/net/tcp/abstract_service_register.hpp"

namespace tirpc {

class ZkServiceRegister : public AbstractServiceRegister {
 public:
  ZkServiceRegister();
  ZkServiceRegister(const std::string &ip, int port, int timeout);
  ~ZkServiceRegister();

  void Register(std::shared_ptr<google::protobuf::Service> service, Address::ptr addr) override;
  void Register(std::shared_ptr<CustomService> service, Address::ptr addr) override;
  auto Discover(const std::string &serviceName) -> std::vector<Address::ptr> override;
  void Clear() override;

 private:
  void Init();

  ZkClient zkCli_;

  std::unordered_set<std::string> pathSet_;
  std::unordered_set<std::string> serviceSet_;
};

}  // namespace tirpc