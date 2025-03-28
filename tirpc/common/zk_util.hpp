#pragma once

#include <semaphore.h>
#include <zookeeper/zookeeper.h>
#include <string>
#include <unordered_map>
#include <vector>

namespace tirpc {

// 封装的zookeeper客户端类
class ZkClient {
 public:
  ZkClient();
  ZkClient(const std::string &ip, int port, int timeout);
  ~ZkClient();

  // zkclient启动连接zkserver
  void start();

  // 在zkserver上根据指定的path创建znode节点
  void create(const char *path, const char *data, int datalen, int state = 0);

  // 根据参数指定的znode节点路径，获取znode节点的值
  std::string getData(const char *path);

  // 获取路径对应的子节点
  std::vector<std::string> getChildrenNodes(const std::string &path);

  // 断开zkclient与zkserver的连接
  void stop();

  // 删除节点
  void deleteNode(const char *path);

  // 获取连接状态
  bool getIsConnected() const { return zhandle_; }

  // 关闭控制台日志输出（只输出错误的）
  void closeLog();

  // 心跳机制
  void sendHeartBeat();

 private:
  // zk的客户端句柄
  zhandle_t *zhandle_;

  sem_t sem_;

  std::string connstr_;

  int timeout_;

  // 缓存路径对应的子节点，这样就不用总是遍历文件系统了
  std::unordered_map<std::string, std::vector<std::string>> childrenNodesMap_;

  // 根据参数指定的znode节点路径，获取znode节点的子节点
  std::vector<std::string> getChildren(const char *path);

  // 获取子节点的watcher
  static void serviceWatcher(zhandle_t *zh, int type, int state, const char *path, void *watcherCtx);

  // 全局的watcher观察器   zkserver给zkclient的通知
  static void globalWatcher(zhandle_t *zh, int type, int state, const char *path, void *watcherCtx);
};

}  // namespace tirpc
