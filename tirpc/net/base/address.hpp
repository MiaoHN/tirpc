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

class Address {
 public:
  using ptr = std::shared_ptr<Address>;

  virtual ~Address() = default;

  virtual auto GetSockAddr() -> sockaddr * = 0;

  virtual auto GetFamily() const -> int = 0;

  virtual auto ToString() const -> std::string = 0;

  virtual auto GetSockLen() const -> socklen_t = 0;
};

class IPAddress : public Address {
 public:
  using ptr = std::shared_ptr<IPAddress>;

  IPAddress(uint32_t addr = INADDR_ANY, uint16_t port = 0);

  IPAddress(const std::string &ip, uint16_t port);

  virtual ~IPAddress() = default;

  explicit IPAddress(const std::string &addr);

  explicit IPAddress(uint16_t port);

  explicit IPAddress(sockaddr_in addr);

  auto GetSockAddr() -> sockaddr * override;

  auto GetFamily() const -> int override;

  auto GetSockLen() const -> socklen_t override;

  auto ToString() const -> std::string override;

  auto GetIP() const -> std::string { return ip_; }

  void SetPort(uint16_t port);

  auto GetPort() const -> uint16_t { return port_; }

 public:
  static auto CheckValidIPAddr(const std::string &addr) -> bool;

 private:
  std::string ip_;
  uint16_t port_;
  sockaddr_in addr_;
};

class UnixDomainAddress : public Address {
 public:
  explicit UnixDomainAddress(std::string &path);

  explicit UnixDomainAddress(sockaddr_un addr);

  virtual ~UnixDomainAddress() = default;

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
