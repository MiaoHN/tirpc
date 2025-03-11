#include <google/protobuf/service.h>
#include <atomic>
#include <future>
#include <memory>

#include "rpc_server.pb.h"

#include "tirpc/common/log.hpp"
#include "tirpc/common/start.hpp"
#include "tirpc/net/base/address.hpp"
#include "tirpc/net/http/http_define.hpp"
#include "tirpc/net/http/http_request.hpp"
#include "tirpc/net/http/http_response.hpp"
#include "tirpc/net/http/http_server.hpp"
#include "tirpc/net/http/http_servlet.hpp"
#include "tirpc/net/rpc/rpc_async_channel.hpp"
#include "tirpc/net/rpc/rpc_channel.hpp"
#include "tirpc/net/rpc/rpc_closure.hpp"
#include "tirpc/net/rpc/rpc_controller.hpp"

const char *html = "<html><body><h1>Welcome to tirPC, just enjoy it!</h1><p>%s</p></body></html>";

tirpc::IPAddress::ptr addr = std::make_shared<tirpc::IPAddress>("127.0.0.1", 39999);

class BlockCallHttpServlet : public tirpc::HttpServlet {
 public:
  BlockCallHttpServlet() = default;
  ~BlockCallHttpServlet() override = default;

  void HandleGet(tirpc::HttpRequest *req, tirpc::HttpResponse *res) override {
    SetHttpCode(res, tirpc::HTTP_OK);
    SetHttpContentType(res, "text/html;charset=utf-8");

    queryAgeReq rpc_req;
    queryAgeRes rpc_res;
    APP_LOG_DEBUG("now to call QueryServer tirPC server to query who's id is %s", req->query_maps_["id"].c_str());
    rpc_req.set_id(std::atoi(req->query_maps_["id"].c_str()));

    tirpc::RpcChannel channel(addr);
    QueryService_Stub stub(&channel);

    tirpc::RpcController rpc_controller;
    rpc_controller.SetTimeout(5000);

    APP_LOG_DEBUG("BlockCallHttpServlet end to call RPC");
    stub.query_age(&rpc_controller, &rpc_req, &rpc_res, nullptr);
    APP_LOG_DEBUG("BlockCallHttpServlet end to call RPC");

    if (rpc_controller.ErrorCode() != 0) {
      char buf[512];
      sprintf(buf, html, "failed to call QueryServer rpc server");
      SetHttpBody(res, std::string(buf));
      return;
    }

    if (rpc_res.ret_code() != 0) {
      std::stringstream ss;
      ss << "QueryServer rpc server return bad result, ret = " << rpc_res.ret_code()
         << ", and res_info = " << rpc_res.res_info();
      APP_LOG_DEBUG(ss.str().c_str());
      char buf[512];
      sprintf(buf, html, ss.str().c_str());
      SetHttpBody(res, std::string(buf));
      return;
    }

    std::stringstream ss;
    ss << "Success!! Your age is," << rpc_res.age() << " and Your id is " << rpc_res.id();

    char buf[512];
    sprintf(buf, html, ss.str().c_str());
    SetHttpBody(res, std::string(buf));
  }

  auto GetServletName() -> std::string override { return "BlockCallHttpServlet"; }
};

class NonBlockCallHttpServlet : public tirpc::HttpServlet {
 public:
  NonBlockCallHttpServlet() = default;
  ~NonBlockCallHttpServlet() override = default;

  void HandleGet(tirpc::HttpRequest *req, tirpc::HttpResponse *res) override {
    SetHttpCode(res, tirpc::HTTP_OK);
    SetHttpContentType(res, "text/html;charset=utf-8");

    auto rpc_req = std::make_shared<queryAgeReq>();
    auto rpc_res = std::make_shared<queryAgeRes>();
    APP_LOG_DEBUG("now to call QueryServer tirPC server to query who's id is %s", req->query_maps_["id"].c_str());
    rpc_req->set_id(std::atoi(req->query_maps_["id"].c_str()));

    auto rpc_controller = std::make_shared<tirpc::RpcController>();
    rpc_controller->SetTimeout(10000);

    APP_LOG_DEBUG("NonBlockCallHttpServlet begin to call RPC async");

    auto async_channel = std::make_shared<tirpc::RpcAsyncChannel>(addr);

    auto cb = [rpc_res]() {
      APP_LOG_DEBUG("NonBlockCallHttpServlet async call end, res=%s", rpc_res->ShortDebugString().c_str());
    };

    auto closure = std::make_shared<tirpc::RpcClosure>(cb);
    async_channel->SaveCallee(rpc_controller, rpc_req, rpc_res, closure);

    QueryService_Stub stub(async_channel.get());

    stub.query_age(rpc_controller.get(), rpc_req.get(), rpc_res.get(), nullptr);
    APP_LOG_DEBUG("NonBlockCallHttpServlet async end, now you can to some another thing");

    async_channel->Wait();
    APP_LOG_DEBUG("wait() back, now to check is rpc call succ");

    if (rpc_controller->ErrorCode() != 0) {
      APP_LOG_DEBUG("failed to call QueryServer rpc server");
      char buf[512];
      sprintf(buf, html, "failed to call QueryServer rpc server");
      SetHttpBody(res, std::string(buf));
      return;
    }

    if (rpc_res->ret_code() != 0) {
      std::stringstream ss;
      ss << "QueryServer rpc server return bad result, ret = " << rpc_res->ret_code()
         << ", and res_info = " << rpc_res->res_info();
      char buf[512];
      sprintf(buf, html, ss.str().c_str());
      SetHttpBody(res, std::string(buf));
      return;
    }

    std::stringstream ss;
    ss << "Success!! Your age is," << rpc_res->age() << " and Your id is " << rpc_res->id();

    char buf[512];
    sprintf(buf, html, ss.str().c_str());
    SetHttpBody(res, std::string(buf));
  }

  auto GetServletName() -> std::string override { return "NonBlockCallHttpServlet"; }
};

class QPSHttpServlet : public tirpc::HttpServlet {
 public:
  QPSHttpServlet() = default;
  ~QPSHttpServlet() override = default;

  void HandleGet(tirpc::HttpRequest *req, tirpc::HttpResponse *res) override {
    SetHttpCode(res, tirpc::HTTP_OK);
    SetHttpContentType(res, "text/html;charset=utf-8");

    std::stringstream ss;
    ss << "QPSHttpServlet Echo Success!! Your id is," << req->query_maps_["id"];
    char buf[512];
    sprintf(buf, html, ss.str().c_str());
    SetHttpBody(res, std::string(buf));
    APP_LOG_DEBUG(ss.str().c_str());
  }

  auto GetServletName() -> std::string override { return "QPSHttpServlet"; }
};

auto main(int argc, char *argv[]) -> int {
  // default config file
  std::string config_file = "./conf/http_server.yml";

  if (argc == 2) {
    config_file = argv[1];
  }

  tirpc::InitConfig(config_file.c_str());

  auto server = std::make_shared<tirpc::HttpServer>(tirpc::GetConfig()->GetAddr());

  server->RegisterHttpServlet("/qps", std::make_shared<QPSHttpServlet>());
  server->RegisterHttpServlet("/block", std::make_shared<BlockCallHttpServlet>());
  server->RegisterHttpServlet("/nonblock", std::make_shared<NonBlockCallHttpServlet>());

  tirpc::StartServer(server);

  return 0;
}
