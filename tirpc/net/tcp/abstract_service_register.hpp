#pragma once

#include <google/protobuf/service.h>
#include <vector>

#include "tirpc/net/base/address.hpp"

namespace tirpc {

// Override this class to use your custom protocol
class CustomService : public std::enable_shared_from_this<CustomService> {
 public:
  typedef std::shared_ptr<CustomService> ptr;
  CustomService() {}
  virtual ~CustomService() {}

  virtual std::string getServiceName() { return "CustomService"; }
};

class AbstractServiceRegister {
 public:
  using ptr = std::shared_ptr<AbstractServiceRegister>;

  AbstractServiceRegister() {}
  virtual ~AbstractServiceRegister() {}

  virtual void Register(std::shared_ptr<google::protobuf::Service> service, Address::ptr addr) = 0;
  virtual void Register(std::shared_ptr<CustomService> service, Address::ptr addr) = 0;

  virtual auto Discover(const std::string &service_name) -> std::vector<Address::ptr> = 0;

  virtual void Clear() = 0;
};

}  // namespace tirpc