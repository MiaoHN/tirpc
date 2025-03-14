#include <google/protobuf/service.h>
#include <chrono>
#include <iostream>
#include <vector>

#include "rpc_server.pb.h"
#include "tirpc/common/config.hpp"
#include "tirpc/common/const.hpp"
#include "tirpc/common/start.hpp"
#include "tirpc/net/base/address.hpp"
#include "tirpc/net/rpc/rpc_channel.hpp"
#include "tirpc/net/rpc/rpc_controller.hpp"
#include "tirpc/net/tcp/abstract_service_register.hpp"
#include "tirpc/net/tcp/service_register.hpp"

// 模式一：创建一个 channel，使用这个 channel 进行多次调用
void TestClientSingleChannel(int numCalls) {
  tirpc::AbstractServiceRegister::ptr center = tirpc::ServiceRegister::Query(tirpc::ServiceRegisterCategory::Zk);
  std::vector<tirpc::Address::ptr> addrs = center->Discover("QueryService");

  tirpc::RpcChannel channel(addrs, tirpc::LoadBalanceCategory::Random);
  QueryService_Stub stub(&channel);

  tirpc::RpcController rpc_controller;
  rpc_controller.SetTimeout(1000000);

  queryNameReq rpc_req;
  queryNameRes rpc_res;

  std::chrono::high_resolution_clock::time_point start = std::chrono::high_resolution_clock::now();

  int successCount = 0;
  for (int i = 0; i < numCalls; ++i) {
    rpc_controller.Reset();  // 重置控制器状态
    stub.query_name(&rpc_controller, &rpc_req, &rpc_res, nullptr);

    if (rpc_controller.ErrorCode() != 0) {
      std::cout << "Call " << i + 1 << " failed, error code: " << rpc_controller.ErrorCode()
                << ", error info: " << rpc_controller.ErrorText() << std::endl;
    } else {
      ++successCount;
    }
  }

  std::chrono::high_resolution_clock::time_point end = std::chrono::high_resolution_clock::now();
  std::chrono::duration<double, std::milli> duration = end - start;

  std::cout << "Single Channel Test - Total calls: " << numCalls << ", Successful calls: " << successCount << std::endl;
  std::cout << "Single Channel Test - Total time: " << duration.count() << " ms" << std::endl;
  if (successCount > 0) {
    std::cout << "Single Channel Test - Average time per successful call: " << duration.count() / successCount << " ms"
              << std::endl;
  }
}

// 模式二：每次调用 RPC 时新建 channel
void TestClientMultipleChannels(int numCalls) {
  tirpc::AbstractServiceRegister::ptr center = tirpc::ServiceRegister::Query(tirpc::ServiceRegisterCategory::Zk);
  std::vector<tirpc::Address::ptr> addrs = center->Discover("QueryService");

  std::chrono::high_resolution_clock::time_point start = std::chrono::high_resolution_clock::now();

  int successCount = 0;
  for (int i = 0; i < numCalls; ++i) {
    tirpc::RpcChannel channel(addrs, tirpc::LoadBalanceCategory::Random);
    QueryService_Stub stub(&channel);

    tirpc::RpcController rpc_controller;
    rpc_controller.SetTimeout(5000);

    queryNameReq rpc_req;
    queryNameRes rpc_res;

    rpc_controller.Reset();  // 重置控制器状态
    stub.query_name(&rpc_controller, &rpc_req, &rpc_res, nullptr);

    if (rpc_controller.ErrorCode() != 0) {
      std::cout << "Call " << i + 1 << " failed, error code: " << rpc_controller.ErrorCode()
                << ", error info: " << rpc_controller.ErrorText() << std::endl;
    } else {
      ++successCount;
    }
  }

  std::chrono::high_resolution_clock::time_point end = std::chrono::high_resolution_clock::now();
  std::chrono::duration<double, std::milli> duration = end - start;

  std::cout << "Multiple Channels Test - Total calls: " << numCalls << ", Successful calls: " << successCount
            << std::endl;
  std::cout << "Multiple Channels Test - Total time: " << duration.count() << " ms" << std::endl;
  if (successCount > 0) {
    std::cout << "Multiple Channels Test - Average time per successful call: " << duration.count() / successCount
              << " ms" << std::endl;
  }
}

auto main(int argc, char *argv[]) -> int {
  // default config file
  std::string config_file = "./conf/rpc_client.yml";

  int numCalls = 10000;  // 默认调用次数
  if (argc == 2) {
    config_file = argv[1];
  } else if (argc == 3) {
    config_file = argv[1];
    numCalls = std::stoi(argv[2]);
  }

  tirpc::Config::LoadFromFile(config_file);

  // 执行两种模式的测试
  TestClientSingleChannel(numCalls);
  std::cout << std::endl;
  TestClientMultipleChannels(numCalls);

  return 0;
}