#pragma once

#include <vector>

#include "tirpc/common/mutex.hpp"
#include "tirpc/coroutine/coroutine.hpp"
#include "tirpc/coroutine/memory.hpp"

namespace tirpc {

class CoroutinePool {
 public:
  explicit CoroutinePool(int pool_size, int stack_size = 1024 * 128);

  ~CoroutinePool();

  auto GetCoroutineInstanse() -> Coroutine::ptr;

  void ReturnCoroutine(Coroutine::ptr &cor);

 private:
  int pool_size_{0};
  int stack_size_{0};

  // first--ptr of cor
  // second
  //    false -- can be dispatched
  //    true -- can't be dispatched
  std::vector<std::pair<Coroutine::ptr, bool>> free_cors_;

  Mutex mutex_;

  std::vector<Memory::ptr> memory_pool_;
};

auto GetCoroutinePool() -> CoroutinePool *;

}  // namespace tirpc