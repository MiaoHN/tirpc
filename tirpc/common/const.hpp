#pragma once

namespace tirpc {

enum LogLevel { TRACE, DEBUG, INFO, WARN, ERROR, FATAL, NONE };

enum class LoadBalanceCategory {
  // 随机算法
  Random,
  // 轮询算法
  Round,
  // 一致性哈希
  ConsistentHash
};

enum class ServiceRegisterCategory {
  None,
  Zk,
};

}  // namespace tirpc