#include "tirpc/net/rpc/rpc_server.hpp"

#include "tirpc/common/config.hpp"
#include "tirpc/common/const.hpp"
#include "tirpc/net/rpc/rpc_dispatcher.hpp"
#include "tirpc/net/tcp/service_register.hpp"
#include "tirpc/net/tcp/tcp_server.hpp"

namespace tirpc {

static ConfigVar<std::string>::ptr g_service_register =
    Config::Lookup("service_register.type", std::string("zk"), "Service Register");

RpcServer::RpcServer() {

}

RpcServer::RpcServer(Address::ptr addr) : TcpServer(addr) {
  dispatcher_ = std::make_shared<RpcDispatcher>();
  codec_ = std::make_shared<TinyPbCodeC>();
  start_info_ = "RPC service has started and is available at " + addr->ToString() + "\n Press Ctrl+C to stop";
}

auto RpcServer::RegisterService(std::shared_ptr<google::protobuf::Service> service) -> bool {
  if (service) {
    dynamic_cast<RpcDispatcher *>(dispatcher_.get())->RegisterService(service);
    if (!register_) {
      ServiceRegisterCategory category;
      if (g_service_register->GetValue() == "zk") {
        category = ServiceRegisterCategory::Zk;
      } else {
        category = ServiceRegisterCategory::None;
      }
      register_ = ServiceRegister::Query(category);
    }
    register_->Register(service, addr_);
  } else {
    LOG_ERROR << "register service error, service ptr is nullptr";
    return false;
  }
  return true;
}

}  // namespace tirpc
