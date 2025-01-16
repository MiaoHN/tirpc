#include "tirpc/coroutine/coroutine.hpp"

#include <atomic>
#include <cassert>
#include <cinttypes>
#include <cstdint>
#include <cstring>
#include <utility>
#include "tirpc/common/log.hpp"
#include "tirpc/coroutine/coctx.hpp"

namespace tirpc {

static thread_local Coroutine *t_main_coroutine = nullptr;

static thread_local Coroutine *t_current_coroutine = nullptr;

static thread_local Runtime *t_current_runtime = nullptr;

static std::atomic_int t_coroutine_count = {0};

static std::atomic_int t_current_coroutine_id = {0};

auto GetCoroutineIndex() -> int { return t_current_coroutine_id.load(); }

auto GetCurrentRuntime() -> Runtime * { return t_current_runtime; }

void SetCurrentRuntime(Runtime *runtime) { t_current_runtime = runtime; }

void CoFunction(Coroutine *co) {
  if (co != nullptr) {
    co->SetIsInCoFunc(true);

    co->callback_();

    co->SetIsInCoFunc(false);
  }

  Coroutine::Yield();
}

Coroutine::Coroutine() {
  cor_id_ = 0;
  t_coroutine_count++;
  memset(&coctx_, 0, sizeof(coctx_));
  t_current_coroutine = this;
}

Coroutine::Coroutine(int size, char *stack_ptr) : stack_size_(size), stack_sp_(stack_ptr) {
  assert(stack_ptr != nullptr);

  if (t_main_coroutine == nullptr) {
    t_main_coroutine = new Coroutine();
  }

  cor_id_ = t_current_coroutine_id++;
  t_coroutine_count++;
}

Coroutine::Coroutine(int size, char *stack_ptr, std::function<void()> cb) : stack_size_(size), stack_sp_(stack_ptr) {
  assert(stack_ptr != nullptr);

  if (t_main_coroutine == nullptr) {
    t_main_coroutine = new Coroutine();
  }

  SetCallBack(std::move(cb));
  cor_id_ = t_current_coroutine_id++;
  t_coroutine_count++;
}

auto Coroutine::SetCallBack(std::function<void()> cb) -> bool {
  if (this == t_main_coroutine) {
    ErrorLog << "main coroutine cannot set callback";
    return false;
  }

  if (is_in_cofunc_) {
    ErrorLog << "this corroutine is in CoFunction";
    return false;
  }

  callback_ = std::move(cb);

  char *top = stack_sp_ + stack_size_;

  top = reinterpret_cast<char *>((reinterpret_cast<uint64_t>(top)) & -16LL);

  memset(&coctx_, 0, sizeof(coctx_));

  coctx_.regs_[kRSP] = top;
  coctx_.regs_[kRBP] = top;
  coctx_.regs_[kRETAddr] = reinterpret_cast<void *>(CoFunction);
  coctx_.regs_[kRDI] = this;

  can_resume_ = true;
  return true;
}

Coroutine::~Coroutine() { t_coroutine_count--; }

auto Coroutine::GetCurrentCoroutine() -> Coroutine * {
  if (t_current_coroutine == nullptr) {
    t_main_coroutine = new Coroutine();
    t_current_coroutine = t_main_coroutine;
  }

  return t_current_coroutine;
}

auto Coroutine::GetMainCoroutine() -> Coroutine * {
  if (t_main_coroutine == nullptr) {
    t_main_coroutine = new Coroutine();
  }

  return t_main_coroutine;
}

auto Coroutine::IsMainCoroutine() -> bool {
  return t_main_coroutine == nullptr || t_current_coroutine == t_main_coroutine;
}

void Coroutine::Yield() {
  if (t_main_coroutine == nullptr) {
    ErrorLog << "main coroutine is nullptr";
    return;
  }

  if (t_current_coroutine == t_main_coroutine) {
    ErrorLog << "cannot yield main coroutine";
    return;
  }

  Coroutine *co = t_current_coroutine;
  t_current_coroutine = t_main_coroutine;
  t_current_runtime = nullptr;
  CoctxSwap(&(co->coctx_), &(t_main_coroutine->coctx_));
}

void Coroutine::Resume(Coroutine *co) {
  if (t_current_coroutine != t_main_coroutine) {
    ErrorLog << "swap error, current coroutine is not main coroutine";
    return;
  }

  if (t_main_coroutine == nullptr) {
    ErrorLog << "main coroutine is nullptr";
    return;
  }

  if ((co == nullptr) || !co->can_resume_) {
    ErrorLog << "pennding coroutine is nullptr or cannot resume";
    return;
  }

  if (t_current_coroutine == co) {
    ErrorLog << "current coroutine is pending cor, need't resume";
    return;
  }

  t_current_coroutine = co;
  t_current_runtime = co->GetRuntime();

  CoctxSwap(&(t_main_coroutine->coctx_), &(co->coctx_));
}

}  // namespace tirpc