#include "tirpc/common/config.hpp"

#include <fstream>

namespace tirpc {

// 静态成员初始化
ConfigMap<std::string> Config::global_map_;

void Config::LoadFromFile(const std::string &filename) {
  std::ifstream file(filename);
  if (!file.is_open()) {
    throw std::runtime_error("Cannot open config file: " + filename);
  }

  Clear();
  std::string line;
  std::string current_section = "";

  while (std::getline(file, line)) {
    trim(line);

    // 跳过空行和注释
    if (line.empty() || line[0] == '#' || line[0] == ';') {
      continue;
    }

    // 处理段标识 [section]
    if (line.front() == '[' && line.back() == ']') {
      current_section = line.substr(1, line.size() - 2);
      trim(current_section);
      continue;
    }

    // 解析键值对
    size_t delimiter_pos = line.find('=');
    if (delimiter_pos == std::string::npos) {
      continue;  // 跳过无效行
    }

    std::string key = line.substr(0, delimiter_pos);
    std::string value = line.substr(delimiter_pos + 1);
    trim(key);
    trim(value);

    // 移除字符串值两端的引号
    if (value.size() >= 2 &&
        ((value.front() == '"' && value.back() == '"') || (value.front() == '\'' && value.back() == '\''))) {
      value = value.substr(1, value.size() - 2);
    }

    // 生成完整键名（段名.键名）
    std::string full_key;
    if (!current_section.empty()) {
      full_key = current_section + "." + key;
    } else {
      full_key = key;
    }

    // 存储到全局配置映射
    global_map_.Set(full_key, value);
  }
}

void Config::Clear() { global_map_ = ConfigMap<std::string>(); }

}  // namespace tirpc
