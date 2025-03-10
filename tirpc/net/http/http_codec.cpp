#include "tirpc/net/http/http_codec.hpp"

#include <algorithm>
#include <sstream>
#include <string_view>

#include "tirpc/common/log.hpp"
#include "tirpc/common/string_util.hpp"
#include "tirpc/net/http/http_request.hpp"
#include "tirpc/net/http/http_response.hpp"
#include "tirpc/net/tcp/abstract_codec.hpp"
#include "tirpc/net/tcp/abstract_data.hpp"
#include "tirpc/net/tcp/tcp_buffer.hpp"

namespace tirpc {

HttpCodeC::HttpCodeC() = default;

HttpCodeC::~HttpCodeC() = default;

void HttpCodeC::Encode(TcpBuffer *buf, AbstractData *data) {
  LOG_DEBUG << "test encode";
  auto *response = dynamic_cast<HttpResponse *>(data);
  response->encode_succ_ = false;

  std::string http_res;
  http_res.reserve(256);  // 根据典型响应大小调整

  http_res.append(response->response_version_)
      .append(" ")
      .append(std::to_string(response->response_code_))
      .append(" ")
      .append(response->response_info_)
      .append("\r\n")
      .append(response->response_header_.ToHttpString())
      .append("\r\n")
      .append(response->response_body_);
  LOG_DEBUG << "encode http response is:  " << http_res;

  buf->WriteToBuffer(http_res.c_str(), http_res.length());
  LOG_DEBUG << "succ encode and write to buffer, writeindex=" << buf->WriteIndex();
  response->encode_succ_ = true;
  LOG_DEBUG << "test encode end";
}

void HttpCodeC::Decode(TcpBuffer *buf, AbstractData *data) {
  LOG_DEBUG << "test http decode start";

  // 输入检查
  if (buf == nullptr || data == nullptr) {
    LOG_ERROR << "decode error! buf or data nullptr";
    return;
  }

  HttpRequest *request = dynamic_cast<HttpRequest *>(data);
  if (request == nullptr) {
    LOG_ERROR << "not http request type";
    return;
  }

  std::string strs = buf->GetBufferString();
  const int totalLength = strs.length();
  std::string_view tmp(strs);
  int len = tmp.length();
  int readSize = 0;

  LOG_DEBUG << "pending to parse str: " << tmp << ", total size = " << totalLength;

  bool isParseRequestLine = false;
  bool isParseRequestHeader = false;
  bool isParseRequestContent = false;

  while (true) {
    if (!isParseRequestLine) {
      size_t i = tmp.find(g_crlf);
      if (i == std::string::npos) {
        LOG_DEBUG << "not found CRLF in buffer";
        return;
      }
      if (i == static_cast<size_t>(len) - 2) {
        LOG_DEBUG << "need to read more data";
        break;
      }
      isParseRequestLine = ParseHttpRequestLine(request, tmp.substr(0, i));
      if (!isParseRequestLine) {
        LOG_ERROR << "Failed to parse request line";
        return;
      }
      tmp = tmp.substr(i + 2);
      len = tmp.length();
      readSize += i + 2;
    }

    if (!isParseRequestHeader) {
      size_t j = tmp.find(g_crlf_double);
      if (j == std::string::npos) {
        LOG_DEBUG << "not found CRLF CRLF in buffer";
        return;
      }
      isParseRequestHeader = ParseHttpRequestHeader(request, tmp.substr(0, j));
      if (!isParseRequestHeader) {
        LOG_ERROR << "Failed to parse request header";
        return;
      }
      tmp = tmp.substr(j + 4);
      len = tmp.length();
      readSize += j + 4;
    }

    if (!isParseRequestContent) {
      int contentLen = std::atoi(request->requeset_header_.maps_["Content-Length"].c_str());
      if (totalLength - readSize < contentLen) {
        LOG_DEBUG << "need to read more data";
        return;
      }
      if (request->request_method_ == POST && contentLen != 0) {
        isParseRequestContent = ParseHttpRequestContent(request, tmp.substr(0, contentLen));
        if (!isParseRequestContent) {
          LOG_ERROR << "Failed to parse request content";
          return;
        }
        readSize += contentLen;
      } else {
        isParseRequestContent = true;
      }
    }

    if (isParseRequestLine && isParseRequestHeader && isParseRequestContent) {
      LOG_DEBUG << "parse http request success, read size is " << readSize << " bytes";
      buf->RecycleRead(readSize);
      break;
    }
  }

  request->decode_succ_ = true;
  data = request;

  LOG_DEBUG << "test http decode end";
}

auto HttpCodeC::GenDataPtr() -> AbstractData::ptr { return std::make_shared<HttpRequest>(); }

auto HttpCodeC::ParseHttpRequestLine(HttpRequest *requset, std::string_view tmp) -> bool {
  size_t s1 = tmp.find_first_of(' ');
  size_t s2 = tmp.find_last_of(' ');

  if (s1 == std::string::npos || s2 == std::string::npos || s1 == s2) {
    LOG_ERROR << "error read Http Requser Line, space is not 2";
    return false;
  }
  std::string method(tmp.substr(0, s1));
  std::transform(method.begin(), method.end(), method.begin(), toupper);
  if (method == "GET") {
    requset->request_method_ = HttpMethod::GET;
  } else if (method == "POST") {
    requset->request_method_ = HttpMethod::POST;
  } else {
    LOG_ERROR << "parse http request request line error, not support http method:" << method;
    return false;
  }

  std::string version(tmp.substr(s2 + 1, tmp.length() - s2 - 1));
  std::transform(version.begin(), version.end(), version.begin(), toupper);
  if (version != "HTTP/1.1" && version != "HTTP/1.0") {
    LOG_ERROR << "parse http request request line error, not support http version:" << version;
    return false;
  }
  requset->request_version_ = version;

  std::string url(tmp.substr(s1 + 1, s2 - s1 - 1));
  size_t j = url.find("://");

  if (j != std::string::npos && j + 3 >= url.length()) {
    LOG_ERROR << "parse http request request line error, bad url:" << url;
    return false;
  }
  int l = 0;
  if (j == std::string::npos) {
    LOG_DEBUG << "url only have path, url is" << url;
  } else {
    url = url.substr(j + 3, s2 - s1 - j - 4);
    LOG_DEBUG << "delete http prefix, url = " << url;
    j = url.find_first_of('/');
    l = url.length();
    if (j == std::string::npos || j == url.length() - 1) {
      LOG_DEBUG << "http request root path, and query is empty";
      return true;
    }
    url = url.substr(j + 1, l - j - 1);
  }

  l = url.length();
  j = url.find_first_of('?');
  if (j == std::string::npos) {
    requset->request_path_ = url;
    LOG_DEBUG << "http request path:" << requset->request_path_ << "and query is empty";
    return true;
  }
  requset->request_path_ = url.substr(0, j);
  requset->request_query_ = url.substr(j + 1, l - j - 1);
  LOG_DEBUG << "http request path:" << requset->request_path_ << ", and query:" << requset->request_query_;
  StringUtil::SplitStrToMap(requset->request_query_, "&", "=", requset->query_maps_);
  return true;
}

auto HttpCodeC::ParseHttpRequestHeader(HttpRequest *requset, std::string_view str) -> bool {
  if (str.empty() || str.length() < 4 || str == "\r\n\r\n") {
    return true;
  }
  StringUtil::SplitStrToMap(str, "\r\n", ":", requset->requeset_header_.maps_);
  return true;
}
auto HttpCodeC::ParseHttpRequestContent(HttpRequest *requset, std::string_view str) -> bool {
  if (str.empty()) {
    return true;
  }
  requset->request_body_ = str;
  return true;
}

}  // namespace tirpc
