#pragma once

#include <pthread.h>
#include <memory>
#include <queue>

// mostly copied form sylar

namespace tirpc {

template <class T>
struct ScopedLockImpl {
 public:
  explicit ScopedLockImpl(T &mutex) : mutex_(mutex) {
    mutex_.Lock();
    locked_ = true;
  }

  ~ScopedLockImpl() { Unlock(); }

  void Lock() {
    if (!locked_) {
      mutex_.Lock();
      locked_ = true;
    }
  }

  void Unlock() {
    if (locked_) {
      mutex_.Unlock();
      locked_ = false;
    }
  }

 private:
  /// mutex
  T &mutex_;
  /// 是否已上锁
  bool locked_;
};

template <class T>
struct ReadScopedLockImpl {
 public:
  explicit ReadScopedLockImpl(T &mutex) : mutex_(mutex) {
    mutex_.RDLock();
    locked_ = true;
  }

  ~ReadScopedLockImpl() { Unlock(); }

  void Lock() {
    if (!locked_) {
      mutex_.RDLock();
      locked_ = true;
    }
  }

  void Unlock() {
    if (locked_) {
      mutex_.Unlock();
      locked_ = false;
    }
  }

 private:
  /// mutex
  T &mutex_;
  /// 是否已上锁
  bool locked_;
};

/**
 * @brief 局部写锁模板实现
 */
template <class T>
struct WriteScopedLockImpl {
 public:
  explicit WriteScopedLockImpl(T &mutex) : mutex_(mutex) {
    mutex_.WRLock();
    locked_ = true;
  }

  ~WriteScopedLockImpl() { Unlock(); }

  void Lock() {
    if (!locked_) {
      mutex_.WRLock();
      locked_ = true;
    }
  }

  void Unlock() {
    if (locked_) {
      mutex_.Unlock();
      locked_ = false;
    }
  }

 private:
  T &mutex_;
  bool locked_;
};

class Mutex {
 public:
  /// 局部锁
  using Locker = ScopedLockImpl<Mutex>;

  Mutex() { pthread_mutex_init(&mutex_, nullptr); }

  ~Mutex() { pthread_mutex_destroy(&mutex_); }

  void Lock() { pthread_mutex_lock(&mutex_); }

  void Unlock() { pthread_mutex_unlock(&mutex_); }

  auto GetMutex() -> pthread_mutex_t * { return &mutex_; }

 private:
  /// mutex
  pthread_mutex_t mutex_;
};

class RWMutex {
 public:
  /// 局部读锁
  using ReadLocker = ReadScopedLockImpl<RWMutex>;

  using WriteLocker = WriteScopedLockImpl<RWMutex>;

  RWMutex() { pthread_rwlock_init(&lock_, nullptr); }

  ~RWMutex() { pthread_rwlock_destroy(&lock_); }

  void RDLock() { pthread_rwlock_rdlock(&lock_); }

  void WRLock() { pthread_rwlock_wrlock(&lock_); }

  void Unlock() { pthread_rwlock_unlock(&lock_); }

 private:
  pthread_rwlock_t lock_;
};

class Coroutine;

class CoroutineMutex {
 public:
  using Locker = ScopedLockImpl<CoroutineMutex>;

  CoroutineMutex();

  ~CoroutineMutex();

  void Lock();

  void Unlock();

 private:
  bool lock_{false};
  Mutex mutex_;
  std::queue<Coroutine *> sleep_cors_;
};

}  // namespace tirpc