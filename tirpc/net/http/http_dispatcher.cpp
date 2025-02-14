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
  auto *resquest = dynamic_cast<HttpRequest *>(data);
  HttpResponse response;
  Coroutine::GetCurrentCoroutine()->GetRuntime()->msg_no_ = MsgReqUtil::GenMsgNumber();
  SetCurrentRuntime(Coroutine::GetCurrentCoroutine()->GetRuntime());

  DebugLog << "begin to dispatch client http request, msgno="
           << Coroutine::GetCurrentCoroutine()->GetRuntime()->msg_no_;

  std::string url_path = resquest->request_path_;
  if (!url_path.empty()) {
    auto it = servlets_.find(url_path);
    if (it == servlets_.end()) {
      auto it = globs_.begin();
      for (; it != globs_.end(); ++it) {
        if (!fnmatch(it->first.c_str(), url_path.c_str(), 0)) {
          break;
        }
      }
      if (it != globs_.end()) {
        Coroutine::GetCurrentCoroutine()->GetRuntime()->interface_name_ = it->second->GetServletName();
        it->second->SetCommParam(resquest, &response);
        it->second->Handle(resquest, &response);
      } else {
        ErrorLog << "No matched servlet for url path '" << url_path << "'";
        NotFoundHttpServlet servlet;
        Coroutine::GetCurrentCoroutine()->GetRuntime()->interface_name_ = servlet.GetServletName();
        servlet.SetCommParam(resquest, &response);
        servlet.Handle(resquest, &response);
      }
    } else {
      Coroutine::GetCurrentCoroutine()->GetRuntime()->interface_name_ = it->second->GetServletName();
      it->second->SetCommParam(resquest, &response);
      it->second->Handle(resquest, &response);
    }
  }

  conn->GetCodec()->Encode(conn->GetOutBuffer(), &response);

  DebugLog << "end dispatch client http request, msgno=" << Coroutine::GetCurrentCoroutine()->GetRuntime()->msg_no_;
}

void HttpDispatcher::RegisterServlet(const std::string &path, HttpServlet::ptr servlet) {
  auto it = servlets_.find(path);
  if (it == servlets_.end()) {
    DebugLog << "register servlet success to path {" << path << "}";
    servlets_[path] = std::move(servlet);
  } else {
    ErrorLog << "failed to register, beacuse path {" << path << "} has already register sertlet";
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