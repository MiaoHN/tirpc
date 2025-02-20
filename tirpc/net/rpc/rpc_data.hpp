#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include "tirpc/common/log.hpp"
#include "tirpc/net/tcp/abstract_data.hpp"

namespace tirpc {

class TinyPbStruct : public AbstractData {
 public:
  using pb_ptr = std::shared_ptr<TinyPbStruct>;
  TinyPbStruct() = default;
  ~TinyPbStruct() override = default;
  TinyPbStruct(const TinyPbStruct &) = default;
  auto operator=(const TinyPbStruct &) -> TinyPbStruct & = default;
  TinyPbStruct(TinyPbStruct &&) = default;
  auto operator=(TinyPbStruct &&) -> TinyPbStruct & = default;
  auto GetMsgReq() const -> std::string override { return msg_seq_; }

  /*
  **  min of package is: 1 + 4 + 4 + 4 + 4 + 4 + 4 + 1 = 26 bytes
  **
  */

  // char start;                      // indentify start of a TinyPb protocal data
  int32_t pk_len_{0};              // len of all package(include start char and end char)
  int32_t msg_req_len_{0};         // len of msg_req
  std::string msg_seq_;            // msg_req, which identify a request
  int32_t service_name_len_{0};    // len of service full name
  std::string service_full_name_;  // service full name, like QueryService.query_name
  // err_code, 0 -- call rpc success, otherwise -- call rpc failed. it only be seted by RpcController
  int32_t err_code_{0};
  int32_t err_info_len_{0};  // len of err_info
  std::string err_info_;   // err_info, empty -- call rpc success, otherwise -- call rpc failed, it will display details
                           // of reason why call rpc failed. it only be seted by RpcController
  std::string pb_data_;    // business pb data
  int32_t check_num_{-1};  // check_num of all package. to check legality of data
  // char end;                        // identify end of a TinyPb protocal data
};

}  // namespace tirpc
