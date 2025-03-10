#pragma once

#include <map>
#include <string>
#include <string_view>
#include <vector>

namespace tirpc {

class StringUtil {
 public:
  // split a string to map
  // for example:  str is a=1&tt=2&cc=3  split_str = '&' joiner='='
  // get res is {"a":"1", "tt":"2", "cc", "3"}
  static void SplitStrToMap(std::string_view str, const std::string &split_str, const std::string &joiner,
                            std::map<std::string, std::string> &res);

  // split a string to vector
  // for example:  str is a=1&tt=2&cc=3  split_str = '&'
  // get res is {"a=1", "tt=2", "cc=3"}
  static void SplitStrToVector(std::string_view str, const std::string &split_str, std::vector<std::string> &res);
};

class FileUtil {
 public:
  static auto IsDirectory(const std::string &path) -> bool;

  static auto ReadFile(const std::string &filepath, std::string &content) -> bool;
};

}  // namespace tirpc