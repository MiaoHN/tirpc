#include "tirpc/common/string_util.hpp"

#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fstream>
#include <iostream>
#include <map>
#include <string>

#include "tirpc/common/log.hpp"
#include "tirpc/coroutine/coroutine_hook.hpp"

extern read_fun_ptr_t g_sys_read_fun;  // sys read func

namespace tirpc {

void StringUtil::SplitStrToMap(const std::string &str, const std::string &split_str, const std::string &joiner,
                               std::map<std::string, std::string> &res) {
  if (str.empty() || split_str.empty() || joiner.empty()) {
    LOG_DEBUG << "str or split_str or joiner_str is empty";
    return;
  }
  const std::string &tmp = str;

  std::vector<std::string> vec;
  SplitStrToVector(tmp, split_str, vec);
  for (const auto &i : vec) {
    if (!i.empty()) {
      size_t j = i.find_first_of(joiner);
      if (j != std::basic_string<char>::npos && j != 0) {
        std::string key = i.substr(0, j);
        std::string value = i.substr(j + joiner.length(), i.length() - j - joiner.length());
        LOG_DEBUG << "insert key=" << key << ", value=" << value;
        res[key.c_str()] = value;
      }
    }
  }
}

void StringUtil::SplitStrToVector(const std::string &str, const std::string &split_str, std::vector<std::string> &res) {
  if (str.empty() || split_str.empty()) {
    // LOG_DEBUG << "str or split_str is empty";
    return;
  }
  std::string tmp = str;
  if (tmp.substr(tmp.length() - split_str.length(), split_str.length()) != split_str) {
    tmp += split_str;
  }

  while (true) {
    size_t i = tmp.find_first_of(split_str);
    if (i == std::string::npos) {
      return;
    }
    int l = tmp.length();
    std::string x = tmp.substr(0, i);
    tmp = tmp.substr(i + split_str.length(), l - i - split_str.length());
    if (!x.empty()) {
      res.push_back(std::move(x));
    }
  }
}

auto FileUtil::IsDirectory(const std::string &path) -> bool {
  struct stat pathStat;
  // 获取文件状态信息
  if (stat(path.c_str(), &pathStat) == 0) {
    // 判断是否为目录
    return S_ISDIR(pathStat.st_mode);
  }
  return false;
}

bool FileUtil::ReadFile(const std::string &filepath, std::string &content) {
  if (IsDirectory(filepath)) {
    LOG_ERROR << "Directory '" << filepath << "' cannot be read as file!";
    return false;
  }
  // 以只读模式打开文件
  int fd = open(filepath.c_str(), O_RDONLY);
  if (fd == -1) {
    // 打开文件失败，输出错误信息
    LOG_ERROR << "Failed to open file '" << filepath << "'";
    return false;
  }

  // 获取文件大小
  struct stat file_stat;
  if (fstat(fd, &file_stat) == -1) {
    // 获取文件状态失败，关闭文件并返回 false
    LOG_ERROR << "Failed to get file status for '" << filepath << "'";
    close(fd);
    return false;
  }
  size_t file_size = file_stat.st_size;

  // 调整字符串大小以容纳文件内容
  content.resize(file_size);

  // 读取文件内容
  ssize_t bytes_read = g_sys_read_fun(fd, &content[0], file_size);
  if (bytes_read == -1) {
    // 读取文件失败，关闭文件并返回 false
    LOG_ERROR << "Failed to read file '" << filepath << "'";
    close(fd);
    return false;
  }

  // 关闭文件
  close(fd);

  // 如果读取的字节数不等于文件大小，说明读取不完整，返回 false
  if (static_cast<size_t>(bytes_read) != file_size) {
    return false;
  }

  return true;
}

}  // namespace tirpc