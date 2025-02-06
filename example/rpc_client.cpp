#include <google/protobuf/service.h>
#include <iostream>

#include "rpc_server.pb.h"
#include "tirpc/net/address.hpp"
#include "tirpc/net/rpc/rpc_async_channel.hpp"
#include "tirpc/net/rpc/rpc_channel.hpp"
#include "tirpc/net/rpc/rpc_closure.hpp"
#include "tirpc/net/rpc/rpc_controller.hpp"

void TestClient() {
  tirpc::IPAddress::ptr addr = std::make_shared<tirpc::IPAddress>("127.0.0.1", 39999);

  tirpc::RpcChannel channel(addr);
  QueryService_Stub stub(&channel);

  tirpc::RpcController rpc_controller;
  rpc_controller.SetTimeout(5000);

  queryAgeReq rpc_req;
  queryAgeRes rpc_res;

  std::cout << "Send to tirpc server " << addr->ToString() << ", requeset body: " << rpc_req.ShortDebugString()
            << std::endl;
  stub.query_age(&rpc_controller, &rpc_req, &rpc_res, nullptr);

  if (rpc_controller.ErrorCode() != 0) {
    std::cout << "Failed to call tirpc server, error code: " << rpc_controller.ErrorCode()
              << ", error info: " << rpc_controller.ErrorText() << std::endl;
    return;
  }

  std::cout << "Success get response frrom tirpc server " << addr->ToString()
            << ", response body: " << rpc_res.ShortDebugString() << std::endl;
}

auto main(int argc, char *argv[]) -> int {
  TestClient();

  return 0;
}
