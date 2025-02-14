#pragma once

#include <map>
#include <memory>

#include "tirpc/net/http/http_servlet.hpp"
#include "tirpc/net/tcp/abstract_dispatcher.hpp"

namespace tirpc {

class HttpDispatcher : public AbstractDispatcher {
 public:
  HttpDispatcher() = default;

  ~HttpDispatcher() override = default;

  void Dispatch(AbstractData *data, TcpConnection *conn) override;

  void RegisterServlet(const std::string &path, HttpServlet::ptr servlet);

  void RegisterGlobServlet(const std::string &path, HttpServlet::ptr servlet);

 public:
  std::map<std::string, HttpServlet::ptr> servlets_;

  std::vector<std::pair<std::string, HttpServlet::ptr>> globs_;
};

}  // namespace tirpc
