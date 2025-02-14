#pragma once

#include <memory>
#include "tirpc/common/msg_req.hpp"

namespace tirpc {

class AbstractData {
 public:
  using ptr = std::shared_ptr<AbstractData>;

  AbstractData() = default;

  virtual ~AbstractData() = default;

  virtual auto GetMsgReq() const -> std::string { return MsgReqUtil::GenMsgNumber(); }

  bool decode_succ_{false};
  bool encode_succ_{false};
};

}  // namespace tirpc
