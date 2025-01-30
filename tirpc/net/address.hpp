#pragma once

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>
#include <unistd.h>
#include <cstdint>
#include <memory>
#include <string>

namespace tirpc {

class NetAddress {
 public:
  using ptr = std::shared_ptr<NetAddress>;

  virtual auto GetSockAddr() -> sockaddr * = 0;

  virtual auto GetFamily() const -> int = 0;

  virtual auto ToString() const -> std::string = 0;

  virtual auto GetSockLen() const -> socklen_t = 0;
};

class IPAddress : public NetAddress {
 public:
  IPAddress(const std::string &ip, uint16_t port);

  explicit IPAddress(const std::string &addr);

  explicit IPAddress(uint16_t port);

  explicit IPAddress(sockaddr_in addr);

  auto GetSockAddr() -> sockaddr * override;

  auto GetFamily() const -> int override;

  auto GetSockLen() const -> socklen_t override;

  auto ToString() const -> std::string override;

  auto GetIP() const -> std::string { return ip_; }

  auto GetPort() const -> uint16_t { return port_; }

 public:
  static auto CheckValidIPAddr(const std::string &addr) -> bool;

 private:
  std::string ip_;
  uint16_t port_;
  sockaddr_in addr_;
};

class UnixDomainAddress : public NetAddress {
 public:
  explicit UnixDomainAddress(std::string &path);

  explicit UnixDomainAddress(sockaddr_un addr);

  auto GetSockAddr() -> sockaddr * override;

  auto GetFamily() const -> int override;

  auto GetSockLen() const -> socklen_t override;

  auto ToString() const -> std::string override;

  auto GetPath() const -> std::string { return path_; }

 private:
  std::string path_;
  sockaddr_un addr_;
};

}  // namespace tirpc
