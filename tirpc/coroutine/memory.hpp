#pragma once

#include <atomic>
#include <memory>
#include <vector>

#include "tirpc/common/lock_free_queue.hpp"
#include "tirpc/common/mutex.hpp"

namespace tirpc {

class Memory {
 public:
  using ptr = std::shared_ptr<Memory>;

  Memory(int block_size, int block_count);

  ~Memory();

  auto GetRefCount() -> int;

  auto GetStart() -> char *;

  auto GetEnd() -> char *;

  auto GetBlock() -> char *;

  void BackBlock(char *block);

  auto HasBlock(char *block) -> bool;

 private:
  int block_size_{0};
  int block_count_{0};

  int size_{0};
  char *start_{nullptr};
  char *end_{nullptr};

  std::atomic<int> ref_count_{0};
  std::vector<bool> blocks_;

  Mutex mutex_;

  LockFreeQueue<char *, 1024> available_blocks_;
};

}  // namespace tirpc
