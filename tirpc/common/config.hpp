#pragma once

#include <cxxabi.h>  // for abi::__cxa_demangle()
#include <stdint.h>
#include <sys/time.h>
#include <sys/types.h>
#include <tinyxml2.h>
#include <yaml-cpp/yaml.h>
#include <algorithm>
#include <cctype>
#include <exception>
#include <functional>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "tirpc/common/const.hpp"
#include "tirpc/common/log.hpp"
#include "tirpc/common/mutex.hpp"
#include "tirpc/net/base/address.hpp"

#ifdef DECLARE_MYSQL_PLUGIN
#include "tirpc/common/mysql_instance.hpp"
#endif

namespace tirpc {

class ConfigVarBase {
 public:
  using ptr = std::shared_ptr<ConfigVarBase>;

  ConfigVarBase(const std::string &name, const std::string &desc = "") : name_(name), desc_(desc) {
    std::transform(name_.begin(), name_.end(), name_.begin(), ::tolower);
  }

  virtual ~ConfigVarBase() = default;

  auto GetName() const -> const std::string & { return name_; }

  auto GetDesc() const -> const std::string & { return desc_; }

  virtual auto ToString() -> std::string = 0;

  virtual auto FromString(const std::string &val) -> bool = 0;

  virtual auto GetTypeName() const -> std::string = 0;

