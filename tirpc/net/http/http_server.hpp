#pragma once

#include "tirpc/net/http/http_servlet.hpp"
#include "tirpc/net/tcp/tcp_server.hpp"

namespace tirpc {

class HttpServer : public TcpServer {
 public:
  HttpServer();

  HttpServer(Address::ptr addr);

  void Init();

  auto RegisterHttpServlet(const std::string &url_path, HttpServlet::ptr servlet) -> bool;

  auto RegisterGlobHttpServlet(const std::string &url_path, HttpServlet::ptr servlet) -> bool;
};

}  // namespace tirpc