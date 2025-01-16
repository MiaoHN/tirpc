#pragma once

#include <arpa/inet.h>
#include <cstdint>
#include <cstring>

namespace tirpc {

auto GetInt32FromNetByte(const char *buf) -> int32_t {
  int32_t tmp;
  memcpy(&tmp, buf, sizeof(tmp));
  return ntohl(tmp);
}

}  // namespace tirpc