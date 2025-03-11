#pragma once

#include <google/protobuf/service.h>
#include <cstdio>
#include <functional>
#include <memory>

#include "tirpc/common/log.hpp"
#include "tirpc/net/base/timer.hpp"
#include "tirpc/net/rpc/rpc_server.hpp"
#include "tirpc/net/tcp/tcp_server.hpp"

namespace tirpc {

void StartServer(TcpServer::ptr server);

auto GetServer() -> TcpServer::ptr;

auto GetIOThreadPoolSize() -> int;

void AddTimerEvent(TimerEvent::ptr event);

}  // namespace tirpc
