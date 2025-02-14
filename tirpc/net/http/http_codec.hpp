#pragma once

#include <map>
#include <string>
#include "tirpc/net/tcp/abstract_codec.hpp"
#include "tirpc/net/tcp/abstract_data.hpp"
#include "tirpc/net/http/http_request.hpp"

namespace tirpc {

class HttpCodeC : public AbstractCodeC {
 public:
  HttpCodeC();

  ~HttpCodeC() override;

  void Encode(TcpBuffer *buf, AbstractData *data) override;

  void Decode(TcpBuffer *buf, AbstractData *data) override;

  auto GetProtocalType() -> ProtocalType override;

 private:
  auto ParseHttpRequestLine(HttpRequest *requset, const std::string &tmp) -> bool;
  auto ParseHttpRequestHeader(HttpRequest *requset, const std::string &tmp) -> bool;
  auto ParseHttpRequestContent(HttpRequest *requset, const std::string &tmp) -> bool;
};

}  // namespace tirpc
