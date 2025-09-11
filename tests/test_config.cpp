#include <gtest/gtest.h>
#include <cstdio>
#include <fstream>

#include "tirpc/common/config.hpp"

using namespace tirpc;

class ConfigTest : public ::testing::Test {
 protected:
  void SetUp() override {
    // 创建临时配置文件
    temp_filename = "test_config.conf";
  }

  void TearDown() override {
    // 删除临时配置文件
    std::remove(temp_filename.c_str());
    // 清除配置状态
    Config::Clear();
  }

  void CreateConfigFile(const std::string &content) {
    std::ofstream file(temp_filename);
    file << content;
    file.close();
  }

  std::string temp_filename;
};

// 测试基本配置加载
TEST_F(ConfigTest, LoadsBasicConfiguration) {
  CreateConfigFile(R"(
msg_req_len = 20
use_lockfree = 1
max_connect_timeout = 75
iothread_num = 1

[log]
log_path = ./
log_prefix = rpc_server
log_max_file_size = 5
rpc_log_level = WARN
app_log_level = WARN
log_sync_interval = 500
log_to_console = 1
)");

  Config::LoadFromFile(temp_filename);

  // 全局配置
  EXPECT_EQ(Config::Get<int>("msg_req_len"), 20);
  EXPECT_EQ(Config::Get<bool>("use_lockfree"), true);
  EXPECT_EQ(Config::Get<int>("max_connect_timeout"), 75);
  EXPECT_EQ(Config::Get<int>("iothread_num"), 1);

  // log 部分
  EXPECT_EQ(Config::Get<std::string>("log.log_path"), "./");
  EXPECT_EQ(Config::Get<std::string>("log.log_prefix"), "rpc_server");
  EXPECT_EQ(Config::Get<int>("log.log_max_file_size"), 5);
  EXPECT_EQ(Config::Get<std::string>("log.rpc_log_level"), "WARN");
  EXPECT_EQ(Config::Get<std::string>("log.app_log_level"), "WARN");
  EXPECT_EQ(Config::Get<int>("log.log_sync_interval"), 500);
  EXPECT_EQ(Config::Get<bool>("log.log_to_console"), true);
}

// 测试默认值功能
TEST_F(ConfigTest, HandlesDefaultValues) {
  CreateConfigFile("key1 = value1");
  Config::LoadFromFile(temp_filename);

  // 存在的键
  EXPECT_EQ(Config::Get<std::string>("key1", "default"), "value1");

  // 不存在的键
  EXPECT_EQ(Config::Get<int>("nonexistent_int", 42), 42);
  EXPECT_EQ(Config::Get<bool>("nonexistent_bool", true), true);
  EXPECT_EQ(Config::Get<std::string>("nonexistent_string", "default"), "default");
}

// 测试类型转换
TEST_F(ConfigTest, ConvertsTypesCorrectly) {
  CreateConfigFile(R"(
int_val = 42
bool_val1 = true
bool_val2 = 0
bool_val3 = yes
bool_val4 = off
string_val = hello world
)");

  Config::LoadFromFile(temp_filename);

  EXPECT_EQ(Config::Get<int>("int_val"), 42);
  EXPECT_EQ(Config::Get<bool>("bool_val1"), true);
  EXPECT_EQ(Config::Get<bool>("bool_val2"), false);
  EXPECT_EQ(Config::Get<bool>("bool_val3"), true);
  EXPECT_EQ(Config::Get<bool>("bool_val4"), false);
  EXPECT_EQ(Config::Get<std::string>("string_val"), "hello world");
}

// 测试空格处理
TEST_F(ConfigTest, HandlesWhitespace) {
  CreateConfigFile(R"(
  key1  =  value1
[ section ]
  key2  =  value2
)");

  Config::LoadFromFile(temp_filename);

  EXPECT_EQ(Config::Get<std::string>("key1"), "value1");
  EXPECT_EQ(Config::Get<std::string>("section.key2"), "value2");
}

