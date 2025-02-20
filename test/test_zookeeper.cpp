#include "tirpc/common/const.hpp"
#include "tirpc/common/start.hpp"
#include "tirpc/common/zk_util.hpp"
#include "tirpc/net/tcp/abstract_service_register.hpp"
#include "tirpc/net/tcp/service_register.hpp"

int main(int argc, char **argv) {
  // default config file
  std::string config_file = "./conf/test_zookeeper.xml";

  if (argc == 2) {
    config_file = argv[1];
  }

  tirpc::InitConfig(config_file.c_str());

  tirpc::ZkClient client;
  client.closeLog();
  client.start();
  client.create("/tirpc", nullptr, 0);
  // tirpc::AbstractServiceRegister::ptr register_ = tirpc::ServiceRegister::Query(tirpc::ServiceRegisterCategory::Zk);

  return 0;
}