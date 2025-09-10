#include "tirpc/net/rpc/rpc_server.hpp"

#include "tirpc/common/config.hpp"
#include "tirpc/common/const.hpp"
#include "tirpc/net/rpc/rpc_dispatcher.hpp"
#include "tirpc/net/tcp/service_register.hpp"
#include "tirpc/net/tcp/tcp_server.hpp"

namespace tirpc {

RpcServer::RpcServer() : TcpServer() {
  dispatcher_ = std::make_shared<RpcDispatcher>();
  codec_ = std::make_shared<TinyPbCodeC>();
  start_info_ = "RPC service has started and is available at " + addr_->ToString() + "\n Press Ctrl+C to stop";
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
      if (Config::Get<std::string>("service_register.type", "none") == "zk") {
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
