#include <google/protobuf/service.h>
#include <unistd.h>
#include <atomic>
#include <chrono>
#include <iostream>
#include <memory>
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

void WorkerFunction(const std::vector<tirpc::Address::ptr> addrs, std::atomic<int> &success_count, int duration) {
  tirpc::RpcChannel channel(addrs, tirpc::LoadBalanceCategory::Random);
  QueryService_Stub stub(&channel);

  tirpc::RpcController rpc_controller;
  rpc_controller.SetTimeout(5000);

  queryNameReq rpc_req;
  queryNameRes rpc_res;

  auto start_time = std::chrono::high_resolution_clock::now();
  auto end_time = start_time + std::chrono::seconds(duration);

  while (std::chrono::high_resolution_clock::now() < end_time) {
    rpc_controller.Reset();  // 重置控制器状态
    stub.query_name(&rpc_controller, &rpc_req, &rpc_res, nullptr);

    if (rpc_controller.ErrorCode() == 0) {
      success_count.fetch_add(1, std::memory_order_relaxed);
    }
  }
}

void RunBenchmark(int numClients, int duration, const std::vector<tirpc::Address::ptr> addrs) {
  // 单 channel 测试
  std::atomic<int> success_count(0);
  std::vector<std::thread> benchmark_threads;
  for (int i = 0; i < numClients; ++i) {
    benchmark_threads.emplace_back(WorkerFunction, addrs, std::ref(success_count), duration);
  }
  int joined = 0;
  for (auto &thread : benchmark_threads) {
    joined++;
    thread.join();
  }

  double qps = static_cast<double>(success_count) / static_cast<double>(duration);
  std::cout << "Total clients: " << numClients << ", Total time: " << duration << " s" << std::endl;
  std::cout << "Successful calls: " << success_count << std::endl;
  std::cout << "QPS: " << qps << std::endl;

  std::cout << std::endl;
}

auto main(int argc, char *argv[]) -> int {
  // default config file
  int num_clients = 1;
  int duration = 10;

  int opt;
  while ((opt = getopt(argc, argv, "c:t:")) != -1) {
    switch (opt) {
      case 'c':
        num_clients = std::stoi(optarg);
        break;
      case 't':
        duration = std::stoi(optarg);
        break;
      default:
        std::cerr << "Usage: " << argv[0] << " [-c num_clients] [-t duration]" << std::endl;
        return 1;
    }
  }

  std::cout << "Start benchmark!" << std::endl;
  std::cout << "Client: " << num_clients << ", Duration: " << duration << "s" << std::endl;

  std::vector<tirpc::Address::ptr> addrs = {std::make_shared<tirpc::IPAddress>("127.0.0.1", 39999)};

  RunBenchmark(num_clients, duration, addrs);

  return 0;
}