#pragma once

#include <pthread.h>
#include <semaphore.h>
#include <sys/syscall.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include <cstdlib>
#include <ctime>
#include <memory>
#include <queue>
#include <sstream>
#include <string>
#include <vector>

#include "tirpc/common/const.hpp"
#include "tirpc/common/mutex.hpp"

namespace tirpc {

template <typename... Args>
auto FormatString(const char *fmt, Args... args) -> std::string {
  int size = snprintf(nullptr, 0, fmt, args...);

  std::string result;
  if (size > 0) {
    result.resize(size);
    snprintf(const_cast<char *>(result.data()), size, fmt, args...);
  }
  return result;
}

#define LOG_LEVEL(level)                                                                                    \
  if (tirpc::OpenLog() && tirpc::LogLevel::level >= tirpc::GetRpcLogLevel())                                \
  tirpc::LogWrapper(std::make_shared<tirpc::LogEvent>(tirpc::LogLevel::level, __FILE__, __LINE__, __func__, \
                                                      tirpc::LogType::RPC_LOG))                             \
      .GetSS()

#define APP_LOG_LEVEL(level, str, ...)                                                                              \
  if (tirpc::OpenLog() && tirpc::LogLevel::level >= tirpc::GetAppLogLevel())                                        \
    tirpc::Logger::GetLogger()->PushAppLog(                                                                         \
        tirpc::LogEvent(tirpc::LogLevel::level, __FILE__, __LINE__, __func__, tirpc::LogType::APP_LOG).ToString() + \
        "[" + std::string(__FILE__) + ":" + std::to_string(__LINE__) + "]\t" +                                      \
        tirpc::FormatString(str, ##__VA_ARGS__) + "\n");

#define LOG_DEBUG LOG_LEVEL(DEBUG)
#define LOG_INFO LOG_LEVEL(INFO)
#define LOG_WARN LOG_LEVEL(WARN)
#define LOG_ERROR LOG_LEVEL(ERROR)

#define APP_LOG_DEBUG(str, ...) APP_LOG_LEVEL(DEBUG, str, ##__VA_ARGS__)
#define APP_LOG_INFO(str, ...) APP_LOG_LEVEL(INFO, str, ##__VA_ARGS__)
#define APP_LOG_WARN(str, ...) APP_LOG_LEVEL(WARN, str, ##__VA_ARGS__)
#define APP_LOG_ERROR(str, ...) APP_LOG_LEVEL(ERROR, str, ##__VA_ARGS__)

enum LogType { RPC_LOG = 1, APP_LOG = 2 };

auto GetTid() -> pid_t;

auto StringToLevel(const std::string &str) -> LogLevel;

auto LevelToString(LogLevel level) -> std::string;

auto OpenLog() -> bool;

auto GetRpcLogLevel() -> LogLevel;
auto GetAppLogLevel() -> LogLevel;

class LogEvent {
 public:
  using ptr = std::shared_ptr<LogEvent>;

  LogEvent(LogLevel level, const char *file_name, int line, const char *func_name, LogType type);

  ~LogEvent();

  auto GetSS() -> std::stringstream &;

  auto ToString() -> std::string;

  void Log();

 private:
  timeval timeval_;
  LogLevel level_;
  pid_t pid_;
  pid_t tid_;
  int cor_id_;

  const char *file_name_;
  int line_;
  const char *func_name_;
  LogType type_;
  std::string msg_no_;
  std::stringstream ss_;
};

class LogWrapper {
 public:
  explicit LogWrapper(LogEvent::ptr event) : event_(std::move(event)) {}

  ~LogWrapper() { event_->Log(); }

  auto GetSS() -> std::stringstream & { return event_->GetSS(); }

 private:
  LogEvent::ptr event_;
};

class AsyncLogger {
 public:
  using ptr = std::shared_ptr<AsyncLogger>;

  AsyncLogger(const char *file_name, const char *file_path, int max_size, LogType type);

  ~AsyncLogger();

  void Push(std::vector<std::string> &buffer);

  void Flush();

  static auto Execute(void *arg) -> void *;

  void Stop();

 public:
  std::queue<std::vector<std::string>> tasks_;

 private:
  const char *file_name_;
  const char *file_path_;
  int max_size_{0};
  LogType type_;
  pthread_t tid_;
  int no_{0};
  bool need_reopen_{false};
  FILE *file_handler_{nullptr};
  std::string date_;

  Mutex mutex_;
  pthread_cond_t condition_;
  bool stop_{false};

 public:
  pthread_t thread_;
  sem_t semaphore_;
};

class Logger {
 public:
  static auto GetLogger() -> Logger *;

 public:
  using ptr = std::shared_ptr<Logger>;

  Logger();

  ~Logger();

  void Init(const char *file_name, const char *file_path, int max_size, int sync_interval);

  void PushRpcLog(const std::string &msg);

  void PushAppLog(const std::string &msg);

  void LoopFunc();

  void Flush();

  void Start();

  auto GetAsyncRpcLogger() -> AsyncLogger::ptr { return rpc_logger_; }

  auto GetAsyncAppLogger() -> AsyncLogger::ptr { return app_logger_; }

 public:
  std::vector<std::string> rpc_buffer_;
  std::vector<std::string> app_buffer_;

 private:
  Mutex rpc_mutex_;
  Mutex app_mutex_;

  bool is_init_{false};

  AsyncLogger::ptr rpc_logger_;
  AsyncLogger::ptr app_logger_;

  int sync_interval_{0};
};

void Exit(int status);

}  // namespace tirpc