 protected:
  std::string name_;
  std::string desc_;
};

template <class F, class T>
class LexicalCast {
 public:
  // FIXIT
  T operator()(const F &v) { return T(); }
};

/**
 * @brief 类型转换模板类片特化(YAML String 转换成 std::vector<T>)
 */
template <class T>
class LexicalCast<std::string, std::vector<T>> {
 public:
  std::vector<T> operator()(const std::string &v) {
    YAML::Node node = YAML::Load(v);
    typename std::vector<T> vec;
    std::stringstream ss;
    for (size_t i = 0; i < node.size(); ++i) {
      ss.str("");
      ss << node[i];
      vec.push_back(LexicalCast<std::string, T>()(ss.str()));
    }
    return vec;
  }
};

/**
 * @brief 类型转换模板类片特化(std::vector<T> 转换成 YAML String)
 */
template <class T>
class LexicalCast<std::vector<T>, std::string> {
 public:
  std::string operator()(const std::vector<T> &v) {
    YAML::Node node(YAML::NodeType::Sequence);
    for (auto &i : v) {
      node.push_back(YAML::Load(LexicalCast<T, std::string>()(i)));
    }
    std::stringstream ss;
    ss << node;
    return ss.str();
  }
};

/**
 * @brief 类型转换模板类片特化(YAML String 转换成 std::list<T>)
 */
template <class T>
class LexicalCast<std::string, std::list<T>> {
 public:
  std::list<T> operator()(const std::string &v) {
    YAML::Node node = YAML::Load(v);
    typename std::list<T> vec;
    std::stringstream ss;
    for (size_t i = 0; i < node.size(); ++i) {
      ss.str("");
      ss << node[i];
      vec.push_back(LexicalCast<std::string, T>()(ss.str()));
    }
    return vec;
  }
};

/**
 * @brief 类型转换模板类片特化(std::list<T> 转换成 YAML String)
 */
template <class T>
class LexicalCast<std::list<T>, std::string> {
 public:
  std::string operator()(const std::list<T> &v) {
    YAML::Node node(YAML::NodeType::Sequence);
    for (auto &i : v) {
      node.push_back(YAML::Load(LexicalCast<T, std::string>()(i)));
    }
    std::stringstream ss;
    ss << node;
    return ss.str();
  }
};

/**
 * @brief 类型转换模板类片特化(YAML String 转换成 std::set<T>)
 */
template <class T>
class LexicalCast<std::string, std::set<T>> {
 public:
  std::set<T> operator()(const std::string &v) {
    YAML::Node node = YAML::Load(v);
    typename std::set<T> vec;
    std::stringstream ss;
    for (size_t i = 0; i < node.size(); ++i) {
      ss.str("");
      ss << node[i];
      vec.insert(LexicalCast<std::string, T>()(ss.str()));
    }
    return vec;
  }
};

/**
 * @brief 类型转换模板类片特化(std::set<T> 转换成 YAML String)
 */
template <class T>
class LexicalCast<std::set<T>, std::string> {
 public:
  std::string operator()(const std::set<T> &v) {
    YAML::Node node(YAML::NodeType::Sequence);
    for (auto &i : v) {
      node.push_back(YAML::Load(LexicalCast<T, std::string>()(i)));
    }
    std::stringstream ss;
    ss << node;
    return ss.str();
  }
};

/**
 * @brief 类型转换模板类片特化(YAML String 转换成 std::unordered_set<T>)
 */
template <class T>
class LexicalCast<std::string, std::unordered_set<T>> {
 public:
  std::unordered_set<T> operator()(const std::string &v) {
    YAML::Node node = YAML::Load(v);
    typename std::unordered_set<T> vec;
    std::stringstream ss;
    for (size_t i = 0; i < node.size(); ++i) {
      ss.str("");
      ss << node[i];
      vec.insert(LexicalCast<std::string, T>()(ss.str()));
    }
    return vec;
  }
};

/**
 * @brief 类型转换模板类片特化(std::unordered_set<T> 转换成 YAML String)
 */
template <class T>
class LexicalCast<std::unordered_set<T>, std::string> {
 public:
  std::string operator()(const std::unordered_set<T> &v) {
    YAML::Node node(YAML::NodeType::Sequence);
    for (auto &i : v) {
      node.push_back(YAML::Load(LexicalCast<T, std::string>()(i)));
    }
    std::stringstream ss;
    ss << node;
    return ss.str();
  }
};

/**
 * @brief 类型转换模板类片特化(YAML String 转换成 std::map<std::string, T>)
 */
template <class T>
class LexicalCast<std::string, std::map<std::string, T>> {
 public:
  std::map<std::string, T> operator()(const std::string &v) {
    YAML::Node node = YAML::Load(v);
    typename std::map<std::string, T> vec;
    std::stringstream ss;
    for (auto it = node.begin(); it != node.end(); ++it) {
      ss.str("");
      ss << it->second;
      vec.insert(std::make_pair(it->first.Scalar(), LexicalCast<std::string, T>()(ss.str())));
    }
    return vec;
  }
};

/**
 * @brief 类型转换模板类片特化(std::map<std::string, T> 转换成 YAML String)
 */
template <class T>
class LexicalCast<std::map<std::string, T>, std::string> {
 public:
  std::string operator()(const std::map<std::string, T> &v) {
    YAML::Node node(YAML::NodeType::Map);
    for (auto &i : v) {
      node[i.first] = YAML::Load(LexicalCast<T, std::string>()(i.second));
    }
    std::stringstream ss;
    ss << node;
    return ss.str();
  }
};

/**
 * @brief 类型转换模板类片特化(YAML String 转换成 std::unordered_map<std::string, T>)
 */
template <class T>
class LexicalCast<std::string, std::unordered_map<std::string, T>> {
 public:
  std::unordered_map<std::string, T> operator()(const std::string &v) {
    YAML::Node node = YAML::Load(v);
    typename std::unordered_map<std::string, T> vec;
    std::stringstream ss;
    for (auto it = node.begin(); it != node.end(); ++it) {
      ss.str("");
      ss << it->second;
      vec.insert(std::make_pair(it->first.Scalar(), LexicalCast<std::string, T>()(ss.str())));
    }
    return vec;
  }
};

/**
 * @brief 类型转换模板类片特化(std::unordered_map<std::string, T> 转换成 YAML String)
 */
template <class T>
class LexicalCast<std::unordered_map<std::string, T>, std::string> {
 public:
  std::string operator()(const std::unordered_map<std::string, T> &v) {
    YAML::Node node(YAML::NodeType::Map);
    for (auto &i : v) {
      node[i.first] = YAML::Load(LexicalCast<T, std::string>()(i.second));
    }
    std::stringstream ss;
    ss << node;
    return ss.str();
  }
};

/**
 * @brief 获取T类型的类型字符串
 */
template <class T>
const char *TypeToName() {
  static const char *s_name = abi::__cxa_demangle(typeid(T).name(), nullptr, nullptr, nullptr);
  return s_name;
}

/**
 * @brief 配置参数模板子类,保存对应类型的参数值
 * @details T 参数的具体类型
 *          FromStr 从std::string转换成T类型的仿函数
 *          ToStr 从T转换成std::string的仿函数
 *          std::string 为YAML格式的字符串
 */
template <class T, class FromStr = LexicalCast<std::string, T>, class ToStr = LexicalCast<T, std::string>>
class ConfigVar : public ConfigVarBase {
 public:
  typedef RWMutex RWMutexType;
  typedef std::shared_ptr<ConfigVar> ptr;
  typedef std::function<void(const T &old_value, const T &new_value)> on_change_cb;

