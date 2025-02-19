#include <algorithm>
#include <cctype>

#include "tirpc/net/tcp/load_balance.hpp"

namespace tirpc {

LoadBalanceStrategy::ptr LoadBalance::s_randomStrategy = std::make_shared<RandomLoadBalanceStrategy>();
LoadBalanceStrategy::ptr LoadBalance::s_RoundStrategy = std::make_shared<RoundLoadBalanceStrategy>();
LoadBalanceStrategy::ptr LoadBalance::s_consistentHashStrategy = std::make_shared<ConsistentHashLoadBalanceStrategy>();

LoadBalanceStrategy::ptr LoadBalance::queryStrategy(LoadBalanceCategory category) {
  switch (category) {
    case LoadBalanceCategory::Random:
      return s_randomStrategy;
    case LoadBalanceCategory::Round:
      return s_RoundStrategy;
    case LoadBalanceCategory::ConsistentHash:
      return s_consistentHashStrategy;
    default:
      return s_randomStrategy;
  }
}

std::string LoadBalance::strategy2Str(LoadBalanceCategory category) {
  switch (category) {
    case LoadBalanceCategory::Random:
      return "Random";
    case LoadBalanceCategory::Round:
      return "Round";
    case LoadBalanceCategory::ConsistentHash:
      return "ConsistentHash";
    default:
      return "Random";
  }
}

LoadBalanceCategory LoadBalance::str2Strategy(const std::string &str) {
  std::string strTemp = str;
  std::transform(str.begin(), str.end(), strTemp.begin(), tolower);
  if (strTemp == "round")
    return LoadBalanceCategory::Round;
  else if (strTemp == "consistenthash")
    return LoadBalanceCategory::ConsistentHash;
  return LoadBalanceCategory::Random;
}

}  // namespace tirpc