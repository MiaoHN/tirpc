#include "tirpc/net/service_register/none_service_register.hpp"

#include <google/protobuf/descriptor.h>
#include <google/protobuf/service.h>

namespace tirpc {

void NoneServiceRegister::Register(std::shared_ptr<google::protobuf::Service> service, Address::ptr addr) {
  std::string service_name = service->GetDescriptor()->full_name();
  if (services_.find(service_name) == services_.end()) {
    services_[service_name] = {};
  }
  services_[service_name].push_back(addr);
}

void NoneServiceRegister::Register(std::shared_ptr<CustomService> service, Address::ptr addr) {
  std::string service_name = service->getServiceName();
  if (services_.find(service_name) == services_.end()) {
    services_[service_name] = {};
  }
  services_[service_name].push_back(addr);
}

std::vector<Address::ptr> NoneServiceRegister::Discover(const std::string &serviceName) {
  if (services_.find(serviceName) == services_.end()) {
    return {};
  }
  return services_[serviceName];
}

void NoneServiceRegister::Clear() { services_.clear(); }

}  // namespace tirpc