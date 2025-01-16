#pragma once

#include <functional>
#include <memory>

namespace tirpc {

template <class T>
class AbstractSlot {
 public:
  using ptr = std::shared_ptr<AbstractSlot>;
  using weakPtr = std::weak_ptr<T>;
  using sharedPtr = std::shared_ptr<T>;

  AbstractSlot(weakPtr ptr, std::function<void(sharedPtr)> cb) : weak_ptr_(ptr), callback_(cb) {}
  ~AbstractSlot() {
    sharedPtr ptr = weak_ptr_.lock();
    if (ptr) {
      callback_(ptr);
    }
  }

 private:
  weakPtr weak_ptr_;
  std::function<void(sharedPtr)> callback_;
};

}  // namespace tirpc