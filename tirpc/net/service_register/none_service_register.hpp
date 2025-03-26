#pragma once

#include <unordered_map>

#include "tirpc/net/tcp/abstract_service_register.hpp"

namespace tirpc {

class NoneServiceRegister : public AbstractServiceRegister {
 public:
  NoneServiceRegister() {}
  ~NoneServiceRegister() {}

  void Register(std::shared_ptr<google::protobuf::Service> service, Address::ptr addr) override;

  void Register(std::shared_ptr<CustomService> service, Address::ptr addr) override;

  std::vector<Address::ptr> Discover(const std::string &serviceName) override;

  void Clear() override;

 private:
  std::unordered_map<std::string, std::vector<Address::ptr>> services_;
};

}  // namespace tirpc