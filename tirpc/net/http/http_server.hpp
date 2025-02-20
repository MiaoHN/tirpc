#pragma once

#include "tirpc/net/tcp/tcp_server.hpp"

namespace tirpc {

class HttpServer : public TcpServer {
 public:
  explicit HttpServer(Address::ptr addr);

  auto RegisterHttpServlet(const std::string &url_path, HttpServlet::ptr servlet) -> bool;

  auto RegisterGlobHttpServlet(const std::string &url_path, HttpServlet::ptr servlet) -> bool;
};

}  // namespace tirpc