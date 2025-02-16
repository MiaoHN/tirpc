#pragma once

#include <memory>
#include <string>

#include "tirpc/net/http/http_define.hpp"
#include "tirpc/net/tcp/abstract_data.hpp"

namespace tirpc {

class HttpResponse : public AbstractData {
 public:
  using ptr = std::shared_ptr<HttpResponse>;

 public:
  std::string response_version_;
  int response_code_;
  std::string response_info_;
  HttpResponseHeader response_header_;
  std::string response_body_;
};

}  // namespace tirpc
