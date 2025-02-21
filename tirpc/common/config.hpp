#pragma once

#include <tinyxml2.h>
#include <yaml-cpp/yaml.h>
#include <memory>
#include <string>

#include "tirpc/common/const.hpp"
#include "tirpc/net/base/address.hpp"

#ifdef DECLARE_MYSQL_PLUGIN
#include "tirpc/common/mysql_instance.hpp"
#endif

namespace tirpc {

enum LogLevel { TRACE, DEBUG, INFO, WARN, ERROR, FATAL, NONE };

class Config {
 public:
  using ptr = std::shared_ptr<Config>;

  explicit Config(const std::string &file_path);

  ~Config();

  void ReadConf();

  void ReadLogConfig(const YAML::Node &node);

  auto GetAddr() -> IPAddress::ptr { return addr_; }

 public:
  // log params
  std::string log_path_;
  std::string log_prefix_;
  int log_max_size_{0};
  LogLevel level_{LogLevel::DEBUG};
  LogLevel app_log_level_{LogLevel::DEBUG};
  int log_sync_interval_{500};
  bool log_to_console_{true};

  // coroutine params
  int cor_stack_size_{0};
  int cor_pool_size_{0};

  int msg_req_len_{0};

  int max_connect_timeout_{0};
  int iothread_num_{0};

  int timewheel_bucket_num_{0};
  int timewheel_interval_{0};

#ifdef DECLARE_MYSQL_PLUGIN
  std::map<std::string, MySQLOption> mysql_options_;
#endif

  IPAddress::ptr addr_;

  ServiceRegisterCategory service_register_;

  /// Zookeeper
  std::string zk_ip_{"127.0.0.1"};
  int zk_port_{2181};
  int zk_timeout_{30000};

 private:
  std::string file_path_;

  YAML::Node yaml_;
};

}  // namespace tirpc