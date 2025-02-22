#pragma once

#include <atomic>
#include <cstddef>
#include <vector>

namespace tirpc {

template <typename T, size_t N = 1024>
class LockFreeQueue {
 public:
  struct Element {
    std::atomic<bool> full;
    T data;
  };

  LockFreeQueue() : m_data(N), m_read_index(0), m_write_index(0) {
    // 显式初始化full标志为false
    for (auto &elem : m_data) {
      elem.full.store(false, std::memory_order_relaxed);
    }
  }

  bool empty() const {
    // 使用acquire确保获取最新的读写索引
    return m_read_index.load(std::memory_order_acquire) == m_write_index.load(std::memory_order_acquire);
  }

  bool enqueue(T value) {
    size_t write_index;
    Element *e = nullptr;
    do {
      write_index = m_write_index.load(std::memory_order_relaxed);
      size_t read_index = m_read_index.load(std::memory_order_acquire);  // 获取最新read_index
      if (write_index >= read_index + m_data.size()) {
        return false;  // 队列已满
      }
      size_t index = write_index % m_data.size();
      e = &m_data[index];
      // 检查该位置是否可用（full为false）
      if (e->full.load(std::memory_order_acquire)) {
        return false;  // 元素仍被占用
      }
    } while (!m_write_index.compare_exchange_weak(write_index, write_index + 1,
                                                  std::memory_order_release,  // CAS成功时同步之前的写入
                                                  std::memory_order_relaxed   // CAS失败时无需同步
                                                  ));

    e->data = std::move(value);
    e->full.store(true, std::memory_order_release);  // 确保data写入对dequeue可见
    return true;
  }

  bool dequeue(T &value) {
    size_t read_index;
    Element *e = nullptr;
    do {
      read_index = m_read_index.load(std::memory_order_relaxed);
      size_t write_index = m_write_index.load(std::memory_order_acquire);  // 获取最新write_index
      if (read_index >= write_index) {
        return false;  // 队列为空
      }
      size_t index = read_index % m_data.size();
      e = &m_data[index];
      // 检查该元素是否已填充
      if (!e->full.load(std::memory_order_acquire)) {
        return false;  // 元素未就绪
      }
    } while (!m_read_index.compare_exchange_weak(read_index, read_index + 1,
                                                 std::memory_order_release,  // CAS成功时同步之前的写入
                                                 std::memory_order_relaxed));

    value = std::move(e->data);
    e->full.store(false, std::memory_order_release);  // 确保data已移出后标记为空
    return true;
  }

 private:
  std::vector<Element> m_data;
  std::atomic<size_t> m_read_index;  // 无符号索引
  std::atomic<size_t> m_write_index;
};

}  // namespace tirpc