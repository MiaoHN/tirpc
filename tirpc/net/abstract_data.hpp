#pragma once

namespace tirpc {

class AbstractData {
 public:
  AbstractData() = default;

  virtual ~AbstractData() = default;

  bool decode_succ_{false};
  bool encode_succ_{false};
};

}  // namespace tirpc
