#include "tirpc/net/tcp/io_thread.hpp"

#include <semaphore.h>
#include <cstdlib>
#include <ctime>
#include <map>
#include <memory>
#include <utility>

#include "tirpc/common/config.hpp"
#include "tirpc/coroutine/coroutine.hpp"
#include "tirpc/coroutine/coroutine_pool.hpp"
#include "tirpc/net/base/reactor.hpp"
#include "tirpc/net/tcp/tcp_connection.hpp"
#include "tirpc/net/tcp/tcp_connection_time_wheel.hpp"
#include "tirpc/net/tcp/tcp_server.hpp"

namespace tirpc {

static thread_local Reactor *t_reactor_ptr = nullptr;

static thread_local IOThread *t_cur_io_thread = nullptr;

IOThread::IOThread() {
  int rt = sem_init(&init_semaphore_, 0, 0);
  assert(rt == 0);

  rt = sem_init(&start_semaphore_, 0, 0);
  assert(rt == 0);

  pthread_create(&thread_, nullptr, &IOThread::Main, this);

  LOG_DEBUG << "semaphore begin to wait until new thread frinish IOThread::main() to init";
  // wait until new thread finish IOThread::main() func to init
  rt = sem_wait(&init_semaphore_);
  assert(rt == 0);
  LOG_DEBUG << "semaphore wait end, finish create io thread";

  sem_destroy(&init_semaphore_);
}

IOThread::~IOThread() {
  reactor_->Stop();
  pthread_join(thread_, nullptr);

  if (reactor_ != nullptr) {
    delete reactor_;
    reactor_ = nullptr;
  }
}

auto IOThread::GetCurrentIOThread() -> IOThread * { return t_cur_io_thread; }

auto IOThread::GetStartSemaphore() -> sem_t * { return &start_semaphore_; }

auto IOThread::GetReactor() -> Reactor * { return reactor_; }

auto IOThread::GetPthreadId() -> pthread_t { return thread_; }

void IOThread::SetThreadIndex(int index) {
  index_ = index;
  reactor_->SetThreadIndex(index);
}

auto IOThread::GetThreadIndex() -> int { return index_; }

auto IOThread::Main(void *arg) -> void * {
  // assert(t_reactor_ptr == nullptr);

  t_reactor_ptr = new Reactor();
  assert(t_reactor_ptr != nullptr);

  auto *thread = static_cast<IOThread *>(arg);
  t_cur_io_thread = thread;
  thread->reactor_ = t_reactor_ptr;
  thread->reactor_->SetReactorType(SubReactor);
  thread->tid_ = GetTid();

  Coroutine::GetCurrentCoroutine();

  LOG_DEBUG << "finish iothread init, now post semaphore";
  sem_post(&thread->init_semaphore_);

  // wait for main thread post start_semaphore_ to start iothread loop
  sem_wait(&thread->start_semaphore_);

  sem_destroy(&thread->start_semaphore_);

  // IOThreadPool::Start 后执行这条语句，启动本线程的 reactor 循环
  t_reactor_ptr->Loop();

  return nullptr;
}

IOThreadPool::IOThreadPool(int size) : size_(size) {
  io_threads_.resize(size);
  for (int i = 0; i < size; ++i) {
    io_threads_[i] = std::make_shared<IOThread>();
    io_threads_[i]->SetThreadIndex(i);
  }
}

void IOThreadPool::Start() {
  for (int i = 0; i < size_; ++i) {
    int rt = sem_post(io_threads_[i]->GetStartSemaphore());
    assert(rt == 0);
  }
}

auto IOThreadPool::GetIoThread() -> IOThread * {
  if (index_ == size_ || index_ == -1) {
    index_ = 0;
  }
  return io_threads_[index_++].get();
}

auto IOThreadPool::GetIoThreadPoolSize() -> int { return size_; }

void IOThreadPool::BroadcastTask(std::function<void()> cb) {
  for (const auto &i : io_threads_) {
    i->GetReactor()->AddTask(cb, true);
  }
}

void IOThreadPool::AddTaskByIndex(int index, std::function<void()> cb) {
  if (index >= 0 && index < size_) {
    io_threads_[index]->GetReactor()->AddTask(cb, true);
  }
}

void IOThreadPool::AddCoroutineToRandomThread(Coroutine::ptr cor, bool self /* = false*/) {
  if (size_ == 1) {
    io_threads_[0]->GetReactor()->AddCoroutine(cor, true);
    return;
  }
  srand(time(nullptr));
  int i = 0;
  while (true) {
    i = rand() % (size_);
    if (!self && io_threads_[i]->GetPthreadId() == t_cur_io_thread->GetPthreadId()) {
      i++;
      if (i == size_) {
        i -= 2;
      }
    }
    break;
  }
  io_threads_[i]->GetReactor()->AddCoroutine(cor, true);
  // if (io_threads_[index_]->GetPthreadId() == t_cur_io_thread->GetPthreadId()) {
  //   index_++;
  //   if (index_ == size_ || index_ == -1) {
  //     index_ = 0;
  //   }
  // }
}

auto IOThreadPool::AddCoroutineToRandomThread(std::function<void()> cb, bool self /* = false*/) -> Coroutine::ptr {
  Coroutine::ptr cor = GetCoroutinePool()->GetCoroutineInstanse();
  cor->SetCallBack(cb);
  AddCoroutineToRandomThread(cor, self);
  return cor;
}

auto IOThreadPool::AddCoroutineToThreadByIndex(int index, std::function<void()> cb,
                                               bool self /* = false*/) -> Coroutine::ptr {
  if (index >= static_cast<int>(io_threads_.size()) || index < 0) {
    LOG_ERROR << "addCoroutineToThreadByIndex error, invalid iothread index[" << index << "]";
    return nullptr;
  }
  Coroutine::ptr cor = GetCoroutinePool()->GetCoroutineInstanse();
  cor->SetCallBack(cb);
  io_threads_[index]->GetReactor()->AddCoroutine(cor, true);
  return cor;
}

void IOThreadPool::AddCoroutineToEachThread(std::function<void()> cb) {
  for (const auto &i : io_threads_) {
    Coroutine::ptr cor = GetCoroutinePool()->GetCoroutineInstanse();
    cor->SetCallBack(cb);
    i->GetReactor()->AddCoroutine(cor, true);
  }
}

}  // namespace tirpc