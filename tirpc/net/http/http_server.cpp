#include "tirpc/net/http/http_server.hpp"
#include "tirpc/net/http/http_codec.hpp"
#include "tirpc/net/http/http_dispatcher.hpp"
#include "tirpc/net/tcp/tcp_server.hpp"

namespace tirpc {

HttpServer::HttpServer() : TcpServer() { Init(); }

HttpServer::HttpServer(Address::ptr addr) : TcpServer(addr) { Init(); }

void HttpServer::Init() {
  dispatcher_ = std::make_shared<HttpDispatcher>();
  codec_ = std::make_shared<HttpCodeC>();
  start_info_ = " View Page at: http://" + addr_->ToString() + "\n Press Ctrl+C to stop";
}

auto HttpServer::RegisterHttpServlet(const std::string &url_path, HttpServlet::ptr servlet) -> bool {
  if (servlet) {
    if (url_path.back() == '*') {
      dynamic_cast<HttpDispatcher *>(dispatcher_.get())->RegisterGlobServlet(url_path, servlet);
    } else {
      dynamic_cast<HttpDispatcher *>(dispatcher_.get())->RegisterServlet(url_path, servlet);
    }
  } else {
    LOG_ERROR << "register http servlet error, servlet ptr is nullptr";
    return false;
  }
  return true;
}

}  // namespace tirpc