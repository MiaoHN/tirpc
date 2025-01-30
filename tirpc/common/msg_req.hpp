#pragma once

#include <string>

namespace tirpc {

class MsgReqUtil {
 public:
  static auto GenMsgNumber() -> std::string;
};

}  // namespace tirpc