  /**
   * @brief 通过参数名,参数值,描述构造ConfigVar
   * @param[in] name 参数名称有效字符为[0-9a-z_.]
   * @param[in] default_value 参数的默认值
   * @param[in] description 参数的描述
   */
  ConfigVar(const std::string &name, const T &default_value, const std::string &description = "")
      : ConfigVarBase(name, description), val_(default_value) {}

  /**
   * @brief 将参数值转换成YAML String
   * @exception 当转换失败抛出异常
   */
  std::string ToString() override {
    try {
      // return boost::lexical_cast<std::string>(val_);
      RWMutexType::ReadLocker lock(mutex_);
      return ToStr()(val_);
    } catch (std::exception &e) {
      LOG_ERROR << "ConfigVar::toString exception " << e.what() << " convert: " << TypeToName<T>() << " to string"
                << " name=" << name_;
    }
    return "";
  }

  /**
   * @brief 从YAML String 转成参数的值
   * @exception 当转换失败抛出异常
   */
  bool FromString(const std::string &val) override {
    try {
      SetValue(FromStr()(val));
    } catch (std::exception &e) {
      LOG_ERROR << "ConfigVar::fromString exception " << e.what() << " convert: string to " << TypeToName<T>()
                << " name=" << name_ << " - " << val;
    }
    return false;
  }

  /**
   * @brief 获取当前参数的值
   */
  const T &GetValue() {
    RWMutexType::ReadLocker lock(mutex_);
    return val_;
  }

  /**
   * @brief 设置当前参数的值
   * @details 如果参数的值有发生变化,则通知对应的注册回调函数
   */
  void SetValue(const T &v) {
    {
      RWMutexType::ReadLocker lock(mutex_);
      if (v == val_) {
        return;
      }
      for (auto &i : cbs_) {
        i.second(val_, v);
      }
    }
    RWMutexType::WriteLocker lock(mutex_);
    val_ = v;
  }

  /**
   * @brief 返回参数值的类型名称(typeinfo)
   */
  std::string GetTypeName() const override { return TypeToName<T>(); }

  /**
   * @brief 添加变化回调函数
   * @return 返回该回调函数对应的唯一id,用于删除回调
   */
  uint64_t addListener(on_change_cb cb) {
    static uint64_t s_fun_id = 0;
    RWMutexType::WriteLocker lock(mutex_);
    ++s_fun_id;
    cbs_[s_fun_id] = cb;
    return s_fun_id;
  }

  /**
   * @brief 删除回调函数
   * @param[in] key 回调函数的唯一id
   */
  void delListener(uint64_t key) {
    RWMutexType::WriteLocker lock(mutex_);
    cbs_.erase(key);
  }

  /**
   * @brief 获取回调函数
   * @param[in] key 回调函数的唯一id
   * @return 如果存在返回对应的回调函数,否则返回nullptr
   */
  on_change_cb GetListener(uint64_t key) {
    RWMutexType::ReadLocker lock(mutex_);
    auto it = cbs_.find(key);
    return it == cbs_.end() ? nullptr : it->second;
  }

  /**
   * @brief 清理所有的回调函数
   */
  void ClearListener() {
    RWMutexType::WriteLocker lock(mutex_);
    cbs_.clear();
  }

 private:
  RWMutexType mutex_;
  T val_;
  // 变更回调函数组, uint64_t key,要求唯一，一般可以用hash
  std::map<uint64_t, on_change_cb> cbs_;
};

/**
 * @brief ConfigVar的管理类
 * @details 提供便捷的方法创建/访问ConfigVar
 */
class Config {
 public:
  typedef std::unordered_map<std::string, ConfigVarBase::ptr> ConfigVarMap;
  typedef RWMutex RWMutexType;

