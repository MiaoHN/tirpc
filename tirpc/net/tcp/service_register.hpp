#pragma once

#include <google/protobuf/service.h>
#include <memory>
#include <string>
#include <vector>
#include "tirpc/common/const.hpp"
#include "tirpc/net/tcp/abstract_service_register.hpp"

namespace tirpc {

class ServiceRegister {
 public:
  static auto Query(ServiceRegisterCategory category) -> AbstractServiceRegister::ptr;

  static auto Category2Str(ServiceRegisterCategory category) -> std::string;

  static auto Str2Category(const std::string &str) -> ServiceRegisterCategory;

 private:
  static AbstractServiceRegister::ptr s_noneServiceRegister;
  static AbstractServiceRegister::ptr s_zkServiceRegister;
};

}  // namespace tirpc