#include "tirpc/net/rpc/rpc_server.hpp"

#include "tirpc/net/rpc/rpc_dispatcher.hpp"
#include "tirpc/net/tcp/tcp_server.hpp"

namespace tirpc {

RpcServer::RpcServer(NetAddress::ptr addr) : TcpServer(addr) {
  dispatcher_ = std::make_shared<RpcDispatcher>();
  codec_ = std::make_shared<TinyPbCodeC>();
  start_info_ = "RPC service has started and is available at " + addr->ToString() + "\n Press Ctrl+C to stop";
}

auto RpcServer::RegisterService(std::shared_ptr<google::protobuf::Service> service) -> bool {
  if (service) {
    dynamic_cast<RpcDispatcher *>(dispatcher_.get())->RegisterService(service);
  } else {
    ErrorLog << "register service error, service ptr is nullptr";
    return false;
  }
  return true;
}

}  // namespace tirpc
