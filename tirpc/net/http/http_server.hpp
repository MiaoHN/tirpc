#pragma once

#include "tirpc/net/tcp/tcp_server.hpp"

namespace tirpc {

class HttpServer : public TcpServer {
 public:
  explicit HttpServer(NetAddress::ptr addr);

  auto RegisterHttpServlet(const std::string &url_path, HttpServlet::ptr servlet) -> bool;
};

}  // namespace tirpc