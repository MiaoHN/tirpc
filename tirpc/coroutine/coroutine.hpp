#pragma once

#include <functional>
#include <memory>
#include <string>

#include "tirpc/coroutine/coctx.hpp"

namespace tirpc {

struct Runtime {
  std::string msg_no_;
  std::string interface_name_;
};

auto GetCoroutineIndex() -> int;

auto GetCurrentRuntime() -> Runtime *;

void SetCurrentRuntime(Runtime *runtime);

class Coroutine {
 public:
 public:
  using ptr = std::shared_ptr<Coroutine>;

 private:
  Coroutine();

 public:
  Coroutine(int size, char *stack_ptr);

  Coroutine(int size, char *stack_ptr, std::function<void()> cb);

  ~Coroutine();

  auto SetCallBack(std::function<void()> cb) -> bool;

  auto GetCorId() const -> int { return cor_id_; }

  void SetIsInCoFunc(bool v) { is_in_cofunc_ = v; }

  auto GetIsInCoFunc() const -> bool { return is_in_cofunc_; }

  auto GetMsgNo() -> std::string { return msg_no_; }

  auto GetRuntime() -> Runtime * { return &runtime_; }

  void SetMsgNo(const std::string &msg_no) { msg_no_ = msg_no; }

  void SetIndex(int index) { index_ = index; }

  auto GetIndex() -> int { return index_; }

  auto GetStackPtr() -> char * { return stack_sp_; }

  auto GetStackSize() -> int { return stack_size_; }

  void SetCanResume(bool v) { can_resume_ = v; }

 public:
  static void Yield();

  static void Resume(Coroutine *cor);

  static auto GetCurrentCoroutine() -> Coroutine *;

  static auto GetMainCoroutine() -> Coroutine *;

  static auto IsMainCoroutine() -> bool;

  // static void SetCoroutineSwapFlag(bool value);

  // static bool GetCoroutineSwapFlag();

 private:
  int cor_id_{0};      // coroutine' id
  CoCtx coctx_;        // coroutine regs
  int stack_size_{0};  // size of stack memory space

  /// coroutine's stack memory space, you can malloc or mmap get some mermory to init this value
  char *stack_sp_{nullptr};
  bool is_in_cofunc_{false};  // true when call CoFunction, false when CoFunction finished
  std::string msg_no_;
  Runtime runtime_;

  bool can_resume_{true};

  int index_{-1};  // index in coroutine pool

 public:
  std::function<void()> callback_{nullptr};
};

}  // namespace tirpc