#include "tirpc/common/mysql_instance.hpp"

#ifdef DECLARE_MYSQL_PLUGIN
#include <mysql/errmsg.h>
#include <mysql/mysql.h>
#endif

#include "tirpc/common/config.hpp"
#include "tirpc/common/log.hpp"
#include "tirpc/coroutine/coroutine_hook.hpp"

namespace tirpc {

#ifdef DECLARE_MYSQL_PLUGIN

static thread_local MySQLInstaseFactroy *t_mysql_factory = NULL;

MySQLThreadInit::MySQLThreadInit() {
  LOG_DEBUG << "<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<< call mysql_thread_init";
  mysql_thread_init();
}

MySQLThreadInit::~MySQLThreadInit() {
  mysql_thread_end();
  LOG_DEBUG << ">>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>> call mysql_thread_end";
}

MySQLInstaseFactroy *MySQLInstaseFactroy::GetThreadMySQLFactory() {
  if (t_mysql_factory) {
    return t_mysql_factory;
  }
  t_mysql_factory = new MySQLInstaseFactroy();
  return t_mysql_factory;
}

MySQLInstase::ptr MySQLInstaseFactroy::GetMySQLInstase(const std::string &key) {
  auto it2 = g_rpc_config->mysql_options_.find(key);
  if (it2 == g_rpc_config->mysql_options_.end()) {
    LOG_ERROR << "get MySQLInstase error, not this key[" << key << "] exist";
    return NULL;
  }
  LOG_DEBUG << "create MySQLInstase of key " << key;
  MySQLInstase::ptr instase = std::make_shared<MySQLInstase>(it2->second);
  return instase;
}

MySQLInstase::MySQLInstase(const MySQLOption &option) : option_(option) {
  int ret = reconnect();
  if (ret != 0) {
    return;
  }

  init_succ_ = true;
}

int MySQLInstase::reconnect() {
  // this static value only call once
  // it will call mysql_thread_init when first call MySQLInstase::reconnect function
  // and it will call mysql_thread_end when current thread destroy
  static thread_local MySQLThreadInit t_mysql_thread_init;

  if (sql_handler_) {
    mysql_close(sql_handler_);
    sql_handler_ = NULL;
  }

  Mutex::Lock lock(mutex_);
  sql_handler_ = mysql_init(NULL);
  // LOG_DEBUG << "mysql fd is " << sql_handler_.net.fd;
  lock.unlock();
  if (!sql_handler_) {
    LOG_ERROR << "faild to call mysql_init allocate MYSQL instase";
    return -1;
  }
  // int value = 0;
  // mysql_options(sql_handler_, MYSQL_OPT_RECONNECT, &value);
  if (!option_.char_set_.empty()) {
    mysql_options(sql_handler_, MYSQL_SET_CHARSET_NAME, option_.char_set_.c_str());
  }
  LOG_DEBUG << "begin to connect mysql{ip:" << option_.addr_.getIP() << ", port:" << option_.addr_.getPort()
            << ", user:" << option_.user_ << ", passwd:" << option_.passwd_ << ", select_db: " << option_.select_db_
            << "charset:" << option_.char_set_ << "}";
  // mysql_real_connect(sql_handler_, option_.addr_.getIP().c_str(), option_.user_.c_str(),
  //     option_.passwd_.c_str(), option_.select_db_.c_str(), option_.addr_.getPort(), NULL, 0);
  if (!mysql_real_connect(sql_handler_, option_.addr_.getIP().c_str(), option_.user_.c_str(), option_.passwd_.c_str(),
                          option_.select_db_.c_str(), option_.addr_.getPort(), NULL, 0)) {
    LOG_ERROR << "faild to call mysql_real_connect, peer addr[ " << option_.addr_.getIP() << ":"
              << option_.addr_.getPort() << "], mysql sys errinfo[" << mysql_error(sql_handler_) << "]";
    return -1;
  }
  LOG_DEBUG << "mysql_handler connect succ";
  return 0;
}

bool MySQLInstase::isInitSuccess() { return init_succ_; }

MySQLInstase::~MySQLInstase() {
  if (sql_handler_) {
    mysql_close(sql_handler_);
    sql_handler_ = NULL;
  }
}

int MySQLInstase::commit() {
  int rt = query("COMMIT;");
  if (rt == 0) {
    in_trans_ = false;
  }
  return rt;
}

int MySQLInstase::begin() {
  int rt = query("BEGIN;");
  if (rt == 0) {
    in_trans_ = true;
  }
  return rt;
}

int MySQLInstase::rollBack() {
  int rt = query("ROLLBACK;");
  if (rt == 0) {
    in_trans_ = false;
  }
  return rt;
}

int MySQLInstase::query(const std::string &sql) {
  if (!init_succ_) {
    LOG_ERROR << "query error, mysql_handler init faild";
    return -1;
  }
  if (!sql_handler_) {
    LOG_DEBUG << "*************** will reconnect mysql ";
    reconnect();
  }
  // if (!sql_handler_) {
  //   LOG_DEBUG << "reconnect error, query return -1";
  //   return -1;
  // }

  LOG_DEBUG << "begin to excute sql[" << sql << "]";
  int rt = mysql_real_query(sql_handler_, sql.c_str(), sql.length());
  if (rt != 0) {
    LOG_ERROR << "excute mysql_real_query error, sql[" << sql << "], mysql sys errinfo[" << mysql_error(sql_handler_)
              << "]";
    // if connect error, begin to reconnect
    if (mysql_errno(sql_handler_) == CR_SERVER_GONE_ERROR || mysql_errno(sql_handler_) == CR_SERVER_LOST) {
      rt = reconnect();
      if (rt != 0 && !in_trans_) {
        // if reconnect succ, and current is not a trans, can do query sql again
        rt = mysql_real_query(sql_handler_, sql.c_str(), sql.length());
        return rt;
      }
    }
  } else {
    LOG_INFO << "excute mysql_real_query success, sql[" << sql << "]";
  }
  return rt;
}

MYSQL_RES *MySQLInstase::storeResult() {
  if (!init_succ_) {
    LOG_ERROR << "query error, mysql_handler init faild";
    return NULL;
  }
  int count = mysql_field_count(sql_handler_);
  if (count != 0) {
    MYSQL_RES *res = mysql_store_result(sql_handler_);
    if (!res) {
      LOG_ERROR << "excute mysql_store_result error, mysql sys errinfo[" << mysql_error(sql_handler_) << "]";
    } else {
      LOG_DEBUG << "excute mysql_store_result success";
    }
    return res;
  } else {
    LOG_DEBUG << "mysql_field_count = 0, not need store result";
    return NULL;
  }
}

MYSQL_ROW MySQLInstase::fetchRow(MYSQL_RES *res) {
  if (!init_succ_) {
    LOG_ERROR << "query error, mysql_handler init faild";
    return NULL;
  }
  return mysql_fetch_row(res);
}

long long MySQLInstase::numFields(MYSQL_RES *res) {
  if (!init_succ_) {
    LOG_ERROR << "query error, mysql_handler init faild";
    return -1;
  }
  return mysql_num_fields(res);
}

void MySQLInstase::freeResult(MYSQL_RES *res) {
  if (!init_succ_) {
    LOG_ERROR << "query error, mysql_handler init faild";
    return;
  }
  if (!res) {
    LOG_DEBUG << "free result error, res is null";
    return;
  }
  mysql_free_result(res);
}

long long MySQLInstase::affectedRows() {
  if (!init_succ_) {
    LOG_ERROR << "query error, mysql_handler init faild";
    return -1;
  }
  return mysql_affected_rows(sql_handler_);
}

std::string MySQLInstase::getMySQLErrorInfo() { return std::string(mysql_error(sql_handler_)); }

int MySQLInstase::getMySQLErrno() { return mysql_errno(sql_handler_); }

#endif

}  // namespace tirpc