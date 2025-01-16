#include "tirpc/net/http/http_servlet.hpp"

#include <memory>

#include "tirpc/common/log.hpp"
#include "tirpc/net/http/http_define.hpp"
#include "tirpc/net/http/http_request.hpp"
#include "tirpc/net/http/http_response.hpp"

namespace tirpc {

extern const char *default_html_template;
extern std::string content_type_text;

HttpServlet::HttpServlet() = default;

HttpServlet::~HttpServlet() = default;

void HttpServlet::Handle(HttpRequest *req, HttpResponse *res) {
  // handleNotFound();
}

void HttpServlet::HandleNotFound(HttpRequest *req, HttpResponse *res) {
  DebugLog << "return 404 html";
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

void HttpServlet::SetHttpBody(HttpResponse *res, const std::string &body) {
  res->response_body_ = body;
  res->response_header_.maps_["Content-Length"] = std::to_string(res->response_body_.length());
}

void HttpServlet::SetCommParam(HttpRequest *req, HttpResponse *res) {
  DebugLog << "set response version=" << req->request_version_;
  res->response_version_ = req->request_version_;
  res->response_header_.maps_["Connection"] = req->requeset_header_.maps_["Connection"];
}

NotFoundHttpServlet::NotFoundHttpServlet() = default;

NotFoundHttpServlet::~NotFoundHttpServlet() = default;

void NotFoundHttpServlet::Handle(HttpRequest *req, HttpResponse *res) { HandleNotFound(req, res); }

auto NotFoundHttpServlet::GetServletName() -> std::string { return "NotFoundHttpServlet"; }

}  // namespace tirpc