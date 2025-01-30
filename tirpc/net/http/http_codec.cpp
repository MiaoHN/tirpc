#include "tirpc/net/http/http_codec.hpp"

#include <algorithm>
#include <sstream>

#include "tirpc/common/log.hpp"
#include "tirpc/common/string_util.hpp"
#include "tirpc/net/abstract_codec.hpp"
#include "tirpc/net/abstract_data.hpp"
#include "tirpc/net/http/http_request.hpp"
#include "tirpc/net/http/http_response.hpp"
#include "tirpc/net/tcp/tcp_buffer.hpp"

namespace tirpc {

HttpCodeC::HttpCodeC() = default;

HttpCodeC::~HttpCodeC() = default;

void HttpCodeC::Encode(TcpBuffer *buf, AbstractData *data) {
  DebugLog << "test encode";
  auto *response = dynamic_cast<HttpResponse *>(data);
  response->encode_succ_ = false;

  std::stringstream ss;
  ss << response->response_version_ << " " << response->response_code_ << " " << response->response_info_ << "\r\n"
     << response->response_header_.ToHttpString() << "\r\n"
     << response->response_body_;
  std::string http_res = ss.str();
  DebugLog << "encode http response is:  " << http_res;

  buf->WriteToBuffer(http_res.c_str(), http_res.length());
  DebugLog << "succ encode and write to buffer, writeindex=" << buf->WriteIndex();
  response->encode_succ_ = true;
  DebugLog << "test encode end";
}

void HttpCodeC::Decode(TcpBuffer *buf, AbstractData *data) {
  DebugLog << "test http decode start";
  std::string strs;
  if ((buf == nullptr) || (data == nullptr)) {
    ErrorLog << "decode error! buf or data nullptr";
    return;
  }
  auto *request = dynamic_cast<HttpRequest *>(data);
  if (request == nullptr) {
    ErrorLog << "not httprequest type";
    return;
  }

  strs = buf->GetBufferString();

  bool is_parse_request_line = false;
  bool is_parse_request_header = false;
  bool is_parse_request_content = false;
  // bool is_parse_succ = false;
  int read_size = 0;
  std::string tmp(strs);
  DebugLog << "pending to parse str:" << tmp << ", total size =" << tmp.size();
  int len = tmp.length();
  while (true) {
    if (!is_parse_request_line) {
      size_t i = tmp.find(g_crlf);
      if (i == std::string::npos) {
        DebugLog << "not found CRLF in buffer";
        return;
      }
      if (i == tmp.length() - 2) {
        DebugLog << "need to read more data";
        break;
      }
      is_parse_request_line = ParseHttpRequestLine(request, tmp.substr(0, i));
      if (!is_parse_request_line) {
        return;
      }
      tmp = tmp.substr(i + 2, len - 2 - i);
      len = tmp.length();
      read_size = read_size + i + 2;
    }

    if (!is_parse_request_header) {
      size_t j = tmp.find(g_crlf_double);
      if (j == std::string::npos) {
        DebugLog << "not found CRLF CRLF in buffer";
        return;
      }
      // if (j == tmp.length() - 4) {
      //   DebugLog << "need to read more data";
      //   goto parse_error;
      // }
      is_parse_request_header = ParseHttpRequestHeader(request, tmp.substr(0, j));
      if (!is_parse_request_header) {
        return;
      }
      tmp = tmp.substr(j + 4, len - 4 - j);
      len = tmp.length();
      read_size = read_size + j + 4;
    }
    if (!is_parse_request_content) {
      int content_len = std::atoi(request->requeset_header_.maps_["Content-Length"].c_str());
      if (static_cast<int>(strs.length()) - read_size < content_len) {
        DebugLog << "need to read more data";
        return;
      }
      if (request->request_method_ == POST && content_len != 0) {
        is_parse_request_content = ParseHttpRequestContent(request, tmp.substr(0, content_len));
        if (!is_parse_request_content) {
          return;
        }
        read_size = read_size + content_len;
      } else {
        is_parse_request_content = true;
      }
    }
    if (is_parse_request_line && is_parse_request_header && is_parse_request_header) {
      DebugLog << "parse http request success, read size is " << read_size << " bytes";
      buf->RecycleRead(read_size);
      break;
    }
  }

  request->decode_succ_ = true;
  data = request;

  DebugLog << "test http decode end";
}

auto HttpCodeC::ParseHttpRequestLine(HttpRequest *requset, const std::string &tmp) -> bool {
  size_t s1 = tmp.find_first_of(' ');
  size_t s2 = tmp.find_last_of(' ');

  if (s1 == std::string::npos || s2 == std::string::npos || s1 == s2) {
    ErrorLog << "error read Http Requser Line, space is not 2";
    return false;
  }
  std::string method = tmp.substr(0, s1);
  std::transform(method.begin(), method.end(), method.begin(), toupper);
  if (method == "GET") {
    requset->request_method_ = HttpMethod::GET;
  } else if (method == "POST") {
    requset->request_method_ = HttpMethod::POST;
  } else {
    ErrorLog << "parse http request request line error, not support http method:" << method;
    return false;
  }

  std::string version = tmp.substr(s2 + 1, tmp.length() - s2 - 1);
  std::transform(version.begin(), version.end(), version.begin(), toupper);
  if (version != "HTTP/1.1" && version != "HTTP/1.0") {
    ErrorLog << "parse http request request line error, not support http version:" << version;
    return false;
  }
  requset->request_version_ = version;

  std::string url = tmp.substr(s1 + 1, s2 - s1 - 1);
  size_t j = url.find("://");

  if (j != std::string::npos && j + 3 >= url.length()) {
    ErrorLog << "parse http request request line error, bad url:" << url;
    return false;
  }
  int l = 0;
  if (j == std::string::npos) {
    DebugLog << "url only have path, url is" << url;
  } else {
    url = url.substr(j + 3, s2 - s1 - j - 4);
    DebugLog << "delete http prefix, url = " << url;
    j = url.find_first_of('/');
    l = url.length();
    if (j == std::string::npos || j == url.length() - 1) {
      DebugLog << "http request root path, and query is empty";
      return true;
    }
    url = url.substr(j + 1, l - j - 1);
  }

  l = url.length();
  j = url.find_first_of('?');
  if (j == std::string::npos) {
    requset->request_path_ = url;
    DebugLog << "http request path:" << requset->request_path_ << "and query is empty";
    return true;
  }
  requset->request_path_ = url.substr(0, j);
  requset->request_query_ = url.substr(j + 1, l - j - 1);
  DebugLog << "http request path:" << requset->request_path_ << ", and query:" << requset->request_query_;
  StringUtil::SplitStrToMap(requset->request_query_, "&", "=", requset->query_maps_);
  return true;
}

auto HttpCodeC::ParseHttpRequestHeader(HttpRequest *requset, const std::string &str) -> bool {
  if (str.empty() || str.length() < 4 || str == "\r\n\r\n") {
    return true;
  }
  const std::string& tmp = str;
  StringUtil::SplitStrToMap(tmp, "\r\n", ":", requset->requeset_header_.maps_);
  return true;
}
auto HttpCodeC::ParseHttpRequestContent(HttpRequest *requset, const std::string &str) -> bool {
  if (str.empty()) {
    return true;
  }
  requset->request_body_ = str;
  return true;
}

auto HttpCodeC::GetProtocalType() -> ProtocalType { return Http_Protocal; }

}  // namespace tirpc
