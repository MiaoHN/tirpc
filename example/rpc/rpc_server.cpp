#include <google/protobuf/service.h>
#include <memory>
#include <string>

#include "rpc_server.pb.h"

#include "tirpc/common/config.hpp"
#include "tirpc/common/log.hpp"
#include "tirpc/common/mutex.hpp"
#include "tirpc/common/start.hpp"
#include "tirpc/net/rpc/rpc_server.hpp"

static int i = 0;

tirpc::CoroutineMutex g_cor_mutex;

class QueryServiceImpl : public QueryService {
 public:
  QueryServiceImpl() = default;
  ~QueryServiceImpl() override = default;

  void query_name(google::protobuf::RpcController *controller, const ::queryNameReq *request, ::queryNameRes *response,
                  ::google::protobuf::Closure *done) override {
    APP_LOG_INFO("QueryServiceImpl.query_name, req={%s}", request->ShortDebugString().c_str());
    response->set_id(request->id());

    response->set_name("404notfixed");

    APP_LOG_INFO("QueryServiceImpl.query_name, res={%s}", response->ShortDebugString().c_str());

    if (done != nullptr) {
      done->Run();
    }
  }

  void query_age(google::protobuf::RpcController *controller, const ::queryAgeReq *request, ::queryAgeRes *response,
                 ::google::protobuf::Closure *done) override {
    APP_LOG_INFO("QueryServiceImpl.query_age, req={%s}", request->ShortDebugString().c_str());
    // APP_LOG_INFO << "QueryServiceImpl.query_age, sleep 6 s begin";
    // sleep(6);
    // APP_LOG_INFO << "QueryServiceImpl.query_age, sleep 6 s end";

    response->set_ret_code(0);
    response->set_res_info("OK");
    response->set_req_no(request->req_no());
    response->set_id(request->id());
    response->set_age(100100111);

    g_cor_mutex.Lock();
    APP_LOG_DEBUG("begin i = %d", i);
    sleep(1);
    i++;
    APP_LOG_DEBUG("end i = %d", i);
    g_cor_mutex.Unlock();

    if (done != nullptr) {
      done->Run();
    }
    // printf("response = %s\n", response->ShortDebugString().c_str());

    APP_LOG_INFO("QueryServiceImpl.query_age, res={%s}", response->ShortDebugString().c_str());
  }
};

auto main(int argc, char *argv[]) -> int {
  // default config file
  std::string config_file = "./conf/rpc_server.yml";
  int port = -1;

  if (argc == 2) {
    config_file = argv[1];
  } else if (argc == 4) {
    if (std::strcmp(argv[2], "-p") != 0) {
      std::cout << "usage: " << argv[0] << " <config> -p <port>" << std::endl;
    }
    config_file = argv[1];
    port = std::stoi(argv[3]);
  }

  tirpc::Config::LoadFromFile(config_file);

  auto server = std::make_shared<tirpc::RpcServer>();

  server->RegisterService(std::make_shared<QueryServiceImpl>());

  tirpc::StartServer(server);

  return 0;
}
