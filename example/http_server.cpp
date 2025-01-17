#include <google/protobuf/service.h>
#include <atomic>
#include <future>

#include "tinypb_server.pb.h"

#include "tirpc/common/log.hpp"
#include "tirpc/common/start.hpp"
#include "tirpc/net/address.hpp"
#include "tirpc/net/http/http_define.hpp"
#include "tirpc/net/http/http_request.hpp"
#include "tirpc/net/http/http_response.hpp"
#include "tirpc/net/http/http_servlet.hpp"
#include "tirpc/net/tinypb/tinypb_rpc_async_channel.hpp"
#include "tirpc/net/tinypb/tinypb_rpc_channel.hpp"
#include "tirpc/net/tinypb/tinypb_rpc_closure.hpp"
#include "tirpc/net/tinypb/tinypb_rpc_controller.hpp"

const char *html = "<html><body><h1>Welcome to tirPC, just enjoy it!</h1><p>%s</p></body></html>";

tirpc::IPAddress::ptr addr = std::make_shared<tirpc::IPAddress>("127.0.0.1", 20000);

class BlockCallHttpServlet : public tirpc::HttpServlet {
 public:
  BlockCallHttpServlet() = default;
  ~BlockCallHttpServlet() override = default;

  void Handle(tirpc::HttpRequest *req, tirpc::HttpResponse *res) override {
    AppDebugLog("BlockCallHttpServlet get request");
    AppDebugLog("BlockCallHttpServlet success recive http request, now to get http response");
    SetHttpCode(res, tirpc::HTTP_OK);
    SetHttpContentType(res, "text/html;charset=utf-8");

    queryAgeReq rpc_req;
    queryAgeRes rpc_res;
    AppDebugLog("now to call QueryServer tirPC server to query who's id is %s", req->query_maps_["id"].c_str());
    rpc_req.set_id(std::atoi(req->query_maps_["id"].c_str()));

    tirpc::TinyPbRpcChannel channel(addr);
    QueryService_Stub stub(&channel);

    tirpc::TinyPbRpcController rpc_controller;
    rpc_controller.SetTimeout(5000);

    AppDebugLog("BlockCallHttpServlet end to call RPC");
    stub.query_age(&rpc_controller, &rpc_req, &rpc_res, nullptr);
    AppDebugLog("BlockCallHttpServlet end to call RPC");

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
      AppDebugLog(ss.str().c_str());
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

  void Handle(tirpc::HttpRequest *req, tirpc::HttpResponse *res) override {
    AppInfoLog("NonBlockCallHttpServlet get request");
    AppDebugLog("NonBlockCallHttpServlet success recive http request, now to get http response");
    SetHttpCode(res, tirpc::HTTP_OK);
    SetHttpContentType(res, "text/html;charset=utf-8");

    std::shared_ptr<queryAgeReq> rpc_req = std::make_shared<queryAgeReq>();
    std::shared_ptr<queryAgeRes> rpc_res = std::make_shared<queryAgeRes>();
    AppDebugLog("now to call QueryServer tirPC server to query who's id is %s", req->query_maps_["id"].c_str());
    rpc_req->set_id(std::atoi(req->query_maps_["id"].c_str()));

    std::shared_ptr<tirpc::TinyPbRpcController> rpc_controller = std::make_shared<tirpc::TinyPbRpcController>();
    rpc_controller->SetTimeout(10000);

    AppDebugLog("NonBlockCallHttpServlet begin to call RPC async");

    tirpc::TinyPbRpcAsyncChannel::ptr async_channel = std::make_shared<tirpc::TinyPbRpcAsyncChannel>(addr);

    auto cb = [rpc_res]() {
      printf("call succ, res = %s\n", rpc_res->ShortDebugString().c_str());
      AppDebugLog("NonBlockCallHttpServlet async call end, res=%s", rpc_res->ShortDebugString().c_str());
    };

    std::shared_ptr<tirpc::TinyPbRpcClosure> closure = std::make_shared<tirpc::TinyPbRpcClosure>(cb);
    async_channel->SaveCallee(rpc_controller, rpc_req, rpc_res, closure);

    QueryService_Stub stub(async_channel.get());

    stub.query_age(rpc_controller.get(), rpc_req.get(), rpc_res.get(), nullptr);
    AppDebugLog("NonBlockCallHttpServlet async end, now you can to some another thing");

    async_channel->Wait();
    AppDebugLog("wait() back, now to check is rpc call succ");

    if (rpc_controller->ErrorCode() != 0) {
      AppDebugLog("failed to call QueryServer rpc server");
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

  void Handle(tirpc::HttpRequest *req, tirpc::HttpResponse *res) override {
    AppDebugLog("QPSHttpServlet get request");
    SetHttpCode(res, tirpc::HTTP_OK);
    SetHttpContentType(res, "text/html;charset=utf-8");

    std::stringstream ss;
    ss << "QPSHttpServlet Echo Success!! Your id is," << req->query_maps_["id"];
    char buf[512];
    sprintf(buf, html, ss.str().c_str());
    SetHttpBody(res, std::string(buf));
    AppDebugLog(ss.str().c_str());
  }

  auto GetServletName() -> std::string override { return "QPSHttpServlet"; }
};

auto main(int argc, char *argv[]) -> int {
  // default config file
  std::string config_file = "./config.xml";

  if (argc == 2) {
    config_file = argv[1];
  }

  tirpc::InitConfig(config_file.c_str());

  AppInfoLog("use config file %s", config_file.c_str());

  REGISTER_HTTP_SERVLET("/qps", QPSHttpServlet);

  REGISTER_HTTP_SERVLET("/block", BlockCallHttpServlet);
  REGISTER_HTTP_SERVLET("/nonblock", NonBlockCallHttpServlet);

  tirpc::StartRpcServer();
  return 0;
}
