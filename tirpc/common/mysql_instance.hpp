#pragma once

#ifdef DECLARE_MYSQL_PLUGIN
#include <mysql/mysql.h>
#endif

#include <map>
#include <memory>
#include <utility>

#include "tirpc/common/mutex.hpp"
#include "tirpc/net/base/address.hpp"

namespace tirpc {

struct MySQLOption {
 public:
  explicit MySQLOption(IPAddress addr) : addr_(std::move(addr)) {};
  ~MySQLOption() = default;

 public:
  IPAddress addr_;
  std::string user_;
  std::string passwd_;
  std::string select_db_;
  std::string char_set_;
};

#ifdef DECLARE_MYSQL_PLUGIN
class MySQLThreadInit {
 public:
  MySQLThreadInit();

  ~MySQLThreadInit();
};

class MySQLInstase {
 public:
  typedef std::shared_ptr<MySQLInstase> ptr;

  MySQLInstase(const MySQLOption &option);

  ~MySQLInstase();

  bool isInitSuccess();

  int query(const std::string &sql);

  int commit();

  int begin();

  int rollBack();

  MYSQL_RES *storeResult();

  MYSQL_ROW fetchRow(MYSQL_RES *res);

  void freeResult(MYSQL_RES *res);

  long long numFields(MYSQL_RES *res);

  long long affectedRows();

  std::string getMySQLErrorInfo();

  int getMySQLErrno();

 private:
  int reconnect();

 private:
  MySQLOption option_;
  bool init_succ_{false};
  bool in_trans_{false};
  Mutex mutex_;
  MYSQL *sql_handler_{NULL};
};

class MySQLInstaseFactroy {
 public:
  MySQLInstaseFactroy() = default;

  ~MySQLInstaseFactroy() = default;

  MySQLInstase::ptr GetMySQLInstase(const std::string &key);

 public:
  static MySQLInstaseFactroy *GetThreadMySQLFactory();
};

#endif

}  // namespace tirpc
