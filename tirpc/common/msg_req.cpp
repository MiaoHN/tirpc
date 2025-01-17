#include "tirpc/common/msg_req.hpp"

#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <random>
#include <string>

#include "tirpc/common/config.hpp"
#include "tirpc/common/log.hpp"

namespace tirpc {

extern Config::ptr g_rpc_config;

static thread_local std::string t_msg_req_nu;
static thread_local std::string t_max_msg_req_nu;
// static thread_local int t_msg_req_len = 20;

static int g_random_fd = -1;

auto MsgReqUtil::GenMsgNumber() -> std::string {
  int t_msg_req_len = 20;
  if (g_rpc_config) {
    t_msg_req_len = g_rpc_config->msg_req_len_;
  }

  if (t_msg_req_nu.empty() || t_msg_req_nu == t_max_msg_req_nu) {
    if (g_random_fd == -1) {
      g_random_fd = open("/dev/urandom", O_RDONLY);
    }
    std::string res(t_msg_req_len, 0);
    if (read(g_random_fd, &res.front(), t_msg_req_len) != t_msg_req_len) {
      ErrorLog << "read /dev/urandom data less " << t_msg_req_len << " bytes";
      return "";
    }
    t_max_msg_req_nu = "";
    for (int i = 0; i < t_msg_req_len; ++i) {
      uint8_t x = (static_cast<uint8_t>(res[i])) % 10;
      res[i] = x + '0';
      t_max_msg_req_nu += "9";
    }
    t_msg_req_nu = res;

  } else {
    int i = t_msg_req_nu.length() - 1;
    while (t_msg_req_nu[i] == '9' && i >= 0) {
      i--;
    }
    if (i >= 0) {
      t_msg_req_nu[i] += 1;
      for (size_t j = i + 1; j < t_msg_req_nu.length(); ++j) {
        t_msg_req_nu[j] = '0';
      }
    }
  }
  // DebugLog << "get msg_req_nu is " << t_msg_req_nu;
  return t_msg_req_nu;
}

}  // namespace tirpc