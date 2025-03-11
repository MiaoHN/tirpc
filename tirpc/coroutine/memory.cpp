#include "tirpc/coroutine/memory.hpp"

#include <cassert>

#include "tirpc/common/config.hpp"
#include "tirpc/common/log.hpp"

namespace tirpc {

static ConfigVar<bool>::ptr g_use_lock_free = Config::Lookup("use_lock_free", false, "wheather to use lock free queue");

Memory::Memory(int block_size, int block_count) : block_size_(block_size), block_count_(block_count) {
  size_ = block_size_ * block_count_;
  start_ = static_cast<char *>(malloc(size_));

  assert(start_ != nullptr);
  LOG_DEBUG << "succ mmap " << size_ << " bytes memory";

  end_ = start_ + size_ - 1;

  blocks_.resize(block_count_);

  for (size_t i = 0; i < blocks_.size(); ++i) {
    blocks_[i] = false;
  }

  ref_count_ = 0;
}

Memory::~Memory() {
  if (start_ == nullptr) {
    return;
  }
  free(start_);
  LOG_INFO << "~succ free mumap " << size_ << " bytes memory";
  start_ = nullptr;
  ref_count_ = 0;
}

auto Memory::GetRefCount() -> int { return ref_count_; }

auto Memory::GetStart() -> char * { return start_; }

auto Memory::GetEnd() -> char * { return end_; }

auto Memory::GetBlock() -> char * {
  int t = -1;
  Mutex::Locker lock(mutex_);
  for (size_t i = 0; i < blocks_.size(); ++i) {
    if (!blocks_[i]) {
      blocks_[i] = true;
      t = i;
      break;
    }
  }
  lock.Unlock();
  if (t == -1) {
    return nullptr;
  }
  ref_count_++;
  return start_ + (t * block_size_);
}

void Memory::BackBlock(char *block) {
  if (block > end_ || block < start_) {
    LOG_ERROR << "error, this block is not belong to this Memory";
    return;
  }

  int t = (block - start_) / block_size_;
  Mutex::Locker lock(mutex_);
  blocks_[t] = false;
  lock.Unlock();

  ref_count_--;
}

auto Memory::HasBlock(char *block) -> bool { return ((block >= start_) && (block <= end_)); }

}  // namespace tirpc