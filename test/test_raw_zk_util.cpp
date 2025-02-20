#include <semaphore.h>
#include <zookeeper/zookeeper.h>
#include <chrono>
#include <iostream>
#include <memory>
#include <string>
#include <thread>
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

// 全局的watcher观察器   zkserver给zkclient的通知
void ZkClient::globalWatcher(zhandle_t *zh, int type, int state, const char *path, void *watcherCtx) {
  ZkClient *cli = (ZkClient *)zoo_get_context(zh);
  if (cli == nullptr) {
    return;
  }

  if (type == ZOO_SESSION_EVENT) {       // 回调的消息类型是和会话相关的消息类型
    if (state == ZOO_CONNECTED_STATE) {  // zkclient和zkserver连接成功
      sem_post(&cli->sem_);              // 连接成功
    } else if (state == ZOO_CONNECTING_STATE) {
    } else if (state == ZOO_EXPIRED_SESSION_STATE || state == ZOO_AUTH_FAILED_STATE) {  // 会话超时，重新连接
      std::cerr << "Trying to reconnect zk......";
      cli->stop();
      cli->start();
    } else {
      std::cerr << "connect failure, state: " << state;
      sem_post(&cli->sem_);
    }
  }
}

ZkClient::ZkClient(const std::string &ip, int port, int timeout) {
  zhandle_ = nullptr;
  connstr_ = ip + ":" + std::to_string(port);
  std::cout << "connstr_ is '" << connstr_ << "'";
  timeout_ = timeout;
}

ZkClient::ZkClient() : zhandle_(nullptr), connstr_("127.0.0.1:2181"), timeout_(30000) {}

ZkClient::~ZkClient() { stop(); }

// zkclient启动连接zkserver
void ZkClient::start() {
  /*
  zookeeper_mt：多线程版本
  zookeeper的API客户端程序提供了三个线程
  API调用线程 （当前线程）
  网络I/O线程  pthread_create  poll
  watcher回调线程 pthread_create 给客户端通知
  */
  // 这是异步的连接
  std::cout << connstr_.c_str() << std::endl;
  zhandle_ = zookeeper_init(connstr_.c_str(), globalWatcher, timeout_, nullptr, nullptr, 0);
  // 返回表示句柄创建成功，不代表连接成功了
  if (nullptr == zhandle_) {
    std::cerr << "zookeeper_init error!" << std::endl;
    exit(0);
  }

  zoo_set_context(zhandle_, this);

  sem_init(&sem_, 0, 0);

  sem_wait(&sem_);

  std::cout << "zookeeper_init success!" << std::endl;
}

// 断开zkclient与zkserver的连接
void ZkClient::stop() {
  if (zhandle_ != nullptr) {
    zookeeper_close(zhandle_);  // 关闭句柄，释放资源  MySQL_Conn
    zhandle_ = nullptr;
    std::cout << "zookeeper connection close success!" << std::endl;
  }
}

// 在zkserver上根据指定的path创建znode节点
// state: 表示永久性节点还是临时性节点，0是永久性节点。永久性节点的ephemeralOwner为0
void ZkClient::create(const char *path, const char *data, int datalen, int state) {
  char path_buffer[1024] = {0};
  int bufferlen = sizeof(path_buffer);
  int flag;
  // 先判断path表示的znode节点是否存在，如果存在，就不再重复创建了
  flag = zoo_exists(zhandle_, path, 0, nullptr);  // 同步的判断
  if (ZNONODE == flag) {                          // 表示path的znode节点不存在
    // 创建指定path的znode节点了
    flag =
        zoo_create(zhandle_, path, data, datalen, &ZOO_OPEN_ACL_UNSAFE, state, path_buffer, bufferlen);  // 也是同步的
    if (flag == ZOK) {
      std::cout << "znode create success... path: " << path << std::endl;
    } else {
      std::cerr << "flag: " << flag << std::endl;
      std::cerr << "znode create error... path: " << path << std::endl;
    }
  }
}

// 删除节点
void ZkClient::deleteNode(const char *path) {
  int flag;
  // 先判断path表示的znode节点是否存在，如果不存在，就不再重复删除
  flag = zoo_exists(zhandle_, path, 0, nullptr);  // 同步的判断
  if (ZNONODE != flag) {                          // 表示path的znode节点不存在
    flag = zoo_delete(zhandle_, path, -1);        // 也是同步的
    if (flag == ZOK) {
      std::cout << "znode delete success... path: " << path << std::endl;
    } else {
      std::cerr << "flag: " << flag << std::endl;
      std::cerr << "znode delete error... path: " << path << std::endl;
    }
  }
}

// 根据指定的path，获取znode节点的值
std::string ZkClient::getData(const char *path) {
  char buffer[64];
  int bufferlen = sizeof(buffer);
  // 以同步的方式获取znode节点的值
  int flag = zoo_get(zhandle_, path, 0, buffer, &bufferlen, nullptr);
  if (flag != ZOK) {
    std::cerr << "get znode error... path: " << path << std::endl;
    return "";
  }
  return buffer;
}

// 获取路径对应的子节点
std::vector<std::string> ZkClient::getChildrenNodes(const std::string &path) {
  auto it = childrenNodesMap_.find(path);
  if (childrenNodesMap_.find(path) != childrenNodesMap_.end()) return it->second;
  std::vector<std::string> result = getChildren(path.c_str());
  childrenNodesMap_[path] = result;
  return result;
}

// 根据参数指定的znode节点路径，获取znode节点的子节点
std::vector<std::string> ZkClient::getChildren(const char *path) {
  struct String_vector node_vec;
  int flag = zoo_wget_children(zhandle_, path, serviceWatcher, nullptr, &node_vec);
  zoo_set_context(zhandle_, this);
  std::vector<std::string> result;
  if (flag != ZOK) {
    std::cerr << "get znode children error... path: " << path << std::endl;
  } else {
    int size = node_vec.count;
    for (int i = 0; i < size; ++i) {
      result.push_back(node_vec.data[i]);
    }
    deallocate_String_vector(&node_vec);
  }
  return result;
}

void ZkClient::serviceWatcher(zhandle_t *zh, int type, int state, const char *path, void *watcherCtx) {
  ZkClient *instance = (ZkClient *)zoo_get_context(zh);
  if (type == ZOO_CHILD_EVENT) {
    // zk设置监听watcher只能是一次性的，每次触发后需要重复设置
    std::vector<std::string> result = instance->getChildren(path);
    instance->childrenNodesMap_[path] = result;
  }
}

// 关闭控制台日志输出（只输出错误的）
void ZkClient::closeLog() { zoo_set_debug_level(ZOO_LOG_LEVEL_ERROR); }

// 心跳机制
void ZkClient::sendHeartBeat() {
  std::thread t([&]() {
    while (zhandle_) {
      int timeout = zoo_recv_timeout(zhandle_);
      if (timeout <= 0) break;

      std::this_thread::sleep_for(std::chrono::milliseconds(timeout / 3));

      if (zoo_exists(zhandle_, "/tirpc", 0, nullptr) != ZOK) {
        std::cerr << "心跳发送失败，可能已断开连接" << std::endl;
        break;
      }
    }
  });
  t.detach();
}

class ServiceRegister {
 public:
  ServiceRegister() { Init(); }

  ~ServiceRegister() { cli.stop(); }

  void Init() {
    cli.closeLog();
    cli.start();
    cli.create("/tirpc", nullptr, 0);
  }

 private:
  ZkClient cli;
};

}  // namespace tirpc

int main(int argc, char **argv) {
  auto center = std::make_shared<tirpc::ServiceRegister>();

  return 0;
}