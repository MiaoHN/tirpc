#include "tirpc/net/http/http_define.hpp"

#include <sstream>
#include <string>

namespace tirpc {

std::string g_crlf = "\r\n";
std::string g_crlf_double = "\r\n\r\n";

std::string content_type_text = "text/html;charset=utf-8";
const char *default_html_template = "<html><body><h1>%s</h1><p>%s</p></body></html>";

auto HttpCodeToString(const int code) -> const char * {
  switch (code) {
    case HTTP_OK:
      return "OK";

    case HTTP_BADREQUEST:
      return "Bad Request";

    case HTTP_FORBIDDEN:
      return "Forbidden";

    case HTTP_NOTFOUND:
      return "Not Found";

    case HTTP_INTERNALSERVERERROR:
      return "Internal Server Error";

    default:
      return "UnKnown code";
  }
}

auto HttpHeaderComm::GetValue(const std::string &key) -> std::string { return maps_[key.c_str()]; }

auto HttpHeaderComm::GetHeaderTotalLength() -> int {
  int len = 0;
  for (const auto &it : maps_) {
    len += it.first.length() + 1 + it.second.length() + 2;
  }
  return len;
}

void HttpHeaderComm::SetKeyValue(const std::string &key, const std::string &value) { maps_[key.c_str()] = value; }

auto HttpHeaderComm::ToHttpString() -> std::string {
  std::stringstream ss;
  for (const auto &it : maps_) {
    ss << it.first << ":" << it.second << "\r\n";
  }
  return ss.str();
}

// void HttpRequestHeader::storeToMap() {
//   maps_["Accept"] = accept_;
//   maps_["Referer"] = referer_;
//   maps_["Accept-Language"] = accept_language_;
//   maps_["Accept-Charset"] = accept_charset_;
//   maps_["User-Agent"] = user_agent_;
//   maps_["Host"] = host_;
//   maps_["Content-Type"] = content_type_;
//   maps_["Content-Length"] = content_length_;
//   maps_["Connection"] = connection_;
//   maps_["Cookie"] = cookie_;
// }

// void HttpResponseHeader::storeToMap() {
//   maps_["Server"] = server_;
//   maps_["Content-Type"] = content_type_;
//   maps_["Content-Length"] = content_length_;
//   maps_["Connection"] = connection_;
//   maps_["Set-Cookie"] = set_cookie_;
// }

}  // namespace tirpc