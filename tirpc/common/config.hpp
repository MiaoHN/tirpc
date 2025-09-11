#pragma once

#include <algorithm>
#include <cctype>
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

class Config {
 public:
  static void LoadFromFile(const std::string &filename);

  template <typename T>
  static T Get(const std::string &key) {
    return convert<T>(global_map_.Get(key));
  }

  template <typename T>
  static T Get(const std::string &key, T default_value) {
    try {
      return convert<T>(global_map_.Get(key));
    } catch (...) {
      return default_value;
    }
  }

  static void Clear();

 private:
  static ConfigMap<std::string> global_map_;
};

}  // namespace tirpc