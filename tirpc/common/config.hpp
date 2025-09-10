#pragma once

#include <algorithm>
#include <cctype>
#include <fstream>
#include <map>
#include <sstream>
#include <stdexcept>
#include <string>

namespace tirpc {

// 辅助函数：去除字符串首尾空格
inline void trim(std::string &str) {
  // 删除前导空格
  str.erase(str.begin(), std::find_if(str.begin(), str.end(), [](int ch) { return !std::isspace(ch); }));
  // 删除尾部空格
  str.erase(std::find_if(str.rbegin(), str.rend(), [](int ch) { return !std::isspace(ch); }).base(), str.end());
}

// 辅助函数：转换字符串到具体类型
template <typename T>
T convert(const std::string &value) {
  std::istringstream iss(value);
  T result;
  if (!(iss >> result)) {
    throw std::runtime_error("Conversion error for value: " + value);
  }
  return result;
}

// 布尔类型的特化处理
template <>
inline bool convert<bool>(const std::string &value) {
  std::string lower;
  lower.reserve(value.size());
  std::transform(value.begin(), value.end(), std::back_inserter(lower), [](char c) { return std::tolower(c); });

  if (lower == "true" || lower == "yes" || lower == "on" || lower == "1") {
    return true;
  } else if (lower == "false" || lower == "no" || lower == "off" || lower == "0") {
    return false;
  }
  throw std::runtime_error("Invalid boolean value: " + value);
}

// 字符串类型的特化处理（直接返回原值）
template <>
inline std::string convert<std::string>(const std::string &value) {
  return value;
}

// 配置映射模板类
template <typename T>
class ConfigMap {
 public:
  void Set(const std::string &key, const T &value) { data_[key] = value; }

  const T &Get(const std::string &key) const {
    auto it = data_.find(key);
    if (it == data_.end()) {
      throw std::runtime_error("Key not found: " + key);
    }
    return it->second;
  }

  T GetOrDefault(const std::string &key, T default_value) const {
    auto it = data_.find(key);
    if (it == data_.end()) {
      return default_value;
    }
    return it->second;
  }

  bool Contains(const std::string &key) const { return data_.find(key) != data_.end(); }

 private:
  std::map<std::string, T> data_;
};

// 配置管理类
class Config {
 public:
  // 从文件加载配置
  static void LoadFromFile(const std::string &filename) {
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

  // 获取配置值（无默认值）
  template <typename T>
  static T Get(const std::string &key) {
    return convert<T>(global_map_.Get(key));
  }

  // 获取配置值（带默认值）
  template <typename T>
  static T Get(const std::string &key, T default_value) {
    try {
      return convert<T>(global_map_.Get(key));
    } catch (...) {
      return default_value;
    }
  }

  // 清空配置
  static void Clear() { global_map_ = ConfigMap<std::string>(); }

 private:
  static ConfigMap<std::string> global_map_;  // 全局配置映射
};

}  // namespace tirpc