#include <google/protobuf/service.h>
#include <unistd.h>
#include <atomic>
#include <chrono>
#include <iostream>
#include <thread>
#include <vector>

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

void SingleChannelWorker(tirpc::AbstractServiceRegister::ptr center, std::atomic<int> &successCount, int duration) {
  std::vector<tirpc::Address::ptr> addrs = center->Discover("QueryService");

  tirpc::RpcChannel channel(addrs, tirpc::LoadBalanceCategory::Random);
  QueryService_Stub stub(&channel);

  tirpc::RpcController rpc_controller;
  rpc_controller.SetTimeout(1000000);

  queryNameReq rpc_req;
  queryNameRes rpc_res;

  auto startTime = std::chrono::high_resolution_clock::now();
  auto endTime = startTime + std::chrono::seconds(duration);

  while (std::chrono::high_resolution_clock::now() < endTime) {
    rpc_controller.Reset();  // 重置控制器状态
    stub.query_name(&rpc_controller, &rpc_req, &rpc_res, nullptr);

    if (rpc_controller.ErrorCode() == 0) {
      successCount.fetch_add(1, std::memory_order_relaxed);
    }
  }
}

void RunBenchmark(int numClients, int duration) {
  tirpc::AbstractServiceRegister::ptr center = tirpc::ServiceRegister::Query(tirpc::ServiceRegisterCategory::Zk);

  // 单 channel 测试
  std::atomic<int> singleChannelSuccessCount(0);
  std::vector<std::thread> singleChannelThreads;
  for (int i = 0; i < numClients; ++i) {
    singleChannelThreads.emplace_back(SingleChannelWorker, center, std::ref(singleChannelSuccessCount), duration);
  }
  for (auto &thread : singleChannelThreads) {
    thread.join();
  }

  double singleChannelQPS = static_cast<double>(singleChannelSuccessCount) / static_cast<double>(duration);
  std::cout << "Total clients: " << numClients << ", Total time: " << duration << " s" << std::endl;
  std::cout << "Successful calls: " << singleChannelSuccessCount << std::endl;
  std::cout << "QPS: " << singleChannelQPS << std::endl;

  std::cout << std::endl;
}

auto main(int argc, char *argv[]) -> int {
  // default config file
  std::string config_file = "./conf/rpc_client.yml";
  int numClients = 1;
  int duration = 10;

  int opt;
  while ((opt = getopt(argc, argv, "c:t:")) != -1) {
    switch (opt) {
      case 'c':
        numClients = std::stoi(optarg);
        break;
      case 't':
        duration = std::stoi(optarg);
        break;
      default:
        std::cerr << "Usage: " << argv[0] << " [-c num_clients] [-t duration] [config_file]" << std::endl;
        return 1;
    }
  }

  if (optind < argc) {
    config_file = argv[optind];
  }

  tirpc::InitConfig(config_file.c_str());

  RunBenchmark(numClients, duration);

  return 0;
}