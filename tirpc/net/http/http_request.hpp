#pragma once

#include <map>
#include <memory>
#include <string>

#include "tirpc/net/http/http_define.hpp"
#include "tirpc/net/tcp/abstract_data.hpp"

namespace tirpc {

class HttpRequest : public AbstractData {
 public:
  using ptr = std::shared_ptr<HttpRequest>;

 public:
  HttpMethod request_method_;
  std::string request_path_;
  std::string request_query_;
  std::string request_version_;
  HttpRequestHeader requeset_header_;
  std::string request_body_;

  std::map<std::string, std::string> query_maps_;
};

}  // namespace tirpc
