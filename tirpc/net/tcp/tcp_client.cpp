#include "tirpc/net/tcp/tcp_client.hpp"

#include <arpa/inet.h>
#include <sys/socket.h>

#include <utility>

#include "tirpc/common/error_code.hpp"
#include "tirpc/common/log.hpp"
#include "tirpc/coroutine/coroutine.hpp"
#include "tirpc/coroutine/coroutine_hook.hpp"
#include "tirpc/coroutine/coroutine_pool.hpp"
#include "tirpc/net/base/address.hpp"
#include "tirpc/net/base/fd_event.hpp"
#include "tirpc/net/base/timer.hpp"
#include "tirpc/net/http/http_codec.hpp"
#include "tirpc/net/rpc/rpc_codec.hpp"

namespace tirpc {

TcpClient::TcpClient(NetAddress::ptr addr, ProtocalType type /*= TinyPb_Protocal*/) : peer_addr_(std::move(addr)) {
  family_ = peer_addr_->GetFamily();
  fd_ = socket(AF_INET, SOCK_STREAM, 0);
  if (fd_ == -1) {
    ErrorLog << "call socket error, fd=-1, sys error=" << strerror(errno);
  }
  DebugLog << "TcpClient() create fd = " << fd_;
  local_addr_ = std::make_shared<IPAddress>("127.0.0.1", 0);
  reactor_ = Reactor::GetReactor();

  if (type == Http_Protocal) {
    codec_ = std::make_shared<HttpCodeC>();
  } else {
    codec_ = std::make_shared<TinyPbCodeC>();
  }

  connection_ = std::make_shared<TcpConnection>(this, reactor_, fd_, 128, peer_addr_);
}

TcpClient::~TcpClient() {
  if (fd_ > 0) {
    FdEventContainer::GetFdContainer()->GetFdEvent(fd_)->UnregisterFromReactor();
    close(fd_);
    DebugLog << "~TcpClient() close fd = " << fd_;
  }
}

auto TcpClient::GetConnection() -> TcpConnection * {
  if (connection_.get() == nullptr) {
    connection_ = std::make_shared<TcpConnection>(this, reactor_, fd_, 128, peer_addr_);
  }
  return connection_.get();
}

void TcpClient::ResetFd() {
  FdEvent::ptr fd_event = FdEventContainer::GetFdContainer()->GetFdEvent(fd_);
  fd_event->UnregisterFromReactor();
  close(fd_);
  fd_ = socket(AF_INET, SOCK_STREAM, 0);
  if (fd_ == -1) {
    ErrorLog << "call socket error, fd=-1, sys error=" << strerror(errno);
  } else {
  }
}

auto TcpClient::SendAndRecvTinyPb(const std::string &msg_no, TinyPbStruct::pb_ptr &res) -> int {
  bool is_timeout = false;
  Coroutine *cur_cor = Coroutine::GetCurrentCoroutine();
  auto timer_cb = [this, &is_timeout, cur_cor]() {
    InfoLog << "TcpClient timer out event occur";
    is_timeout = true;
    this->connection_->SetOverTimeFlag(true);
    Coroutine::Resume(cur_cor);
  };
  TimerEvent::ptr event = std::make_shared<TimerEvent>(max_timeout_, false, timer_cb);
  reactor_->GetTimer()->AddTimerEvent(event);

  DebugLog << "add rpc timer event, timeout on " << event->arrive_time_;

  while (!is_timeout) {
    DebugLog << "begin to connect";
    if (connection_->GetState() != Connected) {
      int rt = connect_hook(fd_, reinterpret_cast<sockaddr *>(peer_addr_->GetSockAddr()), peer_addr_->GetSockLen());
      if (rt == 0) {
        DebugLog << "connect [" << peer_addr_->ToString() << "] succ!";
        connection_->SetUpClient();
        break;
      }
      ResetFd();
      if (errno == ECONNREFUSED) {
        std::stringstream ss;
        ss << "connect error, peer[ " << peer_addr_->ToString() << " ] closed.";
        err_info_ = ss.str();
        ErrorLog << "cancle overtime event, err info=" << err_info_;
        reactor_->GetTimer()->DelTimerEvent(event);
        return ERROR_PEER_CLOSED;
      }
      if (errno == EAFNOSUPPORT) {
        std::stringstream ss;
        ss << "connect cur sys ror, errinfo is " << std::string(strerror(errno)) << " ] closed.";
        err_info_ = ss.str();
        ErrorLog << "cancle overtime event, err info=" << err_info_;
        reactor_->GetTimer()->DelTimerEvent(event);
        return ERROR_CONNECT_SYS_ERR;
      }
    } else {
      break;
    }
  }

  if (connection_->GetState() != Connected) {
    std::stringstream ss;
    ss << "connect peer addr[" << peer_addr_->ToString() << "] error. sys error=" << strerror(errno);
    err_info_ = ss.str();
    reactor_->GetTimer()->DelTimerEvent(event);
    return ERROR_FAILED_CONNECT;
  }

  connection_->SetUpClient();
  connection_->Output();
  if (connection_->GetOverTimerFlag()) {
    InfoLog << "send data over time";
    is_timeout = true;
    goto err_deal;
  }

  while (!connection_->GetResPackageData(msg_no, res)) {
    DebugLog << "redo getResPackageData";
    connection_->Input();

    if (connection_->GetOverTimerFlag()) {
      InfoLog << "read data over time";
      is_timeout = true;
      goto err_deal;
    }
    if (connection_->GetState() == Closed) {
      InfoLog << "peer close";
      goto err_deal;
    }

    connection_->Execute();
  }

  reactor_->GetTimer()->DelTimerEvent(event);
  err_info_ = "";
  return 0;

err_deal:
  // connect error should close fd and reopen new one
  FdEventContainer::GetFdContainer()->GetFdEvent(fd_)->UnregisterFromReactor();
  close(fd_);
  fd_ = socket(AF_INET, SOCK_STREAM, 0);
  std::stringstream ss;
  if (is_timeout) {
    ss << "call rpc falied, over " << max_timeout_ << " ms";
    err_info_ = ss.str();

    connection_->SetOverTimeFlag(false);
    return ERROR_RPC_CALL_TIMEOUT;
  }
  ss << "call rpc falied, peer closed [" << peer_addr_->ToString() << "]";
  err_info_ = ss.str();
  return ERROR_PEER_CLOSED;
}

void TcpClient::Stop() {
  if (!is_stop_) {
    is_stop_ = true;
    reactor_->Stop();
  }
}

}  // namespace tirpc
