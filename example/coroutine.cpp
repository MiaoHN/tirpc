#include "tirpc/coroutine/coroutine.hpp"
#include <google/protobuf/service.h>
#include <pthread.h>
#include <iostream>

#include "tirpc/common/log.hpp"
#include "tirpc/common/mutex.hpp"
#include "tirpc/coroutine/coroutine_hook.hpp"
#include "tirpc/coroutine/coroutine_pool.hpp"

tirpc::Coroutine::ptr cor;
tirpc::Coroutine::ptr cor2;

class Test {
 public:
  tirpc::CoroutineMutex cor_mutex_;
  int a_ = 1;
};

Test test;

void Fun1() {
  std::cout << "cor1 ---- now fitst resume fun1 coroutine by thread 1" << std::endl;
  std::cout << "cor1 ---- now begin to yield fun1 coroutine" << std::endl;

  test.cor_mutex_.Lock();

  std::cout << "cor1 ---- coroutine lock on test, sleep 5s begin" << std::endl;

  sleep(5);
  std::cout << "cor1 ---- sleep 5s end, now back coroutine lock" << std::endl;

  test.cor_mutex_.Unlock();

  tirpc::Coroutine::Yield();
  std::cout << "cor1 ---- fun1 coroutine back, now end" << std::endl;
}

void Fun2() {
  std::cout << "cor222 ---- now fitst resume fun1 coroutine by thread 1" << std::endl;
  std::cout << "cor222 ---- now begin to yield fun1 coroutine" << std::endl;

  sleep(2);
  std::cout << "cor222 ---- coroutine2 want to get coroutine lock of test" << std::endl;
  test.cor_mutex_.Lock();

  std::cout << "cor222 ---- coroutine2 get coroutine lock of test succ" << std::endl;
}

auto Thread1Func(void * /*unused*/) -> void * {
  std::cout << "thread 1 begin" << std::endl;
  std::cout << "now begin to resume fun1 coroutine in thread 1" << std::endl;
  tirpc::Coroutine::Resume(cor.get());
  std::cout << "now fun1 coroutine back in thread 1" << std::endl;
  std::cout << "thread 1 end" << std::endl;
  return nullptr;
}

auto Thread2Func(void * /*unused*/) -> void * {
  tirpc::Coroutine::GetCurrentCoroutine();
  std::cout << "thread 2 begin" << std::endl;
  std::cout << "now begin to resume fun1 coroutine in thread 2" << std::endl;
  tirpc::Coroutine::Resume(cor2.get());
  std::cout << "now fun1 coroutine back in thread 2" << std::endl;
  std::cout << "thread 2 end" << std::endl;
  return nullptr;
}

auto main(int argc, char *argv[]) -> int {
  tirpc::SetHook(false);
  std::cout << "main begin" << std::endl;
  int stack_size = 128 * 1024;
  char *sp = reinterpret_cast<char *>(malloc(stack_size));
  cor = std::make_shared<tirpc::Coroutine>(stack_size, sp, Fun1);

  char *sp2 = reinterpret_cast<char *>(malloc(stack_size));
  cor2 = std::make_shared<tirpc::Coroutine>(stack_size, sp2, Fun2);

  pthread_t thread2;
  pthread_create(&thread2, nullptr, &Thread2Func, nullptr);

  Thread1Func(nullptr);

  pthread_join(thread2, nullptr);

  std::cout << "main end" << std::endl;
}
