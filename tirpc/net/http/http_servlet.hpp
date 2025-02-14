#pragma once

#include <memory>

#include "tirpc/net/http/http_request.hpp"
#include "tirpc/net/http/http_response.hpp"

namespace tirpc {

class HttpServlet : public std::enable_shared_from_this<HttpServlet> {
 public:
  using ptr = std::shared_ptr<HttpServlet>;

  HttpServlet();

  virtual ~HttpServlet();

  virtual void Handle(HttpRequest *req, HttpResponse *res);

  virtual void HandleGet(HttpRequest *req, HttpResponse *res);

  virtual void HandlePost(HttpRequest *req, HttpResponse *res);

  virtual auto GetServletName() -> std::string = 0;

  void HandleNotFound(HttpRequest *req, HttpResponse *res);

  void SetHttpCode(HttpResponse *res, int code);

  void SetHttpContentType(HttpResponse *res, const std::string &content_type);

  void SetHttpContentTypeForFile(HttpResponse *res, const std::string &filepath);

  void SetHttpBody(HttpResponse *res, const std::string &body);

  void SetCommParam(HttpRequest *req, HttpResponse *res);
};

class NotFoundHttpServlet : public HttpServlet {
 public:
  NotFoundHttpServlet();

  ~NotFoundHttpServlet() override;

  void Handle(HttpRequest *req, HttpResponse *res) override;

  auto GetServletName() -> std::string override;
};

}  // namespace tirpc
