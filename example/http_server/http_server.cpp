#include <atomic>
#include <fstream>
#include <future>
#include <memory>
#include <sstream>

#include "tirpc/common/log.hpp"
#include "tirpc/common/start.hpp"
#include "tirpc/common/string_util.hpp"
#include "tirpc/coroutine/coroutine_hook.hpp"
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

class FileHttpServlet : public tirpc::HttpServlet {
 public:
  FileHttpServlet(const std::string &root_dir) : root_dir_(root_dir) {}

  ~FileHttpServlet() override = default;

  void HandleGet(tirpc::HttpRequest *req, tirpc::HttpResponse *res) override {
    std::string filepath = root_dir_ + req->request_path_;
    std::string content;

    if (tirpc::FileUtil::IsDirectory(filepath)) {
      filepath += "/index.html";
    }

    if (tirpc::FileUtil::ReadFile(filepath, content)) {
      SetHttpCode(res, 200);
      SetHttpContentTypeForFile(res, filepath);
      SetHttpBody(res, content);
    } else {
      // 文件未找到，返回 404
      ErrorLog << "File '" << filepath << "' not found";
      HandleNotFound(req, res);
    }
  }

  auto GetServletName() -> std::string override { return "FileHttpServlet"; }

 private:
  std::string root_dir_;
};

auto main(int argc, char *argv[]) -> int {
  // default config file
  std::string config_file = "./conf/http_server.xml";

  if (argc == 2) {
    config_file = argv[1];
  }

  tirpc::InitConfig(config_file.c_str());

  auto server = std::make_shared<tirpc::HttpServer>(tirpc::GetConfig()->GetAddr());

  server->RegisterHttpServlet("/*", std::make_shared<FileHttpServlet>("./root/"));

  tirpc::StartServer(server);

  return 0;
}
