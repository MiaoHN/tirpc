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

ConfigVarBase::ptr Config::LookupBase(const std::string &name) {
  RWMutexType::ReadLocker lock(GetMutex());
  auto it = GetDatas().find(name);
  return it == GetDatas().end() ? nullptr : it->second;
}

//"A.B", 10
// A:
//  B: 10
//  C: str

static void ListAllMember(const std::string &prefix, const YAML::Node &node,
                          std::list<std::pair<std::string, const YAML::Node>> &output) {
  if (prefix.find_first_not_of("abcdefghikjlmnopqrstuvwxyz._012345678") != std::string::npos) {
    LOG_ERROR << "Config invalid name: " << prefix << " : " << node;
    return;
  }
  output.push_back(std::make_pair(prefix, node));
  if (node.IsMap()) {
    for (auto it = node.begin(); it != node.end(); ++it) {
      ListAllMember(prefix.empty() ? it->first.Scalar() : prefix + "." + it->first.Scalar(), it->second, output);
    }
  }
}

void Config::LoadFromYaml(const YAML::Node &root) {
  std::list<std::pair<std::string, const YAML::Node>> all_nodes;
  ListAllMember("", root, all_nodes);

  for (auto &i : all_nodes) {
    std::string key = i.first;
    if (key.empty()) {
      continue;
    }

    std::transform(key.begin(), key.end(), key.begin(), ::tolower);
    ConfigVarBase::ptr var = LookupBase(key);

    if (var) {
      if (i.second.IsScalar()) {
        var->FromString(i.second.Scalar());
      } else {
        std::stringstream ss;
        ss << i.second;
        var->FromString(ss.str());
      }
    }
  }
}

void Config::LoadFromFile(const std::string &filepath) {
  try {
    YAML::Node root = YAML::LoadFile(filepath);
    LoadFromYaml(root);
    LOG_INFO << "LoadConfFile file=" << filepath << " ok";
  } catch (...) {
    LOG_ERROR << "LoadConfFile file=" << filepath << " failed";
  }
}

void Config::Visit(std::function<void(ConfigVarBase::ptr)> cb) {
  RWMutexType::ReadLocker lock(GetMutex());
  ConfigVarMap &m = GetDatas();
  for (auto it = m.begin(); it != m.end(); ++it) {
    cb(it->second);
  }
}

}  // namespace tirpc
