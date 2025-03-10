#pragma once

#include <map>
#include <string>
#include <string_view>

#include "tirpc/net/http/http_request.hpp"
#include "tirpc/net/tcp/abstract_codec.hpp"
#include "tirpc/net/tcp/abstract_data.hpp"

namespace tirpc {

class HttpCodeC : public AbstractCodeC {
 public:
  HttpCodeC();

  ~HttpCodeC() override;

  void Encode(TcpBuffer *buf, AbstractData *data) override;

  void Decode(TcpBuffer *buf, AbstractData *data) override;

  auto GenDataPtr() -> AbstractData::ptr override;

 private:
  auto ParseHttpRequestLine(HttpRequest *requset, std::string_view tmp) -> bool;
  auto ParseHttpRequestHeader(HttpRequest *requset, std::string_view tmp) -> bool;
  auto ParseHttpRequestContent(HttpRequest *requset, std::string_view tmp) -> bool;
};

}  // namespace tirpc
