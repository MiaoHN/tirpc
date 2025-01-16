#include "tirpc/net/http/http_dispatcher.hpp"

#include <google/protobuf/service.h>
#include <memory>
#include <utility>

#include "tirpc/common/log.hpp"
#include "tirpc/common/msg_req.hpp"
#include "tirpc/net/http/http_request.hpp"
#include "tirpc/net/http/http_servlet.hpp"
#include "tirpc/net/tcp/tcp_connection.hpp"

namespace tirpc {

void HttpDispacther::Dispatch(AbstractData *data, TcpConnection *conn) {
  auto *resquest = dynamic_cast<HttpRequest *>(data);
  HttpResponse response;
  Coroutine::GetCurrentCoroutine()->GetRuntime()->msg_no_ = MsgReqUtil::GenMsgNumber();
  SetCurrentRuntime(Coroutine::GetCurrentCoroutine()->GetRuntime());

  InfoLog << "begin to dispatch client http request, msgno=" << Coroutine::GetCurrentCoroutine()->GetRuntime()->msg_no_;

  std::string url_path = resquest->request_path_;
  if (!url_path.empty()) {
    auto it = servlets_.find(url_path);
    if (it == servlets_.end()) {
      ErrorLog << "404, url path{ " << url_path
               << "}, msgno=" << Coroutine::GetCurrentCoroutine()->GetRuntime()->msg_no_;
      NotFoundHttpServlet servlet;
      Coroutine::GetCurrentCoroutine()->GetRuntime()->interface_name_ = servlet.GetServletName();
      servlet.SetCommParam(resquest, &response);
      servlet.Handle(resquest, &response);
    } else {
      Coroutine::GetCurrentCoroutine()->GetRuntime()->interface_name_ = it->second->GetServletName();
      it->second->SetCommParam(resquest, &response);
      it->second->Handle(resquest, &response);
    }
  }

  conn->GetCodec()->Encode(conn->GetOutBuffer(), &response);

  InfoLog << "end dispatch client http request, msgno=" << Coroutine::GetCurrentCoroutine()->GetRuntime()->msg_no_;
}

void HttpDispacther::RegisterServlet(const std::string &path, HttpServlet::ptr servlet) {
  auto it = servlets_.find(path);
  if (it == servlets_.end()) {
    DebugLog << "register servlet success to path {" << path << "}";
    servlets_[path] = std::move(servlet);
  } else {
    ErrorLog << "failed to register, beacuse path {" << path << "} has already register sertlet";
  }
}

}  // namespace tirpc