  /**
   * @brief 获取/创建对应参数名的配置参数
   * @param[in] name 配置参数名称
   * @param[in] default_value 参数默认值
   * @param[in] description 参数描述
   * @details 获取参数名为name的配置参数,如果存在直接返回
   *          如果不存在,创建参数配置并用default_value赋值
   * @return 返回对应的配置参数,如果参数名存在但是类型不匹配则返回nullptr
   * @exception 如果参数名包含非法字符[^0-9a-z_.] 抛出异常 std::invalid_argument
   */
  template <class T>
  static typename ConfigVar<T>::ptr Lookup(const std::string &name, const T &default_value,
                                           const std::string &description = "") {
    RWMutexType::WriteLocker lock(GetMutex());
    auto it = GetDatas().find(name);
    if (it != GetDatas().end()) {
      auto tmp = std::dynamic_pointer_cast<ConfigVar<T>>(it->second);
      if (tmp) {
        LOG_INFO << "Lookup name=" << name << " exists";
        return tmp;
      } else {
        LOG_ERROR << "Lookup name=" << name << " exists but type not " << TypeToName<T>()
                  << " real_type=" << it->second->GetTypeName() << " " << it->second->ToString();
        return nullptr;
      }
    }

    if (name.find_first_not_of("abcdefghikjlmnopqrstuvwxyz._012345678") != std::string::npos) {
      LOG_ERROR << "Lookup name invalid " << name;
      throw std::invalid_argument(name);
    }

    typename ConfigVar<T>::ptr v(new ConfigVar<T>(name, default_value, description));
    GetDatas()[name] = v;
    return v;
  }

  /**
   * @brief 查找配置参数
   * @param[in] name 配置参数名称
   * @return 返回配置参数名为name的配置参数
   */
  template <class T>
  static typename ConfigVar<T>::ptr Lookup(const std::string &name) {
    RWMutexType::ReadLocker lock(GetMutex());
    auto it = GetDatas().find(name);
    if (it == GetDatas().end()) {
      return nullptr;
    }
    return std::dynamic_pointer_cast<ConfigVar<T>>(it->second);
  }

  /**
   * @brief 使用YAML::Node初始化配置模块
   */
  static void LoadFromYaml(const YAML::Node &root);

  /**
   * @brief 加载path文件夹里面的配置文件
   */
  static void LoadFromFile(const std::string &filepath);

  /**
   * @brief 查找配置参数,返回配置参数的基类
   * @param[in] name 配置参数名称
   */
  static ConfigVarBase::ptr LookupBase(const std::string &name);

  /**
   * @brief 遍历配置模块里面所有配置项
   * @param[in] cb 配置项回调函数
   */
  static void Visit(std::function<void(ConfigVarBase::ptr)> cb);

 private:
  /**
   * @brief 返回所有的配置项
   */
  static ConfigVarMap &GetDatas() {
    static ConfigVarMap s_datas;
    return s_datas;
  }

  /**
   * @brief 配置项的RWMutex
   */
  static RWMutexType &GetMutex() {
    static RWMutexType s_mutex;
    return s_mutex;
  }
};

// class Config {
//  public:
//   using ptr = std::shared_ptr<Config>;

//   explicit Config(const std::string &file_path);

//   ~Config();

//   void ReadConf();

//   void ReadLogConfig(const YAML::Node &node);

//   auto GetAddr() -> IPAddress::ptr { return addr_; }

//  public:
//   // log params
//   std::string log_path_;
//   std::string log_prefix_;
//   int log_max_size_{0};
//   LogLevel level_{LogLevel::DEBUG};
//   LogLevel app_log_level_{LogLevel::DEBUG};
//   int log_sync_interval_{500};
//   bool log_to_console_{true};

//   // coroutine params
//   int cor_stack_size_{0};
//   int cor_pool_size_{0};

//   int msg_req_len_{0};

//   int max_connect_timeout_{0};
//   int iothread_num_{0};

//   int timewheel_bucket_num_{0};
//   int timewheel_interval_{0};

//   bool use_look_free_{false};

// #ifdef DECLARE_MYSQL_PLUGIN
//   std::map<std::string, MySQLOption> mysql_options_;
// #endif

//   IPAddress::ptr addr_;

//   ServiceRegisterCategory service_register_;

//   /// Zookeeper
//   std::string zk_ip_{"127.0.0.1"};
//   int zk_port_{2181};
//   int zk_timeout_{30000};

//  private:
//   std::string file_path_;

//   YAML::Node yaml_;
// };

}  // namespace tirpc