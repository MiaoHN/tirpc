#include <google/protobuf/descriptor.h>
#include <google/protobuf/service.h>

#include "tirpc/net/service_register/zk_service_register.hpp"

namespace tirpc {

static const char *ROOT_PATH = "/tirpc";

ZkServiceRegister::ZkServiceRegister() { Init(); }

ZkServiceRegister::ZkServiceRegister(const std::string &ip, int port, int timeout) : zkCli_(ip, port, timeout) {
  Init();
}

void ZkServiceRegister::Init() {
  zkCli_.closeLog();
  // 启动zkclient客户端
  zkCli_.start();
  // 加入根节点
  zkCli_.create(ROOT_PATH, nullptr, 0);
}

void ZkServiceRegister::Register(std::shared_ptr<google::protobuf::Service> service, Address::ptr addr) {
  std::string serviceName = service->GetDescriptor()->full_name();
  serviceSet_.insert(serviceName);
  // znode路径：/tirpc/serviceName/ip:port
  std::string servicePath = ROOT_PATH;
  servicePath += "/" + serviceName;
  zkCli_.create(servicePath.c_str(), nullptr, 0);  // 不能递归注册节点，只能逐级注册
  servicePath += "/" + addr->ToString();
  zkCli_.create(servicePath.c_str(), nullptr, 0);
  pathSet_.insert(servicePath);
}

void ZkServiceRegister::Register(std::shared_ptr<CustomService> service, Address::ptr addr) {
  std::string serviceName = service->getServiceName() + "_Custom";
  serviceSet_.insert(serviceName);
  // znode路径：/tirpc/serviceName/ip:port
  std::string servicePath = ROOT_PATH;
  servicePath += "/" + serviceName;
  zkCli_.create(servicePath.c_str(), nullptr, 0);  // 不能递归注册节点，只能逐级注册
  servicePath += "/" + addr->ToString();
  zkCli_.create(servicePath.c_str(), nullptr, 0);
  pathSet_.insert(servicePath);
}

auto ZkServiceRegister::Discover(const std::string &serviceName) -> std::vector<Address::ptr> {
  std::string servicePath = ROOT_PATH;
  servicePath += "/" + serviceName;
  std::vector<std::string> strNodes = zkCli_.getChildrenNodes(servicePath);
  std::vector<Address::ptr> nodes;
  for (const auto &node : strNodes) {
    nodes.push_back(std::make_shared<tirpc::IPAddress>(node));
  }
  return nodes;
}

void ZkServiceRegister::Clear() {
  if (zkCli_.getIsConnected()) {
    // 先删除注册的地址对应的节点
    for (const std::string &path : pathSet_) {
      zkCli_.deleteNode(path.c_str());
    }
    // 然后如果子节点为空，就删除注册的服务对应的节点
    for (auto &sp : serviceSet_) {
      std::string servicePath = ROOT_PATH;
      servicePath += "/" + sp;
      if (zkCli_.getChildrenNodes(servicePath).empty()) {
        zkCli_.deleteNode(servicePath.c_str());
      }
    }
    if (zkCli_.getChildrenNodes(ROOT_PATH).empty()) {
      zkCli_.deleteNode(ROOT_PATH);
    }
    // 断开与zkserver的连接
    zkCli_.stop();
  }
}

ZkServiceRegister::~ZkServiceRegister() { Clear(); }

}  // namespace tirpc