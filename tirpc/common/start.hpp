#pragma once

#include <google/protobuf/service.h>
#include <cstdio>
#include <functional>
#include <memory>

#include "tirpc/common/log.hpp"
#include "tirpc/net/tcp/tcp_server.hpp"
#include "tirpc/net/timer.hpp"

namespace tirpc {

#define REGISTER_HTTP_SERVLET(path, servlet)                                                                \
  do {                                                                                                      \
    if (!tirpc::GetServer()->RegisterHttpServlet(path, std::make_shared<servlet>())) {                      \
      printf(                                                                                               \
          "Start tirpc server error, because register http servelt error, please look up rpc log get more " \
          "details!\n");                                                                                    \
      tirpc::Exit(0);                                                                                       \
    }                                                                                                       \
  } while (0)

#define REGISTER_SERVICE(service)                                                                               \
  do {                                                                                                          \
    if (!tirpc::GetServer()->RegisterService(std::make_shared<service>())) {                                    \
      printf(                                                                                                   \
          "Start tirpc server error, because register protobuf service error, please look up rpc log get more " \
          "details!\n");                                                                                        \
      tirpc::Exit(0);                                                                                           \
    }                                                                                                           \
  } while (0)

void InitConfig(const char *file);

// void RegisterService(google::protobuf::Service* service);

void StartRpcServer();

auto GetServer() -> TcpServer::ptr;

auto GetIOThreadPoolSize() -> int;

auto GetConfig() -> Config::ptr;

void AddTimerEvent(TimerEvent::ptr &event);

}  // namespace tirpc
