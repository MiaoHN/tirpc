#include "tirpc/net/http/http_servlet.hpp"

#include <memory>
#include <unordered_map>

#include "tirpc/common/log.hpp"
#include "tirpc/net/http/http_define.hpp"
#include "tirpc/net/http/http_request.hpp"
#include "tirpc/net/http/http_response.hpp"

namespace tirpc {

extern const char *default_html_template;
extern std::string content_type_text;

HttpServlet::HttpServlet() = default;

HttpServlet::~HttpServlet() = default;

static auto GetParamsStr(const std::map<std::string, std::string> &query_map) -> std::string {
  std::stringstream ss;
  if (!query_map.empty()) {
    ss << ", params = { ";
    for (const auto &[k, v] : query_map) {
      ss << k + ": " + v + ", ";
    }
    ss << "}";
  }
  return ss.str();
}

void HttpServlet::Handle(HttpRequest *req, HttpResponse *res) {
  // handleNotFound();
  switch (req->request_method_) {
    case HttpMethod::GET: {
      LOG_INFO << "<GET> path = '" << req->request_path_ << "'" << GetParamsStr(req->query_maps_);
      if (req->request_version_ == "HTTP/1.0") {
        is_close = true;
      }
      HandleGet(req, res);
      break;
    }
    case HttpMethod::POST: {
      LOG_INFO << "<POST> path = '" << req->request_path_ << "'" << GetParamsStr(req->query_maps_);
      if (req->request_version_ == "HTTP/1.0") {
        is_close = true;
      }
      HandlePost(req, res);
      break;
    }
    default: {
      LOG_WARN << "http method not supported";
      break;
    }
  }
}

void HttpServlet::HandleGet(HttpRequest *req, HttpResponse *res) { LOG_WARN << "<GET> not implemented"; }

void HttpServlet::HandlePost(HttpRequest *req, HttpResponse *res) { LOG_WARN << "<POST> not implemented"; }

void HttpServlet::HandleNotFound(HttpRequest *req, HttpResponse *res) {
  LOG_DEBUG << "return 404 html";
  SetHttpCode(res, HTTP_NOTFOUND);
  char buf[512];
  sprintf(buf, default_html_template, std::to_string(HTTP_NOTFOUND).c_str(), HttpCodeToString(HTTP_NOTFOUND));
  res->response_body_ = std::string(buf);
  res->response_header_.maps_["Content-Type"] = content_type_text;
  res->response_header_.maps_["Content-Length"] = std::to_string(res->response_body_.length());
}

void HttpServlet::SetHttpCode(HttpResponse *res, const int code) {
  res->response_code_ = code;
  res->response_info_ = std::string(HttpCodeToString(code));
}

void HttpServlet::SetHttpContentType(HttpResponse *res, const std::string &content_type) {
  res->response_header_.maps_["Content-Type"] = content_type;
}

static auto GetContentType(const std::string &fileExtension) -> std::string {
  static std::unordered_map<std::string, std::string> contentTypes = {{".html", "text/html;charset=utf-8"},
                                                                      {".css", "text/css;charset=utf-8"},
                                                                      {".js", "application/javascript;charset=utf-8"},
                                                                      {".jpg", "image/jpeg"},
                                                                      {".png", "image/png"},
                                                                      {".gif", "image/gif"},
                                                                      {".json", "application/json;charset=utf-8"},
                                                                      {".xml", "application/xml;charset=utf-8"}};

  auto it = contentTypes.find(fileExtension);
  if (it != contentTypes.end()) {
    return it->second;
  }
  return "application/octet-stream";  // 默认类型
}

void HttpServlet::SetHttpContentTypeForFile(HttpResponse *res, const std::string &filepath) {
  // 提取文件后缀
  size_t dotPos = filepath.rfind('.');
  std::string fileExtension;
  if (dotPos != std::string::npos) {
    fileExtension = filepath.substr(dotPos);
  }

  // 获取 Content-Type
  std::string content_type = GetContentType(fileExtension);

  // 设置 Content-Type 到响应头
  SetHttpContentType(res, content_type);
}

void HttpServlet::SetHttpBody(HttpResponse *res, const std::string &body) {
  res->response_body_ = body;
  res->response_header_.maps_["Content-Length"] = std::to_string(res->response_body_.length());
}

void HttpServlet::SetCommParam(HttpRequest *req, HttpResponse *res) {
  LOG_DEBUG << "set response version=" << req->request_version_;
  res->response_version_ = req->request_version_;
  res->response_header_.maps_["Connection"] = req->requeset_header_.maps_["Connection"];
}

NotFoundHttpServlet::NotFoundHttpServlet() = default;

NotFoundHttpServlet::~NotFoundHttpServlet() = default;

void NotFoundHttpServlet::Handle(HttpRequest *req, HttpResponse *res) { HandleNotFound(req, res); }

auto NotFoundHttpServlet::GetServletName() -> std::string { return "NotFoundHttpServlet"; }

}  // namespace tirpc