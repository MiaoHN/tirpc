#include "tirpc/common/mutex.hpp"

#include <pthread.h>
#include <memory>

#include "tirpc/common/log.hpp"
#include "tirpc/coroutine/coroutine.hpp"
#include "tirpc/coroutine/coroutine_hook.hpp"
#include "tirpc/net/base/reactor.hpp"

// this file copy form sylar

namespace tirpc {

CoroutineMutex::CoroutineMutex() = default;

CoroutineMutex::~CoroutineMutex() {
  if (lock_) {
    Unlock();
  }
}

void CoroutineMutex::Lock() {
  if (Coroutine::IsMainCoroutine()) {
    LOG_ERROR << "main coroutine can't use coroutine mutex";
    return;
  }

  Coroutine *cor = Coroutine::GetCurrentCoroutine();

  Mutex::Locker lock(mutex_);
  if (!lock_) {
    lock_ = true;
    LOG_DEBUG << "coroutine succ get coroutine mutex";
    lock.Unlock();
  } else {
    sleep_cors_.push(cor);
    auto tmp = sleep_cors_;
    lock.Unlock();

    LOG_DEBUG << "coroutine yield, pending coroutine mutex, current sleep queue exist [" << tmp.size() << "] coroutines";

    Coroutine::Yield();
  }
}

void CoroutineMutex::Unlock() {
  if (Coroutine::IsMainCoroutine()) {
    LOG_ERROR << "main coroutine can't use coroutine mutex";
    return;
  }

  Mutex::Locker lock(mutex_);
  if (lock_) {
    lock_ = false;
    if (sleep_cors_.empty()) {
      return;
    }

    Coroutine *cor = sleep_cors_.front();
    sleep_cors_.pop();
    lock.Unlock();

    if (cor != nullptr) {
      // wakeup the first cor in sleep queue
      LOG_DEBUG << "coroutine unlock, now to resume coroutine[" << cor->GetCorId() << "]";

      Reactor::GetReactor()->AddTask([cor]() { Coroutine::Resume(cor); }, true);
    }
  }
}

}  // namespace tirpc