#include "tirpc/net/rpc/rpc_codec.hpp"

#include <cstring>
#include <memory>
#include <vector>

#include "tirpc/common/log.hpp"
#include "tirpc/common/msg_req.hpp"
#include "tirpc/net/base/byte.hpp"
#include "tirpc/net/rpc/rpc_data.hpp"
#include "tirpc/net/tcp/abstract_data.hpp"
#include "tirpc/net/tcp/tcp_buffer.hpp"

namespace tirpc {

static const char PB_START = 0x02;  // start char
static const char PB_END = 0x03;    // end char
// static const int MSG_REQ_LEN = 20;  // default length of msg_req

TinyPbCodeC::TinyPbCodeC() = default;

TinyPbCodeC::~TinyPbCodeC() = default;

void TinyPbCodeC::Encode(TcpBuffer *buf, AbstractData *data) {
  if ((buf == nullptr) || (data == nullptr)) {
    LOG_ERROR << "encode error! buf or data nullptr";
    return;
  }
  // LOG_DEBUG << "test encode start";
  auto *tmp = dynamic_cast<TinyPbStruct *>(data);

  int len = 0;
  const char *re = EncodePbData(tmp, len);
  if (re == nullptr || len == 0 || !tmp->encode_succ_) {
    LOG_ERROR << "encode error";
    data->encode_succ_ = false;
    return;
  }
  LOG_DEBUG << "encode package len = " << len;
  if (buf != nullptr) {
    buf->WriteToBuffer(re, len);
    LOG_DEBUG << "succ encode and write to buffer, writeindex=" << buf->WriteIndex();
  }
  data = tmp;
  if (re != nullptr) {
    free(reinterpret_cast<void *>(const_cast<char *>(re)));
    re = nullptr;
  }
  // LOG_DEBUG << "test encode end";
}

auto TinyPbCodeC::EncodePbData(TinyPbStruct *data, int &len) -> const char * {
  if (data->service_full_name_.empty()) {
    LOG_ERROR << "parse error, service_full_name_ is empty";
    data->encode_succ_ = false;
    return nullptr;
  }
  if (data->msg_seq_.empty()) {
    data->msg_seq_ = MsgReqUtil::GenMsgNumber();
    data->msg_req_len_ = data->msg_seq_.length();
    LOG_DEBUG << "generate msgno = " << data->msg_seq_;
  }

  int32_t pk_len = 2 * sizeof(char) + 6 * sizeof(int32_t) + data->pb_data_.length() +
                   data->service_full_name_.length() + data->msg_seq_.length() + data->err_info_.length();

  LOG_DEBUG << "encode pk_len = " << pk_len;
  char *buf = reinterpret_cast<char *>(malloc(pk_len));
  char *tmp = buf;
  *tmp = PB_START;
  tmp++;

  int32_t pk_len_net = htonl(pk_len);
  memcpy(tmp, &pk_len_net, sizeof(int32_t));
  tmp += sizeof(int32_t);

  int32_t msg_req_len = data->msg_seq_.length();
  LOG_DEBUG << "msg_req_len_= " << msg_req_len;
  int32_t msg_req_len__net = htonl(msg_req_len);
  memcpy(tmp, &msg_req_len__net, sizeof(int32_t));
  tmp += sizeof(int32_t);

  if (msg_req_len != 0) {
    memcpy(tmp, (data->msg_seq_).data(), msg_req_len);
    tmp += msg_req_len;
  }

  int32_t service_full_name_len = data->service_full_name_.length();
  LOG_DEBUG << "src service_full_name_len_ = " << service_full_name_len;
  int32_t service_full_name_len_net = htonl(service_full_name_len);
  memcpy(tmp, &service_full_name_len_net, sizeof(int32_t));
  tmp += sizeof(int32_t);

  if (service_full_name_len != 0) {
    memcpy(tmp, (data->service_full_name_).data(), service_full_name_len);
    tmp += service_full_name_len;
  }

  int32_t err_code = data->err_code_;
  LOG_DEBUG << "err_code_= " << err_code;
  int32_t err_code_net = htonl(err_code);
  memcpy(tmp, &err_code_net, sizeof(int32_t));
  tmp += sizeof(int32_t);

  int32_t err_info_len = data->err_info_.length();
  LOG_DEBUG << "err_info_len= " << err_info_len;
  int32_t err_info_len_net = htonl(err_info_len);
  memcpy(tmp, &err_info_len_net, sizeof(int32_t));
  tmp += sizeof(int32_t);

  if (err_info_len != 0) {
    memcpy(tmp, (data->err_info_).data(), err_info_len);
    tmp += err_info_len;
  }

  memcpy(tmp, (data->pb_data_).data(), data->pb_data_.length());
  tmp += data->pb_data_.length();
  LOG_DEBUG << "pb_data_len= " << data->pb_data_.length();

  int32_t checksum = 1;
  int32_t checksum_net = htonl(checksum);
  memcpy(tmp, &checksum_net, sizeof(int32_t));
  tmp += sizeof(int32_t);

  *tmp = PB_END;

  data->pk_len_ = pk_len;
  data->msg_req_len_ = msg_req_len;
  data->service_name_len_ = service_full_name_len;
  data->err_info_len_ = err_info_len;

  // checksum has not been implemented yet, directly skip chcksum
  data->check_num_ = checksum;
  data->encode_succ_ = true;

  len = pk_len;

  return buf;
}

void TinyPbCodeC::Decode(TcpBuffer *buf, AbstractData *data) {
  if ((buf == nullptr) || (data == nullptr)) {
    LOG_ERROR << "decode error! buf or data nullptr";
    return;
  }

  std::vector<char> tmp = buf->GetBufferVector();
  // int total_size = buf->readAble();
  int start_index = buf->ReadIndex();
  int end_index = -1;
  int32_t pk_len = -1;

  bool parse_full_pack = false;

  for (int i = start_index; i < buf->WriteIndex(); ++i) {
    // first find start
    if (tmp[i] == PB_START) {
      if (i + 1 < buf->WriteIndex()) {
        pk_len = GetInt32FromNetByte(&tmp[i + 1]);
        LOG_DEBUG << "prase pk_len =" << pk_len;
        int j = i + pk_len - 1;
        LOG_DEBUG << "j =" << j << ", i=" << i;

        if (j >= buf->WriteIndex()) {
          // LOG_DEBUG << "recv package not complete, or pk_start find error, continue next parse";
          continue;
        }
        if (tmp[j] == PB_END) {
          start_index = i;
          end_index = j;
          // LOG_DEBUG << "parse succ, now break";
          parse_full_pack = true;
          break;
        }
      }
    }
  }

  if (!parse_full_pack) {
    LOG_DEBUG << "not parse full package, return";
    return;
  }

  buf->RecycleRead(end_index + 1 - start_index);

  LOG_DEBUG << "read_buffer size=" << buf->GetBufferVector().size() << "rd=" << buf->ReadIndex()
            << "wd=" << buf->WriteIndex();

  // TinyPbStruct pb_struct;
  auto pb_struct = dynamic_cast<TinyPbStruct *>(data);
  pb_struct->pk_len_ = pk_len;
  pb_struct->decode_succ_ = false;

  int msg_req_len__index = start_index + sizeof(char) + sizeof(int32_t);
  if (msg_req_len__index >= end_index) {
    LOG_ERROR << "parse error, msg_req_len__index[" << msg_req_len__index << "] >= end_index[" << end_index << "]";
    // drop this error package
    return;
  }

  pb_struct->msg_req_len_ = GetInt32FromNetByte(&tmp[msg_req_len__index]);
  if (pb_struct->msg_req_len_ == 0) {
    LOG_ERROR << "prase error, msg_req_ emptr";
    return;
  }

  LOG_DEBUG << "msg_req_len_= " << pb_struct->msg_req_len_;
  int msg_req_index = msg_req_len__index + sizeof(int32_t);
  LOG_DEBUG << "msg_req_len__index= " << msg_req_index;

  char msg_req[50] = {0};

  memcpy(&msg_req[0], &tmp[msg_req_index], pb_struct->msg_req_len_);
  pb_struct->msg_seq_ = std::string(msg_req);
  LOG_DEBUG << "msg_req_= " << pb_struct->msg_seq_;

  int service_name_len_index = msg_req_index + pb_struct->msg_req_len_;
  if (service_name_len_index >= end_index) {
    LOG_ERROR << "parse error, service_name_len_index[" << service_name_len_index << "] >= end_index[" << end_index
              << "]";
    // drop this error package
    return;
  }

  LOG_DEBUG << "service_name_len_index = " << service_name_len_index;
  int service_name_index = service_name_len_index + sizeof(int32_t);

  if (service_name_index >= end_index) {
    LOG_ERROR << "parse error, service_name_index[" << service_name_index << "] >= end_index[" << end_index << "]";
    return;
  }

  pb_struct->service_name_len_ = GetInt32FromNetByte(&tmp[service_name_len_index]);

  if (pb_struct->service_name_len_ > pk_len) {
    LOG_ERROR << "parse error, service_name_len[" << pb_struct->service_name_len_ << "] >= pk_len [" << pk_len << "]";
    return;
  }
  LOG_DEBUG << "service_name_len = " << pb_struct->service_name_len_;

  char service_name[512] = {0};

  memcpy(&service_name[0], &tmp[service_name_index], pb_struct->service_name_len_);
  pb_struct->service_full_name_ = std::string(service_name);
  LOG_DEBUG << "service_name = " << pb_struct->service_full_name_;

  int err_code_index = service_name_index + pb_struct->service_name_len_;
  pb_struct->err_code_ = GetInt32FromNetByte(&tmp[err_code_index]);

  int err_info_len_index = err_code_index + sizeof(int32_t);

  if (err_info_len_index >= end_index) {
    LOG_ERROR << "parse error, err_info_len_index[" << err_info_len_index << "] >= end_index[" << end_index << "]";
    // drop this error package
    return;
  }
  pb_struct->err_info_len_ = GetInt32FromNetByte(&tmp[err_info_len_index]);
  LOG_DEBUG << "err_info_len = " << pb_struct->err_info_len_;
  int err_info_index = err_info_len_index + sizeof(int32_t);

  char err_info[512] = {0};

  memcpy(&err_info[0], &tmp[err_info_index], pb_struct->err_info_len_);
  pb_struct->err_info_ = std::string(err_info);

  int pb_data_len = pb_struct->pk_len_ - pb_struct->service_name_len_ - pb_struct->msg_req_len_ -
                    pb_struct->err_info_len_ - 2 * sizeof(char) - 6 * sizeof(int32_t);

  int pb_data_index = err_info_index + pb_struct->err_info_len_;
  LOG_DEBUG << "pb_data_len= " << pb_data_len << ", pb_index = " << pb_data_index;

  if (pb_data_index >= end_index) {
    LOG_ERROR << "parse error, pb_data_index[" << pb_data_index << "] >= end_index[" << end_index << "]";
    return;
  }
  // LOG_DEBUG << "pb_data_index = " << pb_data_index << ", pb_data_.length = " << pb_data_len;

  std::string pb_data_str(&tmp[pb_data_index], pb_data_len);
  pb_struct->pb_data_ = pb_data_str;

  // LOG_DEBUG << "decode succ,  pk_len = " << pk_len << ", service_name = " << pb_struct->service_full_name_;

  pb_struct->decode_succ_ = true;
  data = pb_struct;
}

auto TinyPbCodeC::GenDataPtr() -> AbstractData::ptr { return std::make_shared<TinyPbStruct>(); }

}  // namespace tirpc
