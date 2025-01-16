#include "tirpc/common/config.hpp"

#include <algorithm>
#include <cassert>
#include <cstdio>
#include <memory>

#include <tinyxml.h>

#include "tirpc/common/log.hpp"
#include "tirpc/net/abstract_codec.hpp"
#include "tirpc/net/address.hpp"
#include "tirpc/net/tcp/tcp_server.hpp"

namespace tirpc {

extern Logger::ptr g_rpc_logger;
extern TcpServer::ptr g_rpc_server;

Config::Config(const char *file_path) : file_path_(std::string(file_path)) {
  xml_file_ = new tinyxml2::XMLDocument();
  bool rt = xml_file_->LoadFile(file_path) != 0U;
  if (rt) {
    printf(
        "start tirpc server error! read conf file [%s] error info: [%s], errorid: [%d], error_line_number:[%d"
        "]\n",
        file_path, xml_file_->ErrorStr(), xml_file_->ErrorID(), xml_file_->ErrorLineNum());
    exit(0);
  }
}

void Config::ReadLogConfig(tinyxml2::XMLElement *log_node) {
  tinyxml2::XMLElement *node = log_node->FirstChildElement("log_path");
  if ((node == nullptr) || (node->GetText() == nullptr)) {
    printf("start tirpc server error! read config file [%s] error, cannot read [log_path] xml node\n",
           file_path_.c_str());
    exit(0);
  }
  log_path_ = std::string(node->GetText());

  node = log_node->FirstChildElement("log_prefix");
  if ((node == nullptr) || (node->GetText() == nullptr)) {
    printf("start tirpc server error! read config file [%s] error, cannot read [log_prefix] xml node\n",
           file_path_.c_str());
    exit(0);
  }
  log_prefix_ = std::string(node->GetText());

  node = log_node->FirstChildElement("log_max_file_size");
  if ((node == nullptr) || (node->GetText() == nullptr)) {
    printf("start tirpc server error! read config file [%s] error, cannot read [log_max_file_size] xml node\n",
           file_path_.c_str());
    exit(0);
  }

  int log_max_size = std::atoi(node->GetText());
  log_max_size_ = log_max_size * 1024 * 1024;

  node = log_node->FirstChildElement("rpc_log_level");
  if ((node == nullptr) || (node->GetText() == nullptr)) {
    printf("start tirpc server error! read config file [%s] error, cannot read [rpc_log_level] xml node\n",
           file_path_.c_str());
    exit(0);
  }

  std::string log_level = std::string(node->GetText());
  level_ = StringToLevel(log_level);

  node = log_node->FirstChildElement("app_log_level");
  if ((node == nullptr) || (node->GetText() == nullptr)) {
    printf("start tirpc server error! read config file [%s] error, cannot read [app_log_level] xml node\n",
           file_path_.c_str());
    exit(0);
  }

  log_level = std::string(node->GetText());
  app_log_level_ = StringToLevel(log_level);

  node = log_node->FirstChildElement("log_sync_interval");
  if ((node == nullptr) || (node->GetText() == nullptr)) {
    printf("start tirpc server error! read config file [%s] error, cannot read [log_sync_interval] xml node\n",
           file_path_.c_str());
    exit(0);
  }

  log_sync_interval_ = std::atoi(node->GetText());

  g_rpc_logger = std::make_shared<Logger>();
  g_rpc_logger->Init(log_prefix_.c_str(), log_path_.c_str(), log_max_size_, log_sync_interval_);
}

void Config::ReadDBConfig(tinyxml2::XMLElement *node) {
#ifdef DECLARE_MYSQL_PLUGIN

  printf("read db config\n");
  if (!node) {
    printf("start tirpc server error! read config file [%s] error, cannot read [database] xml node\n",
           file_path_.c_str());
    exit(0);
  }
  for (tinyxml2::XMLElement *element = node->FirstChildElement("db_key"); element != NULL;
       element = element->NextSiblingElement()) {
    std::string key = element->FirstAttribute()->Value();
    printf("key is %s\n", key.c_str());
    tinyxml2::XMLElement *ip_e = element->FirstChildElement("ip");
    std::string ip;
    int port = 3306;
    if (ip_e) {
      ip = std::string(ip_e->GetText());
    }
    if (ip.empty()) {
      continue;
    }

    tinyxml2::XMLElement *port_e = element->FirstChildElement("port");
    if (port_e && port_e->GetText()) {
      port = std::atoi(port_e->GetText());
    }

    MySQLOption option(IPAddress(ip, port));

    tinyxml2::XMLElement *user_e = element->FirstChildElement("user");
    if (user_e && user_e->GetText()) {
      option.user_ = std::string(user_e->GetText());
    }

    tinyxml2::XMLElement *passwd_e = element->FirstChildElement("passwd");
    if (passwd_e && passwd_e->GetText()) {
      option.passwd_ = std::string(passwd_e->GetText());
    }

    tinyxml2::XMLElement *select_db_e = element->FirstChildElement("select_db");
    if (select_db_e && select_db_e->GetText()) {
      option.select_db_ = std::string(select_db_e->GetText());
    }

    tinyxml2::XMLElement *char_set_e = element->FirstChildElement("char_set");
    if (char_set_e && char_set_e->GetText()) {
      option.char_set_ = std::string(char_set_e->GetText());
    }
    mysql_options_.insert(std::make_pair(key, option));
    char buf[512];
    sprintf(buf, "read config from file [%s], key:%s {addr: %s, user: %s, passwd: %s, select_db: %s, charset: %s}\n",
            file_path_.c_str(), key.c_str(), option.addr_.toString().c_str(), option.user_.c_str(),
            option.passwd_.c_str(), option.select_db_.c_str(), option.char_set_.c_str());
    std::string s(buf);
    InfoLog << s;
  }

#endif
}

void Config::ReadConf() {
  tinyxml2::XMLElement *root = xml_file_->RootElement();
  tinyxml2::XMLElement *log_node = root->FirstChildElement("log");
  if (log_node == nullptr) {
    printf("start tirpc server error! read config file [%s] error, cannot read [log] xml node\n", file_path_.c_str());
    exit(0);
  }

  ReadLogConfig(log_node);

  tinyxml2::XMLElement *time_wheel_node = root->FirstChildElement("time_wheel");
  if (time_wheel_node == nullptr) {
    printf("start tirpc server error! read config file [%s] error, cannot read [time_wheel] xml node\n",
           file_path_.c_str());
    exit(0);
  }

  tinyxml2::XMLElement *coroutine_node = root->FirstChildElement("coroutine");
  if (coroutine_node == nullptr) {
    printf("start tirpc server error! read config file [%s] error, cannot read [coroutine] xml node\n",
           file_path_.c_str());
    exit(0);
  }

  if ((coroutine_node->FirstChildElement("coroutine_stack_size") == nullptr) ||
      (coroutine_node->FirstChildElement("coroutine_stack_size")->GetText() == nullptr)) {
    printf(
        "start tirpc server error! read config file [%s] error, cannot read [coroutine.coroutine_stack_size] xml "
        "node\n",
        file_path_.c_str());
    exit(0);
  }

  if ((coroutine_node->FirstChildElement("coroutine_pool_size") == nullptr) ||
      (coroutine_node->FirstChildElement("coroutine_pool_size")->GetText() == nullptr)) {
    printf(
        "start tirpc server error! read config file [%s] error, cannot read [coroutine.coroutine_pool_size] xml node\n",
        file_path_.c_str());
    exit(0);
  }

  int cor_stack_size = std::atoi(coroutine_node->FirstChildElement("coroutine_stack_size")->GetText());
  cor_stack_size_ = 1024 * cor_stack_size;
  cor_pool_size_ = std::atoi(coroutine_node->FirstChildElement("coroutine_pool_size")->GetText());

  if ((root->FirstChildElement("msg_req_len") == nullptr) ||
      (root->FirstChildElement("msg_req_len")->GetText() == nullptr)) {
    printf("start tirpc server error! read config file [%s] error, cannot read [msg_req_len] xml node\n",
           file_path_.c_str());
    exit(0);
  }

  msg_req_len_ = std::atoi(root->FirstChildElement("msg_req_len")->GetText());

  if ((root->FirstChildElement("max_connect_timeout") == nullptr) ||
      (root->FirstChildElement("max_connect_timeout")->GetText() == nullptr)) {
    printf("start tirpc server error! read config file [%s] error, cannot read [max_connect_timeout] xml node\n",
           file_path_.c_str());
    exit(0);
  }
  int max_connect_timeout = std::atoi(root->FirstChildElement("max_connect_timeout")->GetText());
  max_connect_timeout_ = max_connect_timeout * 1000;

  if ((root->FirstChildElement("iothread_num") == nullptr) ||
      (root->FirstChildElement("iothread_num")->GetText() == nullptr)) {
    printf("start tirpc server error! read config file [%s] error, cannot read [iothread_num] xml node\n",
           file_path_.c_str());
    exit(0);
  }

  iothread_num_ = std::atoi(root->FirstChildElement("iothread_num")->GetText());

  if ((time_wheel_node->FirstChildElement("bucket_num") == nullptr) ||
      (time_wheel_node->FirstChildElement("bucket_num")->GetText() == nullptr)) {
    printf("start tirpc server error! read config file [%s] error, cannot read [time_wheel.bucket_num] xml node\n",
           file_path_.c_str());
    exit(0);
  }
  if ((time_wheel_node->FirstChildElement("interval") == nullptr) ||
      (time_wheel_node->FirstChildElement("interval")->GetText() == nullptr)) {
    printf("start tirpc server error! read config file [%s] error, cannot read [time_wheel.interval] xml node\n",
           file_path_.c_str());
    exit(0);
  }
  timewheel_bucket_num_ = std::atoi(time_wheel_node->FirstChildElement("bucket_num")->GetText());
  timewheel_interval_ = std::atoi(time_wheel_node->FirstChildElement("interval")->GetText());

  tinyxml2::XMLElement *net_node = root->FirstChildElement("server");
  if (net_node == nullptr) {
    printf("start tirpc server error! read config file [%s] error, cannot read [server] xml node\n",
           file_path_.c_str());
    exit(0);
  }

  if ((net_node->FirstChildElement("ip") == nullptr) || (net_node->FirstChildElement("port") == nullptr) ||
      (net_node->FirstChildElement("protocal") == nullptr)) {
    printf(
        "start tirpc server error! read config file [%s] error, cannot read [server.ip] or [server.port] or "
        "[server.protocal] xml node\n",
        file_path_.c_str());
    exit(0);
  }
  std::string ip = std::string(net_node->FirstChildElement("ip")->GetText());
  if (ip.empty()) {
    ip = "0.0.0.0";
  }
  int port = std::atoi(net_node->FirstChildElement("port")->GetText());
  if (port == 0) {
    printf("start tirpc server error! read config file [%s] error, read [server.port] = 0\n", file_path_.c_str());
    exit(0);
  }
  std::string protocal = std::string(net_node->FirstChildElement("protocal")->GetText());
  std::transform(protocal.begin(), protocal.end(), protocal.begin(), toupper);

  tirpc::IPAddress::ptr addr = std::make_shared<tirpc::IPAddress>(ip, port);

  if (protocal == "HTTP") {
    g_rpc_server = std::make_shared<TcpServer>(addr, Http_Protocal);
  } else {
    g_rpc_server = std::make_shared<TcpServer>(addr, TinyPb_Protocal);
  }

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
  InfoLog << s;

  tinyxml2::XMLElement *database_node = root->FirstChildElement("database");

  if (database_node != nullptr) {
    ReadDBConfig(database_node);
  }
}

Config::~Config() {
  if (xml_file_ != nullptr) {
    delete xml_file_;
    xml_file_ = nullptr;
  }
}

auto Config::GetXmlNode(const std::string &name) -> tinyxml2::XMLElement * {
  return xml_file_->RootElement()->FirstChildElement(name.c_str());
}

}  // namespace tirpc
