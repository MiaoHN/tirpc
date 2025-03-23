#include "tirpc/net/base/socket.hpp"
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <cerrno>
#include <cstring>
#include <memory>
#include "tirpc/common/log.hpp"
#include "tirpc/coroutine/coroutine_hook.hpp"
#include "tirpc/net/base/address.hpp"

namespace tirpc {

Socket::ptr Socket::CreateTCP(Address::ptr addr) { return std::make_shared<Socket>(addr->GetFamily(), SOCK_STREAM, 0); }

Socket::Socket(int family, int type, int protocol)
    : fd_{-1}, family_(family), type_(type), protocol_(protocol), is_connected_(false) {}

Socket::~Socket() { Close(); }

Socket::ptr Socket::Accept() {
  Socket::ptr sock(new Socket(family_, type_, protocol_));

  int new_sockfd = accept_hook(fd_, nullptr, nullptr);
  if (new_sockfd == -1) {
    LOG_ERROR << "accept(" << fd_ << ") errno=" << errno << " errstr=" << strerror(errno);
    return nullptr;
  }
  if (sock->Init(new_sockfd)) {
    return sock;
  }
  return nullptr;
}

bool Socket::Bind(const Address::ptr addr) {
  if (!IsValid()) {
    NewSock();
  }
  if (::bind(fd_, addr->GetSockAddr(), addr->GetSockLen())) {
    return false;
  }
  GetLocalAddr();
  return true;
}

bool Socket::Connect(const Address::ptr addr, uint64_t timeout_ms) {
  remote_addr_ = addr;
  if (!IsValid()) {
    NewSock();
  }
  if (timeout_ms == (uint64_t)-1) {
    if (::connect(fd_, addr->GetSockAddr(), addr->GetSockLen())) {
      Close();
      return false;
    }
  }
  is_connected_ = true;
  GetRemoteAddr();
  GetLocalAddr();
  return true;
}

bool Socket::IsValid() { return fd_ != -1; }

bool Socket::Listen(int backlog) { return ::listen(fd_, backlog); }

void Socket::Close() {}

bool Socket::GetOption(int level, int option, void *result, socklen_t *len) {
  int rt = ::getsockopt(fd_, level, option, result, (socklen_t *)len);
  if (rt) {
    return false;
  }
  return true;
}

bool Socket::SetOption(int level, int option, const void *result, socklen_t len) {
  if (setsockopt(fd_, level, option, result, (socklen_t)len)) {
    return false;
  }
  return true;
}

int Socket::Send(const void *buffer, size_t length, int flags) {
  if (is_connected_) {
    return send_hook(fd_, buffer, length, flags);
  }
  return -1;
}

int Socket::Recv(void *buffer, size_t length, int flags) {
  if (is_connected_) {
    return recv_hook(fd_, buffer, length, flags);
  }
  return -1;
}

Address::ptr Socket::GetRemoteAddr() {
  if (remote_addr_) {
    return remote_addr_;
  }

  Address::ptr result;
  switch (family_) {
    case AF_INET:
      result.reset(new IPAddress());
      break;
  }

  socklen_t addrlen = result->GetSockLen();
  if (getpeername(fd_, result->GetSockAddr(), &addrlen)) {
    // SYLAR_LOG_ERROR(g_logger) << "getpeername error sock=" << m_sock
    //     << " errno=" << errno << " errstr=" << strerror(errno);
    // return Address::ptr(new UnknownAddress(m_family));
    return nullptr;
  }
  remote_addr_ = result;
  return remote_addr_;
}

Address::ptr Socket::GetLocalAddr() {
  if (local_addr_) {
    return local_addr_;
  }

  Address::ptr result;
  switch (family_) {
    case AF_INET:
      result.reset(new IPAddress());
      break;
  }
  socklen_t addrlen = result->GetSockLen();
  if (getsockname(fd_, result->GetSockAddr(), &addrlen)) {
    LOG_ERROR << "getsockname error sock=" << fd_ << " errno=" << errno << " errstr=" << strerror(errno);
    return nullptr;
  }
  local_addr_ = result;
  return local_addr_;
}

void Socket::InitSock() {
  int val = 1;
  SetOption(SOL_SOCKET, SO_REUSEADDR, val);
  if (type_ == SOCK_STREAM) {
    SetOption(IPPROTO_TCP, TCP_NODELAY, val);
  }
}

void Socket::NewSock() {
  fd_ = ::socket(family_, type_, protocol_);
  if (fd_ != -1) {
    InitSock();
  } else {
    LOG_ERROR << "socket(" << family_ << ", " << type_ << ", " << protocol_ << ") errno=" << errno
              << " errstr=" << strerror(errno);
  }
}

bool Socket::Init(int fd) {
  fd_ = fd;
  is_connected_ = true;
  InitSock();
  GetLocalAddr();
  GetRemoteAddr();
  return true;
}

}  // namespace tirpc