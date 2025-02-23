#include "tirpc/net/tcp/service_register.hpp"

#include <algorithm>
#include <cctype>

#include "tirpc/net/service_register/zk_service_register.hpp"
#include "tirpc/net/service_register/none_service_register.hpp"

namespace tirpc {

AbstractServiceRegister::ptr ServiceRegister::s_noneServiceRegister;
AbstractServiceRegister::ptr ServiceRegister::s_zkServiceRegister;

auto ServiceRegister::Query(ServiceRegisterCategory category) -> AbstractServiceRegister::ptr {
  switch (category) {
    case ServiceRegisterCategory::None:
      if (!s_noneServiceRegister) {
        s_noneServiceRegister = std::make_shared<NoneServiceRegister>();
      }
      return s_noneServiceRegister;
    case ServiceRegisterCategory::Zk:
      if (!s_zkServiceRegister) {
        // TODO: 更灵活
        s_zkServiceRegister = std::make_shared<ZkServiceRegister>("127.0.0.1", 2181, 30000);
      }
      return s_zkServiceRegister;
    default:
      if (!s_noneServiceRegister) {
        s_noneServiceRegister = std::make_shared<NoneServiceRegister>();
      }
      return s_noneServiceRegister;
  }
}

auto ServiceRegister::Category2Str(ServiceRegisterCategory category) -> std::string {
  switch (category) {
    case ServiceRegisterCategory::None:
      return "None";
    case ServiceRegisterCategory::Zk:
      return "Zk";
    default:
      return "None";
  }
}

auto ServiceRegister::Str2Category(const std::string &str) -> ServiceRegisterCategory {
  std::string strTemp = str;
  std::transform(str.begin(), str.end(), strTemp.begin(), tolower);
  if (strTemp == "zk") return ServiceRegisterCategory::Zk;
  return ServiceRegisterCategory::None;
}

}  // namespace tirpc