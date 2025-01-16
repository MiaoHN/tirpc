#pragma once

#include <map>
#include <string>

namespace tirpc {

extern std::string g_crlf;
extern std::string g_crlf_double;

extern std::string content_type_text;
extern const char *default_html_template;

enum HttpMethod {
  GET = 1,
  POST = 2,
};

enum HttpCode {
  HTTP_OK = 200,
  HTTP_BADREQUEST = 400,
  HTTP_FORBIDDEN = 403,
  HTTP_NOTFOUND = 404,
  HTTP_INTERNALSERVERERROR = 500,
};

auto HttpCodeToString(int code) -> const char *;

class HttpHeaderComm {
 public:
  HttpHeaderComm() = default;

  virtual ~HttpHeaderComm() = default;

  auto GetHeaderTotalLength() -> int;

  // virtual void storeToMap() = 0;

  auto GetValue(const std::string &key) -> std::string;

  void SetKeyValue(const std::string &key, const std::string &value);

  auto ToHttpString() -> std::string;

 public:
  std::map<std::string, std::string> maps_;
};

class HttpRequestHeader : public HttpHeaderComm {
 public:
  // std::string accept_;
  // std::string referer_;
  // std::string accept_language_;
  // std::string accept_charset_;
  // std::string user_agent_;
  // std::string host_;
  // std::string content_type_;
  // std::string content_length_;
  // std::string connection_;
  // std::string cookie_;

  //  public:
  //   void storeToMap();
};

class HttpResponseHeader : public HttpHeaderComm {
  //  public:
  //   std::string server_;
  //   std::string content_type_;
  //   std::string content_length_;
  //   std::string set_cookie_;
  //   std::string connection_;

  //  public:
  //   void storeToMap();
};

}  // namespace tirpc
