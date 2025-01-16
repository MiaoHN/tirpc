#pragma once

#include <pthread.h>
#include <functional>
#include <queue>
#include <vector>

#include "tirpc/common/mutex.hpp"

namespace tirpc {

class ThreadPool {
 public:
  explicit ThreadPool(int size);

  ~ThreadPool();

  void Start();

  void Stop();

  void AddTask(const std::function<void()> &task);

 private:
  static auto MainFunction(void *ptr) -> void *;

 public:
  int size_{0};
  std::vector<pthread_t> threads_;
  std::queue<std::function<void()>> tasks_;

  Mutex mutex_;
  pthread_cond_t condition_;
  bool stop_{false};
};

}  // namespace tirpc