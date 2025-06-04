#include "tirpc/net/http/http_server.hpp"
#include "tirpc/common/config.hpp"
#include "tirpc/common/start.hpp"
#include "tirpc/common/util.hpp"

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
      LOG_ERROR << "File '" << filepath << "' not found";
      HandleNotFound(req, res);
    }
  }

  auto GetServletName() -> std::string override { return "FileHttpServlet"; }

 private:
  std::string root_dir_;
};

auto main(int argc, char *argv[]) -> int {
  // default config file
  std::string config_file = "./conf/http_server.yml";

  if (argc == 2) {
    config_file = argv[1];
  }

  tirpc::Config::LoadFromFile(config_file);

  auto server = std::make_shared<tirpc::HttpServer>();

  server->RegisterHttpServlet("/*", std::make_shared<FileHttpServlet>("./root/"));

  tirpc::StartServer(server);

  return 0;
}
