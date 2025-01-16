#pragma once

#include <string>

namespace tirpc {

class MsgReqUtil {
 public:
  static std::string GenMsgNumber();
};

}  // namespace tirpc