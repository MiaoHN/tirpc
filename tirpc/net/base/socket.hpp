#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>

#include "tirpc/net/base/address.hpp"

namespace tirpc {

class Socket : public std::enable_shared_from_this<Socket> {
 public:
  using ptr = std::shared_ptr<Socket>;

  Socket(int family, int type, int protocol = 0);

  ~Socket();

  static Socket::ptr CreateTCP(Address::ptr addr);

  bool IsValid();

  Socket::ptr Accept();

  bool Bind(const Address::ptr addr);

  bool Connect(const Address::ptr addr, uint64_t timeout_ms = -1);

  bool Listen(int backlog = SOMAXCONN);

  void Close();

  bool GetOption(int level, int option, void *result, socklen_t *len);

  template <class T>
  bool GetOption(int level, int option, T &result) {
    socklen_t length = sizeof(T);
    return GetOption(level, option, &result, &length);
  }

  bool SetOption(int level, int option, const void *result, socklen_t len);

  template <class T>
  bool SetOption(int level, int option, const T &value) {
    return SetOption(level, option, &value, sizeof(T));
  }

  int Send(const void *buffer, size_t length, int flags = 0);

  int Recv(void *buffer, size_t length, int flags = 0);

  Address::ptr GetRemoteAddr();

  Address::ptr GetLocalAddr();

  int GetFd() { return fd_; }

 protected:
  void InitSock();

  void NewSock();

  bool Init(int fd);

 private:
  int fd_{-1};
  int family_{-1};
  int type_{-1};
  int protocol_{-1};

  bool is_connected_{false};

  Address::ptr local_addr_{nullptr};
  Address::ptr remote_addr_{nullptr};
};

}  // namespace tirpc
