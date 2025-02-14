#include "tirpc/common/log.hpp"

#include <execinfo.h>
#include <fcntl.h>
#include <semaphore.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include <algorithm>
#include <atomic>
#include <cassert>
#include <cerrno>
#include <csignal>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <iostream>
#include <sstream>

#include "tirpc/net/base/reactor.hpp"
#include "tirpc/net/base/timer.hpp"

namespace tirpc {

extern Logger::ptr g_rpc_logger;

void BackTrace(std::vector<std::string> &bt, int size, int skip) {
  void **array = static_cast<void **>(malloc(sizeof(void *) * size));
  size_t s = ::backtrace(array, size);

  char **strings = backtrace_symbols(array, s);
  if (strings == nullptr) {
    printf("backtrace_symbols return nullptr\n");
    return;
  }

  for (size_t i = skip; i < s; ++i) {
    bt.emplace_back(strings[i]);
  }

  free(strings);
  free(array);
}

auto BacktraceToString(int size = 64, int skip = 2, const std::string &prefix = "") -> std::string {
  std::vector<std::string> bt;
  BackTrace(bt, size, skip);
  std::stringstream ss;
  for (auto &i : bt) {
    ss << prefix << i << std::endl;
  }
  return ss.str();
}

void CoredumpHandler(int signal_no) {
  ErrorLog << "progress receive invalid signal, will exit";
  printf("progress receive invalid signal, will exit\n");

  std::string bt = BacktraceToString();

  ErrorLog << "coredump stack:\n" << bt;

  printf("coredump stack:\n%s\n", bt.c_str());

  g_rpc_logger->Flush();

  pthread_join(g_rpc_logger->GetAsyncRpcLogger()->thread_, nullptr);
  pthread_join(g_rpc_logger->GetAsyncAppLogger()->thread_, nullptr);

  signal(signal_no, SIG_DFL);
  raise(signal_no);
}

class Coroutine;

static thread_local pid_t t_thread_id = 0;

static pid_t g_pid = 0;

auto GetTid() -> pid_t {
  if (t_thread_id == 0) {
    t_thread_id = syscall(SYS_gettid);
  }
  return t_thread_id;
}

auto OpenLog() -> bool { return g_rpc_logger != nullptr; }

LogEvent::LogEvent(LogLevel level, const char *file_name, int line, const char *func_name, LogType type)
    : level_(level), file_name_(file_name), line_(line), func_name_(func_name), type_(type) {}

LogEvent::~LogEvent() = default;

auto LevelToString(LogLevel level) -> std::string {
  std::string re = "DEBUG";
  switch (level) {
    case DEBUG:
      re = "DEBUG";
      return re;

    case INFO:
      re = "INFO";
      return re;

    case WARN:
      re = "WARN";
      return re;

    case ERROR:
      re = "ERROR";
      return re;
    case NONE:
      re = "NONE";

    default:
      return re;
  }
}

auto StringToLevel(const std::string &str) -> LogLevel {
  if (str == "DEBUG") {
    return LogLevel::DEBUG;
  }

  if (str == "INFO") {
    return LogLevel::INFO;
  }

  if (str == "WARN") {
    return LogLevel::WARN;
  }

  if (str == "ERROR") {
    return LogLevel::ERROR;
  }

  if (str == "NONE") {
    return LogLevel::NONE;
  }

  return LogLevel::DEBUG;
}

auto LogTypeToString(LogType logtype) -> std::string {
  switch (logtype) {
    case APP_LOG:
      return "app";
    case RPC_LOG:
      return "rpc";
    default:
      return "";
  }
}

auto LogEvent::GetSS() -> std::stringstream & {
  // time_t now_time = timestamp_;

  gettimeofday(&timeval_, nullptr);

  struct tm time;
  localtime_r(&(timeval_.tv_sec), &time);

  const char *format = "%Y-%m-%d %H:%M:%S";
  char buf[128];
  strftime(buf, sizeof(buf), format, &time);

  ss_ << "[" << buf << "." << timeval_.tv_usec << "]\t";

  std::string s_level = LevelToString(level_);
  ss_ << "[" << s_level << "]\t";

  if (g_pid == 0) {
    g_pid = getpid();
  }
  pid_ = g_pid;

  if (t_thread_id == 0) {
    t_thread_id = GetTid();
  }
  tid_ = t_thread_id;

  cor_id_ = Coroutine::GetCurrentCoroutine()->GetCorId();

  ss_ << "[" << pid_ << "]\t"
      << "[" << tid_ << "]\t"
      << "[" << cor_id_ << "]\t"
      << "[" << file_name_ << ":" << line_ << "]\t";
  // << "[" << func_name_ << "]\t";
  Runtime *runtime = GetCurrentRuntime();
  if (runtime != nullptr) {
    std::string msgno = runtime->msg_no_;
    if (!msgno.empty()) {
      ss_ << "[" << msgno << "]\t";
    }

    std::string interface_name = runtime->interface_name_;
    if (!interface_name.empty()) {
      ss_ << "[" << interface_name << "]\t";
    }
  }
  return ss_;
}

auto LogEvent::ToString() -> std::string { return GetSS().str(); }

void LogEvent::Log() {
  ss_ << "\n";
  if (level_ >= g_rpc_config->level_ && type_ == RPC_LOG) {
    g_rpc_logger->PushRpcLog(ss_.str());
  } else if (level_ >= g_rpc_config->app_log_level_ && type_ == APP_LOG) {
    g_rpc_logger->PushAppLog(ss_.str());
  }
}

// cannot do anything which will call LOG ,otherwise is will coredump
Logger::Logger() = default;

Logger::~Logger() {
  Flush();
  pthread_join(rpc_logger_->thread_, nullptr);
  pthread_join(app_logger_->thread_, nullptr);
}

auto Logger::GetLogger() -> Logger * { return g_rpc_logger.get(); }

void Logger::Init(const char *file_name, const char *file_path, int max_size, int sync_interval) {
  if (!is_init_) {
    sync_interval_ = sync_interval;

    rpc_logger_ = std::make_shared<AsyncLogger>(file_name, file_path, max_size, RPC_LOG);
    app_logger_ = std::make_shared<AsyncLogger>(file_name, file_path, max_size, APP_LOG);

    signal(SIGSEGV, CoredumpHandler);
    signal(SIGABRT, CoredumpHandler);
    signal(SIGTERM, CoredumpHandler);
    signal(SIGKILL, CoredumpHandler);
    signal(SIGINT, CoredumpHandler);
    signal(SIGSTKFLT, CoredumpHandler);

    // ignore SIGPIPE
    signal(SIGPIPE, SIG_IGN);
    is_init_ = true;
  }
}

void Logger::Start() {
  TimerEvent::ptr event = std::make_shared<TimerEvent>(sync_interval_, true, std::bind(&Logger::LoopFunc, this));
  Reactor::GetReactor()->GetTimer()->AddTimerEvent(event);
}

void Logger::LoopFunc() {
  std::vector<std::string> app_tmp;
  Mutex::Locker lock1(app_mutex_);
  app_tmp.swap(app_buffer_);
  lock1.Unlock();

  std::vector<std::string> rpc_tmp = rpc_buffer_;
  Mutex::Locker lock2(rpc_mutex_);
  rpc_tmp.swap(rpc_buffer_);
  lock2.Unlock();

  rpc_logger_->Push(rpc_tmp);
  app_logger_->Push(app_tmp);
}

void Logger::PushRpcLog(const std::string &msg) {
  Mutex::Locker lock(rpc_mutex_);
  rpc_buffer_.push_back(msg);
  lock.Unlock();
}

void Logger::PushAppLog(const std::string &msg) {
  Mutex::Locker lock(app_mutex_);
  app_buffer_.push_back(msg);
  lock.Unlock();
}

void Logger::Flush() {
  LoopFunc();
  rpc_logger_->Stop();
  rpc_logger_->Flush();

  app_logger_->Stop();
  app_logger_->Flush();
}

AsyncLogger::AsyncLogger(const char *file_name, const char *file_path, int max_size, LogType logtype)
    : file_name_(file_name), file_path_(file_path), max_size_(max_size), type_(logtype) {
  int rt = sem_init(&semaphore_, 0, 0);
  assert(rt == 0);

  rt = pthread_create(&thread_, nullptr, &AsyncLogger::Execute, this);
  assert(rt == 0);
  rt = sem_wait(&semaphore_);
  assert(rt == 0);
}

AsyncLogger::~AsyncLogger() = default;

auto AsyncLogger::Execute(void *arg) -> void * {
  auto *ptr = reinterpret_cast<AsyncLogger *>(arg);
  int rt = pthread_cond_init(&ptr->condition_, nullptr);
  assert(rt == 0);

  rt = sem_post(&ptr->semaphore_);
  assert(rt == 0);

  while (true) {
    Mutex::Locker lock(ptr->mutex_);
    while (ptr->tasks_.empty() && !ptr->stop_) {
      pthread_cond_wait(&(ptr->condition_), ptr->mutex_.GetMutex());
    }
    bool is_stop = ptr->stop_;
    if (is_stop && ptr->tasks_.empty()) {
      lock.Unlock();
      break;
    }
    std::vector<std::string> tmp;
    tmp.swap(ptr->tasks_.front());
    ptr->tasks_.pop();
    lock.Unlock();

    timeval now;
    gettimeofday(&now, nullptr);

    struct tm now_time;
    localtime_r(&(now.tv_sec), &now_time);

    const char *format = "%Y%m%d";
    char date[32];
    strftime(date, sizeof(date), format, &now_time);
    if (ptr->date_ != std::string(date)) {
      // cross day
      // reset no_ date_
      ptr->no_ = 0;
      ptr->date_ = std::string(date);
      ptr->need_reopen_ = true;
    }

    if (ptr->file_handler_ == nullptr) {
      ptr->need_reopen_ = true;
    }

    std::stringstream ss;
    ss << ptr->file_path_ << ptr->file_name_ << "_" << ptr->date_ << "_" << LogTypeToString(ptr->type_) << "_"
       << ptr->no_ << ".log";
    std::string full_file_name = ss.str();

    if (ptr->need_reopen_) {
      if (ptr->file_handler_ != nullptr) {
        fclose(ptr->file_handler_);
      }

      ptr->file_handler_ = fopen(full_file_name.c_str(), "a");
      if (ptr->file_handler_ == nullptr) {
        printf("open fail errno = %d reason = %s \n", errno, strerror(errno));
      }
      ptr->need_reopen_ = false;
    }

    if (ftell(ptr->file_handler_) > ptr->max_size_) {
      fclose(ptr->file_handler_);

      // single log file over max size
      ptr->no_++;
      std::stringstream ss2;
      ss2 << ptr->file_path_ << ptr->file_name_ << "_" << ptr->date_ << "_" << LogTypeToString(ptr->type_) << "_"
          << ptr->no_ << ".log";
      full_file_name = ss2.str();

      // printf("open file %s", full_file_name.c_str());
      ptr->file_handler_ = fopen(full_file_name.c_str(), "a");
      ptr->need_reopen_ = false;
    }

    if (ptr->file_handler_ == nullptr) {
      printf("open log file %s error!", full_file_name.c_str());
    }

    for (const auto &i : tmp) {
      if (!i.empty()) {
        // Write to file
        fwrite(i.c_str(), 1, i.length(), ptr->file_handler_);

        // Write to console
        if (g_rpc_config->log_to_console_) {
          // [APP] / [RPC]
          if (ptr->type_ == APP_LOG) {
            printf("[APP] %s", i.c_str());
          } else {
            printf("[RPC] %s", i.c_str());
          }
        }
      }
    }
    tmp.clear();
    fflush(ptr->file_handler_);
    if (is_stop) {
      break;
    }
  }
  if (ptr->file_handler_ != nullptr) {
    fclose(ptr->file_handler_);
    ptr->file_handler_ = nullptr;
  }

  return nullptr;
}

void AsyncLogger::Push(std::vector<std::string> &buffer) {
  if (!buffer.empty()) {
    Mutex::Locker lock(mutex_);
    tasks_.push(buffer);
    lock.Unlock();
    pthread_cond_signal(&condition_);
  }
}

void AsyncLogger::Flush() {
  if (file_handler_ != nullptr) {
    fflush(file_handler_);
  }
}

void AsyncLogger::Stop() {
  if (!stop_) {
    stop_ = true;
    pthread_cond_signal(&condition_);
  }
}

void Exit(int status) {
#ifdef DECLARE_MYSQL_PLUGIN
  mysql_library_end();
#endif

  printf("It's sorry to said we start TinyRPC server error, look up log file to get more deatils!\n");
  g_rpc_logger->Flush();
  pthread_join(g_rpc_logger->GetAsyncRpcLogger()->thread_, nullptr);

  _exit(status);
}

}  // namespace tirpc