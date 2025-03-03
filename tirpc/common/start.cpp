#include "tirpc/common/start.hpp"

#include <google/protobuf/service.h>

#include "tirpc/common/config.hpp"
#include "tirpc/common/log.hpp"
#include "tirpc/coroutine/coroutine_hook.hpp"
#include "tirpc/net/tcp/tcp_server.hpp"

namespace tirpc {

Config::ptr g_rpc_config;
Logger::ptr g_rpc_logger;
TcpServer::ptr g_rpc_server;

static int g_init_config = 0;

void InitConfig(const char *file) {
  tirpc::SetHook(false);

#ifdef DECLARE_MYSQL_PULGIN
  int rt = mysql_library_init(0, NULL, NULL);
  if (rt != 0) {
    printf("Start tirpc server error, call mysql_library_init error\n");
    mysql_library_end();
    exit(0);
  }
#endif

  tirpc::SetHook(true);

  if (g_init_config == 0) {
    g_rpc_config = std::make_shared<tirpc::Config>(file);
    g_rpc_config->ReadConf();
    g_init_config = 1;
  }
}

// void RegisterService(google::protobuf::Service* service) {
//   g_rpc_server->registerService(service);
// }
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
  g_rpc_logger->Start();
  g_rpc_server->Start();
}

auto GetServer() -> TcpServer::ptr { return g_rpc_server; }

auto GetIOThreadPoolSize() -> int { return g_rpc_server->GetIoThreadPool()->GetIoThreadPoolSize(); }

auto GetConfig() -> Config::ptr { return g_rpc_config; }

void AddTimerEvent(TimerEvent::ptr event) {}

}  // namespace tirpc