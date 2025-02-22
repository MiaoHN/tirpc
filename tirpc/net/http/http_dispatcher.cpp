#include "tirpc/net/http/http_dispatcher.hpp"

#include <fnmatch.h>
#include <google/protobuf/service.h>
#include <memory>
#include <utility>

#include "tirpc/common/log.hpp"
#include "tirpc/common/msg_req.hpp"
#include "tirpc/net/http/http_request.hpp"
#include "tirpc/net/http/http_servlet.hpp"
#include "tirpc/net/tcp/tcp_connection.hpp"

namespace tirpc {

void HttpDispatcher::Dispatch(AbstractData *data, TcpConnection *conn) {
  auto *request = dynamic_cast<HttpRequest *>(data);
  if (!request) {
    LOG_ERROR << "Failed to cast AbstractData to HttpRequest";
    return;
  }

  HttpResponse response;
  auto runtime = Coroutine::GetCurrentCoroutine()->GetRuntime();
  runtime->msg_no_ = MsgReqUtil::GenMsgNumber();
  SetCurrentRuntime(runtime);

  LOG_DEBUG << "begin to dispatch client http request, msgno=" << runtime->msg_no_;

  std::string url_path = request->request_path_;
  if (url_path.empty()) {
    LOG_ERROR << "Empty URL path in HTTP request";
    return;
  }

  auto it = servlets_.find(url_path);
  HttpServlet::ptr servlet = nullptr;

  if (it != servlets_.end()) {
    servlet = it->second;
  } else {
    // servlets_ 未找到，在 globs_ 中找最匹配的
    auto glob_it = globs_.begin();
    for (; glob_it != globs_.end(); ++glob_it) {
      if (!fnmatch(glob_it->first.c_str(), url_path.c_str(), 0)) {
        servlet = glob_it->second;
        break;
      }
    }

    if (!servlet) {
      // 如果都未找到，使用 NotFoundHttpServlet
      servlet = std::make_shared<NotFoundHttpServlet>();
    }
  }

  // 设置接口名称并处理请求
  runtime->interface_name_ = servlet->GetServletName();
  servlet->SetCommParam(request, &response);
  servlet->Handle(request, &response);

  if (servlet->is_close) {
    conn->ShutdownConnection();
  }

  conn->GetCodec()->Encode(conn->GetOutBuffer(), &response);

  LOG_DEBUG << "end dispatch client http request, msgno=" << runtime->msg_no_;
}

void HttpDispatcher::RegisterServlet(const std::string &path, HttpServlet::ptr servlet) {
  auto it = servlets_.find(path);
  if (it == servlets_.end()) {
    LOG_DEBUG << "register servlet success to path {" << path << "}";
    servlets_[path] = std::move(servlet);
  } else {
    LOG_ERROR << "failed to register, beacuse path {" << path << "} has already register sertlet";
  }
}

void HttpDispatcher::RegisterGlobServlet(const std::string &path, HttpServlet::ptr servlet) {
  for (auto it = globs_.begin(); it != globs_.end(); ++it) {
    if (it->first == path) {
      globs_.erase(it);
      break;
    }
  }
  globs_.push_back(std::make_pair(path, servlet));
}

}  // namespace tirpc