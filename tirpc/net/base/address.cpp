#include "tirpc/net/base/address.hpp"
#include "tirpc/common/log.hpp"

namespace tirpc {

auto IPAddress::CheckValidIPAddr(const std::string &addr) -> bool {
  size_t i = addr.find_first_of(':');
  if (i == std::string::npos) {
    return false;
  }
  int port = std::atoi(addr.substr(i + 1, addr.size() - i - 1).c_str());
  if (port < 0 || port > 65536) {
    return false;
  }

  if (inet_addr(addr.substr(0, i).c_str()) == INADDR_NONE) {
    return false;
  }
  return true;
}

IPAddress::IPAddress(const std::string &ip, uint16_t port) : port_(port) {
  ip_ = ip;
  memset(&addr_, 0, sizeof(addr_));
  addr_.sin_family = AF_INET;
  addr_.sin_addr.s_addr = inet_addr(ip_.c_str());
  addr_.sin_port = htons(port_);

  LOG_DEBUG << "create ipv4 address succ [" << ToString() << "]";
}

IPAddress::IPAddress(sockaddr_in addr) : addr_(addr) {
  // if (addr_.sin_family != AF_INET) {
  // LOG_ERROR << "err family, this address is valid";
  // }
  LOG_DEBUG << "ip[" << ip_ << "], port[" << addr.sin_port;
  ip_ = std::string(inet_ntoa(addr_.sin_addr));
  port_ = ntohs(addr_.sin_port);
}

IPAddress::IPAddress(const std::string &addr) {
  size_t i = addr.find_first_of(':');
  if (i == std::string::npos) {
    LOG_ERROR << "invalid addr[" << addr << "]";
    return;
  }
  ip_ = addr.substr(0, i);
  port_ = std::atoi(addr.substr(i + 1, addr.size() - i - 1).c_str());

  memset(&addr_, 0, sizeof(addr_));
  addr_.sin_family = AF_INET;
  addr_.sin_addr.s_addr = inet_addr(ip_.c_str());
  addr_.sin_port = htons(port_);
  LOG_DEBUG << "create ipv4 address succ [" << ToString() << "]";
}

IPAddress::IPAddress(uint16_t port) : port_(port) {
  memset(&addr_, 0, sizeof(addr_));
  addr_.sin_family = AF_INET;
  addr_.sin_addr.s_addr = INADDR_ANY;
  addr_.sin_port = htons(port_);

  LOG_DEBUG << "create ipv4 address succ [" << ToString() << "]";
}

auto IPAddress::GetFamily() const -> int { return addr_.sin_family; }

auto IPAddress::GetSockAddr() -> sockaddr * { return reinterpret_cast<sockaddr *>(&addr_); }

auto IPAddress::ToString() const -> std::string {
  std::stringstream ss;
  ss << ip_ << ":" << port_;
  return ss.str();
}

void IPAddress::SetPort(uint16_t port) {
  port_ = port;
  memset(&addr_, 0, sizeof(addr_));
  addr_.sin_family = AF_INET;
  addr_.sin_addr.s_addr = INADDR_ANY;
  addr_.sin_port = htons(port_);
  LOG_DEBUG << "Update port, current ip address is [" << ToString() << "]";
}

auto IPAddress::GetSockLen() const -> socklen_t { return sizeof(addr_); }

UnixDomainAddress::UnixDomainAddress(std::string &path) : path_(path) {
  memset(&addr_, 0, sizeof(addr_));
  unlink(path_.c_str());
  addr_.sun_family = AF_UNIX;
  strcpy(addr_.sun_path, path_.c_str());
}

UnixDomainAddress::UnixDomainAddress(sockaddr_un addr) : addr_(addr) { path_ = addr_.sun_path; }

auto UnixDomainAddress::GetFamily() const -> int { return addr_.sun_family; }

auto UnixDomainAddress::GetSockAddr() -> sockaddr * { return reinterpret_cast<sockaddr *>(&addr_); }

auto UnixDomainAddress::GetSockLen() const -> socklen_t { return sizeof(addr_); }

auto UnixDomainAddress::ToString() const -> std::string { return path_; }

}  // namespace tirpc