// 测试引号处理
TEST_F(ConfigTest, DISABLED_HandlesQuotes) {
  CreateConfigFile(R"(
str1 = "quoted value"
str2 = 'single quoted'
str3 = "escaped\"quote"
str4 = 'escaped\'quote'
)");

  Config::LoadFromFile(temp_filename);

  EXPECT_EQ(Config::Get<std::string>("str1"), "quoted value");
  EXPECT_EQ(Config::Get<std::string>("str2"), "single quoted");
  EXPECT_EQ(Config::Get<std::string>("str3"), "escaped\"quote");
  EXPECT_EQ(Config::Get<std::string>("str4"), "escaped\'quote");
}

// 测试异常情况
TEST_F(ConfigTest, HandlesExceptions) {
  // 文件不存在
  EXPECT_THROW(Config::LoadFromFile("nonexistent_file.conf"), std::runtime_error);

  // 无效键
  CreateConfigFile("valid_key = value");
  Config::LoadFromFile(temp_filename);
  EXPECT_THROW(Config::Get<std::string>("invalid_key"), std::runtime_error);

  // 类型转换错误
  CreateConfigFile("int_val = not_an_int");
  Config::LoadFromFile(temp_filename);
  EXPECT_THROW(Config::Get<int>("int_val"), std::runtime_error);
}

// 测试多段配置
TEST_F(ConfigTest, HandlesMultipleSections) {
  CreateConfigFile(R"(
global_val = 100

[section1]
key1 = value1

[section2]
key2 = value2
)");

  Config::LoadFromFile(temp_filename);

  EXPECT_EQ(Config::Get<int>("global_val"), 100);
  EXPECT_EQ(Config::Get<std::string>("section1.key1"), "value1");
  EXPECT_EQ(Config::Get<std::string>("section2.key2"), "value2");
}

// 测试注释和空行
TEST_F(ConfigTest, IgnoresCommentsAndEmptyLines) {
  CreateConfigFile(R"(
# This is a comment
; Another comment

key1 = value1

[section]
# Section comment
key2 = value2
)");

  Config::LoadFromFile(temp_filename);

  EXPECT_EQ(Config::Get<std::string>("key1"), "value1");
  EXPECT_EQ(Config::Get<std::string>("section.key2"), "value2");
  EXPECT_THROW(Config::Get<std::string>("#"), std::runtime_error);
}

// 测试布尔值转换边界
TEST_F(ConfigTest, BooleanConversionBoundaries) {
  CreateConfigFile(R"(
bool1 = 1
bool2 = 0
bool3 = true
bool4 = false
bool5 = yes
bool6 = no
bool7 = on
bool8 = off
bool9 = TRUE
bool10 = False
)");

  Config::LoadFromFile(temp_filename);

  EXPECT_TRUE(Config::Get<bool>("bool1"));
  EXPECT_FALSE(Config::Get<bool>("bool2"));
  EXPECT_TRUE(Config::Get<bool>("bool3"));
  EXPECT_FALSE(Config::Get<bool>("bool4"));
  EXPECT_TRUE(Config::Get<bool>("bool5"));
  EXPECT_FALSE(Config::Get<bool>("bool6"));
  EXPECT_TRUE(Config::Get<bool>("bool7"));
  EXPECT_FALSE(Config::Get<bool>("bool8"));
  EXPECT_TRUE(Config::Get<bool>("bool9"));
  EXPECT_FALSE(Config::Get<bool>("bool10"));
}

// 测试清除功能
TEST_F(ConfigTest, ClearFunctionality) {
  CreateConfigFile("key1 = value1");
  Config::LoadFromFile(temp_filename);
  EXPECT_EQ(Config::Get<std::string>("key1"), "value1");

  Config::Clear();
  EXPECT_THROW(Config::Get<std::string>("key1"), std::runtime_error);
}

// 测试键名大小写敏感性
TEST_F(ConfigTest, KeyCaseSensitivity) {
  CreateConfigFile(R"(
Key1 = value1
[Section]
Key2 = value2
)");

  Config::LoadFromFile(temp_filename);

  EXPECT_EQ(Config::Get<std::string>("Key1"), "value1");
  EXPECT_EQ(Config::Get<std::string>("Section.Key2"), "value2");
  EXPECT_THROW(Config::Get<std::string>("key1"), std::runtime_error);
  EXPECT_THROW(Config::Get<std::string>("section.key2"), std::runtime_error);
}

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}