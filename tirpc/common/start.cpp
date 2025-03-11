#include "tirpc/common/start.hpp"

#include <google/protobuf/service.h>

#include "tirpc/common/config.hpp"
#include "tirpc/common/log.hpp"
#include "tirpc/coroutine/coroutine_hook.hpp"
#include "tirpc/net/tcp/tcp_server.hpp"

namespace tirpc {

TcpServer::ptr g_rpc_server;

static const std::string tirpc_banner[] = {  //
    "████████╗██╗██████╗ ██████╗  ██████╗",  //
    "╚══██╔══╝██║██╔══██╗██╔══██╗██╔════╝",  //
    "   ██║   ██║██████╔╝██████╔╝██║     ",  //
    "   ██║   ██║██╔══██╗██╔═══╝ ██║     ",  //
    "   ██║   ██║██║  ██║██║     ╚██████╗",  //
    "   ╚═╝   ╚═╝╚═╝  ╚═╝╚═╝      ╚═════╝"};

static void ShowBanner() {
  std::cout << std::endl;
  for (const auto &line : tirpc_banner) {
    std::cout << line << std::endl;
  }
  std::cout << " :: TiRPC ::        (v1.0.1.DEVELOP)\n" << std::endl;
}

void StartServer(TcpServer::ptr server) {
  ShowBanner();
  g_rpc_server = server;
  Logger::GetLogger()->Start();
  g_rpc_server->Start();
}

auto GetServer() -> TcpServer::ptr { return g_rpc_server; }

auto GetIOThreadPoolSize() -> int { return g_rpc_server->GetIoThreadPool()->GetIoThreadPoolSize(); }

void AddTimerEvent(TimerEvent::ptr event) {}

}  // namespace tirpc