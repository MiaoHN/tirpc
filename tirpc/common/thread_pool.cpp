#include "tirpc/common/thread_pool.hpp"

namespace tirpc {

auto ThreadPool::MainFunction(void *ptr) -> void * {
  auto *pool = reinterpret_cast<ThreadPool *>(ptr);
  pthread_cond_init(&pool->condition_, nullptr);

  while (!pool->stop_) {
    std::function<void()> cb;
    {
      Mutex::Locker lock(pool->mutex_);

      while (pool->tasks_.empty()) {
        pthread_cond_wait(&(pool->condition_), pool->mutex_.GetMutex());
      }
      cb = pool->tasks_.front();
      pool->tasks_.pop();
    }

    cb();
  }
  return nullptr;
}

ThreadPool::ThreadPool(int size) : size_(size) {
  for (int i = 0; i < size_; ++i) {
    pthread_t thread;
    threads_.emplace_back(thread);
  }
  pthread_cond_init(&condition_, nullptr);
}

void ThreadPool::Start() {
  for (int i = 0; i < size_; ++i) {
    pthread_create(&threads_[i], nullptr, &ThreadPool::MainFunction, this);
  }
}

void ThreadPool::Stop() { stop_ = true; }

void ThreadPool::AddTask(const std::function<void()> &cb) {
  {
    Mutex::Locker lock(mutex_);
    tasks_.push(cb);
  }
  pthread_cond_signal(&condition_);
}

ThreadPool::~ThreadPool() {
  for (int i = 0; i < size_; ++i) {
    pthread_join(threads_[i], nullptr);
  }
}

}  // namespace tirpc