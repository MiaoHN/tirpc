#include <iostream>
#include <zookeeper/zookeeper.h>
#include <cstring>
#include <semaphore.h>

// 信号量，用于等待连接完成
sem_t sem;

// 全局的 watcher 函数，处理 ZooKeeper 事件
void globalWatcher(zhandle_t *zh, int type, int state, const char *path, void *watcherCtx) {
    if (type == ZOO_SESSION_EVENT) {
        if (state == ZOO_CONNECTED_STATE) {
            // 连接成功，释放信号量
            sem_post(&sem);
        }
    }
}

int main() {
    // ZooKeeper 服务器地址和端口
    const char* host = "127.0.0.1:2181";
    // 会话超时时间（毫秒）
    int timeout = 30000;

    // 初始化信号量
    sem_init(&sem, 0, 0);

    zoo_set_debug_level(ZOO_LOG_LEVEL_ERROR);

    // 初始化 ZooKeeper 句柄
    zhandle_t* zh = zookeeper_init(host, globalWatcher, timeout, 0, nullptr, 0);
    if (zh == nullptr) {
        std::cerr << "Failed to initialize ZooKeeper handle." << std::endl;
        return 1;
    }

    // 等待连接完成
    sem_wait(&sem);

    std::cout << "Successfully connected to ZooKeeper server." << std::endl;

    // 创建一个临时节点进行测试
    const char* path = "/test_node";
    const char* data = "This is a test data";
    int dataLen = strlen(data);
    char pathBuffer[1024];
    int bufferLen = sizeof(pathBuffer);
    int flag = zoo_create(zh, path, data, dataLen, &ZOO_OPEN_ACL_UNSAFE, ZOO_EPHEMERAL, pathBuffer, bufferLen);
    if (flag == ZOK) {
        std::cout << "Successfully created node: " << pathBuffer << std::endl;
    } else {
        std::cerr << "Failed to create node: " << path << ", error code: " << flag << std::endl;
    }

    // 获取节点的数据
    char buffer[1024];
    int bufferLenGet = sizeof(buffer);
    struct Stat stat;
    flag = zoo_get(zh, path, 0, buffer, &bufferLenGet, &stat);
    if (flag == ZOK) {
        std::cout << "Data of node " << path << ": " << buffer << std::endl;
    } else {
        std::cerr << "Failed to get data of node: " << path << ", error code: " << flag << std::endl;
    }

    // 删除临时节点
    flag = zoo_delete(zh, path, -1);
    if (flag == ZOK) {
        std::cout << "Successfully deleted node: " << path << std::endl;
    } else {
        std::cerr << "Failed to delete node: " << path << ", error code: " << flag << std::endl;
    }

    // 关闭 ZooKeeper 句柄
    zookeeper_close(zh);

    // 销毁信号量
    sem_destroy(&sem);

    return 0;
}