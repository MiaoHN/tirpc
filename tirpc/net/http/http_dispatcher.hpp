#pragma once

#include <map>
#include <memory>

#include "tirpc/net/tcp/abstract_dispatcher.hpp"
#include "tirpc/net/http/http_servlet.hpp"

namespace tirpc {

class HttpDispacther : public AbstractDispatcher {
 public:
  HttpDispacther() = default;

  ~HttpDispacther() override = default;

  void Dispatch(AbstractData *data, TcpConnection *conn) override;

  void RegisterServlet(const std::string &path, HttpServlet::ptr servlet);

 public:
  std::map<std::string, HttpServlet::ptr> servlets_;
};

}  // namespace tirpc
