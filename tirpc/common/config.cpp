#include "tirpc/common/config.hpp"

#include <algorithm>
#include <cassert>
#include <cstdio>
#include <iostream>
#include <memory>

#include <yaml-cpp/node/parse.h>

#include "tirpc/common/const.hpp"
#include "tirpc/common/log.hpp"
#include "tirpc/net/base/address.hpp"

namespace tirpc {

extern Logger::ptr g_rpc_logger;

Config::Config(const std::string &file_path) : file_path_(file_path) {
  try {
    yaml_ = YAML::LoadFile(file_path_);
  } catch (YAML::Exception &e) {
    std::cerr << "Read config file error! file_path: " << file_path_ << ", error_info: " << e.what() << std::endl;
    exit(0);
  }
}

/**
 * @brief Helper function to read the text content of an XML element
 *
 * @param node
 * @param element_name
 * @return std::string
 */
static auto GetElementText(const YAML::Node &node, const std::string &element_name) -> std::string {
  auto nnode = node[element_name];
  if (!nnode) {
    std::cerr << "start tirpc server error! Cannot read [" << element_name << "] xml node" << std::endl;
    exit(0);
  }
  return nnode.as<std::string>();
}

template <typename T>
static auto GetElementType(const YAML::Node &node, const std::string &element_name) -> T {
  throw std::runtime_error("unsupported type");
}

template <>
auto GetElementType<int>(const YAML::Node &node, const std::string &element_name) -> int {
  return std::atoi(GetElementText(node, element_name).c_str());
}

template <>
auto GetElementType<std::string>(const YAML::Node &node, const std::string &element_name) -> std::string {
  return GetElementText(node, element_name);
}

void Config::ReadLogConfig(const YAML::Node &log_node) {
  log_path_ = GetElementType<std::string>(log_node, "log_path");
  log_prefix_ = GetElementType<std::string>(log_node, "log_prefix");

  int log_max_size = GetElementType<int>(log_node, "log_max_file_size");
  log_max_size_ = log_max_size * 1024 * 1024;

  level_ = StringToLevel(GetElementType<std::string>(log_node, "rpc_log_level"));

  app_log_level_ = StringToLevel(GetElementType<std::string>(log_node, "app_log_level"));

  log_sync_interval_ = GetElementType<int>(log_node, "log_sync_interval");

  log_to_console_ = static_cast<bool>(GetElementType<int>(log_node, "log_to_console"));

  g_rpc_logger = std::make_shared<Logger>();
  g_rpc_logger->Init(log_prefix_.c_str(), log_path_.c_str(), log_max_size_, log_sync_interval_);
}

void Config::ReadConf() {
  const YAML::Node &log_node = yaml_["log"];
  if (!log_node) {
    printf("start tirpc server error! read config file [%s] error, cannot read [log] xml node\n", file_path_.c_str());
    exit(0);
  }

  ReadLogConfig(log_node);

  const YAML::Node &time_wheel_node = yaml_["time_wheel"];
  if (!time_wheel_node) {
    printf("start tirpc server error! read config file [%s] error, cannot read [time_wheel] xml node\n",
           file_path_.c_str());
    exit(0);
  }

  const YAML::Node &coroutine_node = yaml_["coroutine"];
  if (!coroutine_node) {
    printf("start tirpc server error! read config file [%s] error, cannot read [coroutine] xml node\n",
           file_path_.c_str());
    exit(0);
  }

  int cor_stack_size = GetElementType<int>(coroutine_node, "coroutine_stack_size");
  cor_stack_size_ = 1024 * cor_stack_size;

  cor_pool_size_ = GetElementType<int>(coroutine_node, "coroutine_pool_size");

  msg_req_len_ = GetElementType<int>(yaml_, "msg_req_len");

  int max_connect_timeout = GetElementType<int>(yaml_, "max_connect_timeout");
  max_connect_timeout_ = max_connect_timeout * 1000;

  iothread_num_ = GetElementType<int>(yaml_, "iothread_num");

  timewheel_bucket_num_ = GetElementType<int>(time_wheel_node, "bucket_num");
  timewheel_interval_ = GetElementType<int>(time_wheel_node, "interval");

  // NOTE: Currently only support ZooKeeper
  const YAML::Node &service_register_node = yaml_["service_register"];
  std::string sr_type = GetElementType<std::string>(service_register_node, "type");
  if (sr_type == "zk") {
    service_register_ = ServiceRegisterCategory::Zk;
    zk_ip_ = GetElementType<std::string>(service_register_node, "ip");
    zk_port_ = GetElementType<int>(service_register_node, "port");
    zk_timeout_ = GetElementType<int>(service_register_node, "timeout");
  } else if (sr_type == "none") {
    service_register_ = ServiceRegisterCategory::None;
  }

  const YAML::Node &net_node = yaml_["server"];
  if (!net_node) {
    printf("start tirpc server error! read config file [%s] error, cannot read [server] xml node\n",
           file_path_.c_str());
    exit(0);
  }

  std::string ip = GetElementType<std::string>(net_node, "ip");

  if (ip.empty()) {
    ip = "0.0.0.0";
  }

  int port = GetElementType<int>(net_node, "port");

  if (port == 0) {
    printf("start tirpc server error! read config file [%s] error, read [server.port] = 0\n", file_path_.c_str());
    exit(0);
  }
  std::string protocal = GetElementType<std::string>(net_node, "protocal");

  std::transform(protocal.begin(), protocal.end(), protocal.begin(), toupper);

  addr_ = std::make_shared<tirpc::IPAddress>(ip, port);

  char buff[512];
  sprintf(buff,
          "read config from file [%s]: [log_path: %s], [log_prefix: %s], [log_max_size: %d MB], [log_level: %s], "
          "[coroutine_stack_size: %d KB], [coroutine_pool_size: %d], "
          "[msg_req_len: %d], [max_connect_timeout: %d s], "
          "[iothread_num:%d], [timewheel_bucket_num: %d], [timewheel_interval: %d s], [server_ip: %s], [server_Port: "
          "%d], [server_protocal: %s]\n",
          file_path_.c_str(), log_path_.c_str(), log_prefix_.c_str(), log_max_size_ / 1024 / 1024,
          LevelToString(level_).c_str(), cor_stack_size, cor_pool_size_, msg_req_len_, max_connect_timeout,
          iothread_num_, timewheel_bucket_num_, timewheel_interval_, ip.c_str(), port, protocal.c_str());

  std::string s(buff);
  LOG_DEBUG << s;
}

Config::~Config() {}

}  // namespace tirpc
