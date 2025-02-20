#pragma once

namespace tirpc {

enum class LoadBalanceCategory {
  // 随机算法
  Random,
  // 轮询算法
  Round,
  // 一致性哈希
  ConsistentHash
};

enum class ServiceRegisterCategory {
  // 不进行服务注册
  None,
  // zk
  Zk,
};

}  // namespace tirpc