#include <google/protobuf/service.h>
#include <iostream>

#include "rpc_server.pb.h"
#include "tirpc/common/const.hpp"
#include "tirpc/common/start.hpp"
#include "tirpc/net/base/address.hpp"
#include "tirpc/net/rpc/rpc_async_channel.hpp"
#include "tirpc/net/rpc/rpc_channel.hpp"
#include "tirpc/net/rpc/rpc_closure.hpp"
#include "tirpc/net/rpc/rpc_controller.hpp"
#include "tirpc/net/tcp/abstract_service_register.hpp"
#include "tirpc/net/tcp/service_register.hpp"

void TestClient() {
  tirpc::AbstractServiceRegister::ptr center = tirpc::ServiceRegister::Query(tirpc::ServiceRegisterCategory::Zk);
  std::vector<tirpc::IPAddress::ptr> addrs = center->Discover("QueryService");

  tirpc::RpcChannel channel(addrs);
  QueryService_Stub stub(&channel);

  tirpc::RpcController rpc_controller;
  rpc_controller.SetTimeout(5000);

  queryAgeReq rpc_req;
  queryAgeRes rpc_res;

  stub.query_age(&rpc_controller, &rpc_req, &rpc_res, nullptr);

  if (rpc_controller.ErrorCode() != 0) {
    std::cout << "Failed to call tirpc server, error code: " << rpc_controller.ErrorCode()
              << ", error info: " << rpc_controller.ErrorText() << std::endl;
    return;
  }

  std::cout << "Success get response from tirpc server, response body: " << rpc_res.ShortDebugString() << std::endl;
}

auto main(int argc, char *argv[]) -> int {
  // default config file
  std::string config_file = "./conf/rpc_client.xml";

  if (argc == 2) {
    config_file = argv[1];
  }

  tirpc::InitConfig(config_file.c_str());

  TestClient();

  return 0;
}
