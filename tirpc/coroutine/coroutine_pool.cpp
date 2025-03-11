#include "tirpc/coroutine/coroutine_pool.hpp"

#include <sys/mman.h>
#include <vector>

#include "tirpc/common/config.hpp"
#include "tirpc/common/log.hpp"
#include "tirpc/common/mutex.hpp"
#include "tirpc/coroutine/coroutine.hpp"

namespace tirpc {

static ConfigVar<int>::ptr g_cor_stack_size = Config::Lookup("coroutine.coroutine_stack_size", 256);
static ConfigVar<int>::ptr g_cor_pool_size = Config::Lookup("coroutine.coroutine_pool_size", 1000);

static CoroutinePool *t_coroutine_container_ptr = nullptr;

auto GetCoroutinePool() -> CoroutinePool * {
  if (t_coroutine_container_ptr == nullptr) {
    t_coroutine_container_ptr = new CoroutinePool(g_cor_pool_size->GetValue(), g_cor_stack_size->GetValue() * 1024);
  }
  return t_coroutine_container_ptr;
}

CoroutinePool::CoroutinePool(int pool_size, int stack_size /*= 1024 * 128 B*/)
    : pool_size_(pool_size), stack_size_(stack_size) {
  // set main coroutine first
  Coroutine::GetCurrentCoroutine();

  memory_pool_.push_back(std::make_shared<Memory>(stack_size_, pool_size_));

  Memory::ptr tmp = memory_pool_[0];

  for (int i = 0; i < pool_size_; ++i) {
    Coroutine::ptr cor = std::make_shared<Coroutine>(stack_size_, tmp->GetBlock());
    cor->SetIndex(i);
    free_cors_.emplace_back(cor, false);
  }
}

CoroutinePool::~CoroutinePool() = default;

auto CoroutinePool::GetCoroutineInstanse() -> Coroutine::ptr {
  // from 0 to find first free coroutine which: 1. it.second = false, 2. getIsInCoFunc() is false
  // try our best to reuse used corroutine, and try our best not to choose unused coroutine
  // beacuse used couroutine which used has already write bytes into physical memory,
  // but unused coroutine no physical memory yet. we just call mmap get virtual address, but not write yet.
  // so linux will alloc physical when we realy write, that casuse page fault interrupt

  Mutex::Locker lock(mutex_);
  for (int i = 0; i < pool_size_; ++i) {
    if (!free_cors_[i].first->GetIsInCoFunc() && !free_cors_[i].second) {
      free_cors_[i].second = true;
      Coroutine::ptr cor = free_cors_[i].first;
      lock.Unlock();
      return cor;
    }
  }

  for (size_t i = 1; i < memory_pool_.size(); ++i) {
    char *tmp = memory_pool_[i]->GetBlock();
    if (tmp != nullptr) {
      Coroutine::ptr cor = std::make_shared<Coroutine>(stack_size_, tmp);
      return cor;
    }
  }
  memory_pool_.push_back(std::make_shared<Memory>(stack_size_, pool_size_));
  return std::make_shared<Coroutine>(stack_size_, memory_pool_[memory_pool_.size() - 1]->GetBlock());
}

void CoroutinePool::ReturnCoroutine(Coroutine::ptr cor) {
  int i = cor->GetIndex();
  if (i >= 0 && i < pool_size_) {
    free_cors_[i].second = false;
  } else {
    for (size_t i = 1; i < memory_pool_.size(); ++i) {
      if (memory_pool_[i]->HasBlock(cor->GetStackPtr())) {
        memory_pool_[i]->BackBlock(cor->GetStackPtr());
      }
    }
  }
}

}  // namespace tirpc