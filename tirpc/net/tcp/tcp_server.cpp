#include "tirpc/net/tcp/tcp_server.hpp"

#include <fcntl.h>
#include <sys/socket.h>
#include <cassert>
#include <cstring>
#include <utility>

#include "tirpc/common/config.hpp"
#include "tirpc/coroutine/coroutine.hpp"
#include "tirpc/coroutine/coroutine_hook.hpp"
#include "tirpc/coroutine/coroutine_pool.hpp"
#include "tirpc/net/http/http_codec.hpp"
#include "tirpc/net/http/http_dispatcher.hpp"
#include "tirpc/net/rpc/rpc_dispatcher.hpp"
#include "tirpc/net/tcp/io_thread.hpp"
#include "tirpc/net/tcp/tcp_connection.hpp"
#include "tirpc/net/tcp/tcp_connection_time_wheel.hpp"

namespace tirpc {

extern tirpc::Config::ptr g_rpc_config;

TcpAcceptor::TcpAcceptor(Address::ptr net_addr) : local_addr_(std::move(net_addr)) {
  family_ = local_addr_->GetFamily();
}

void TcpAcceptor::Init() {
  fd_ = socket(local_addr_->GetFamily(), SOCK_STREAM, 0);
  if (fd_ < 0) {
    LOG_ERROR << "start server error. socket error, sys error=" << strerror(errno);
    Exit(0);
  }
  // assert(fd_ != -1);
  LOG_DEBUG << "create listenfd succ, listenfd=" << fd_;

  // int flag = fcntl(fd_, F_GETFL, 0);
  // int rt = fcntl(fd_, F_SETFL, flag | O_NONBLOCK);

  // if (rt != 0) {
  // LOG_ERROR << "fcntl set nonblock error, errno=" << errno << ", error=" << strerror(errno);
  // }

  int val = 1;
  if (setsockopt(fd_, SOL_SOCKET, SO_REUSEADDR, &val, sizeof(val)) < 0) {
    LOG_ERROR << "set REUSEADDR error";
  }

  socklen_t len = local_addr_->GetSockLen();
  int rt = bind(fd_, local_addr_->GetSockAddr(), len);
  if (rt != 0) {
    LOG_ERROR << "start server error. bind error, errno=" << errno << ", error=" << strerror(errno);
    Exit(0);
  }
  // assert(rt == 0);

  LOG_DEBUG << "set REUSEADDR succ";
  rt = listen(fd_, 10);
  if (rt != 0) {
    LOG_ERROR << "start server error. listen error, fd= " << fd_ << ", errno=" << errno << ", error=" << strerror(errno);
    Exit(0);
  }
  // assert(rt == 0);
}

TcpAcceptor::~TcpAcceptor() {
  FdEvent::ptr fd_event = FdEventContainer::GetFdContainer()->GetFdEvent(fd_);
  fd_event->UnregisterFromReactor();
  if (fd_ != -1) {
    close(fd_);
  }
}

auto TcpAcceptor::ToAccept() -> int {
  socklen_t len = 0;
  int rt = 0;

  if (family_ == AF_INET) {
    sockaddr_in cli_addr;
    memset(&cli_addr, 0, sizeof(cli_addr));
    len = sizeof(cli_addr);
    // call hook accept
    rt = accept_hook(fd_, reinterpret_cast<sockaddr *>(&cli_addr), &len);
    if (rt == -1) {
      LOG_DEBUG << "error, no new client coming, errno=" << errno << "error=" << strerror(errno);
      return -1;
    }
    LOG_DEBUG << "New client accepted succ! port:[" << cli_addr.sin_port;
    peer_addr_ = std::make_shared<IPAddress>(cli_addr);
  } else if (family_ == AF_UNIX) {
    sockaddr_un cli_addr;
    len = sizeof(cli_addr);
    memset(&cli_addr, 0, sizeof(cli_addr));
    // call hook accept
    rt = accept_hook(fd_, reinterpret_cast<sockaddr *>(&cli_addr), &len);
    if (rt == -1) {
      LOG_DEBUG << "error, no new client coming, errno=" << errno << "error=" << strerror(errno);
      return -1;
    }
    peer_addr_ = std::make_shared<UnixDomainAddress>(cli_addr);

  } else {
    LOG_ERROR << "unknown type protocol!";
    close(rt);
    return -1;
  }

  LOG_DEBUG << "New client accepted succ! fd:[" << rt << ", addr:[" << peer_addr_->ToString() << "]";

  return rt;
}

TcpServer::TcpServer(Address::ptr addr) : addr_(std::move(addr)) {
  io_pool_ = std::make_shared<IOThreadPool>(g_rpc_config->iothread_num_);

  main_reactor_ = Reactor::GetReactor();
  main_reactor_->SetReactorType(MainReactor);

  time_wheel_ = std::make_shared<TcpTimeWheel>(main_reactor_, g_rpc_config->timewheel_bucket_num_,
                                               g_rpc_config->timewheel_interval_);

  clear_clent_timer_event_ =
      std::make_shared<TimerEvent>(10000, true, std::bind(&TcpServer::ClearClientTimerFunc, this));
  main_reactor_->GetTimer()->AddTimerEvent(clear_clent_timer_event_);

  LOG_DEBUG << "TcpServer setup on [" << addr_->ToString() << "]";
}

void TcpServer::Start() {
  if (!start_info_.empty()) {
    std::cout << start_info_ << std::endl << std::endl;
  }
  acceptor_.reset(new TcpAcceptor(addr_));
  acceptor_->Init();
  accept_cor_ = GetCoroutinePool()->GetCoroutineInstanse();
  accept_cor_->SetCallBack(std::bind(&TcpServer::MainAcceptCorFunc, this));

  LOG_DEBUG << "resume accept coroutine";
  Coroutine::Resume(accept_cor_.get());

  // accept_cor 已经执行，但在服务器刚启动时没有其它连接（NonBlocking），所以 Yield 回来了

  io_pool_->Start();
  main_reactor_->Loop();
}

TcpServer::~TcpServer() {
  GetCoroutinePool()->ReturnCoroutine(accept_cor_);
  if (register_) {
    register_->Clear();
  }
  LOG_DEBUG << "~TcpServer";
}

void TcpServer::MainAcceptCorFunc() {
  while (!is_stop_accept_) {
    int fd = acceptor_->ToAccept();
    if (fd == -1) {
      Coroutine::Yield();
      continue;
    }
    IOThread *io_thread = io_pool_->GetIoThread();
    TcpConnection::ptr conn = AddClient(io_thread, fd);
    conn->InitServer();
    LOG_DEBUG << "tcpconnection address is " << conn.get() << ", and fd is" << fd;

    io_thread->GetReactor()->AddCoroutine(conn->GetCoroutine());
    tcp_counts_++;
    LOG_DEBUG << "current tcp connection count is [" << tcp_counts_ << "]";
  }
}

void TcpServer::AddCoroutine(Coroutine::ptr cor) { main_reactor_->AddCoroutine(cor); }

auto TcpServer::AddClient(IOThread *io_thread, int fd) -> TcpConnection::ptr {
  auto it = clients_.find(fd);
  if (it != clients_.end()) {
    it->second.reset();
    // set new Tcpconnection
    LOG_DEBUG << "fd " << fd << "have exist, reset it";
    it->second = std::make_shared<TcpConnection>(this, io_thread, fd, 128, GetPeerAddr());
    return it->second;
  }
  LOG_DEBUG << "fd " << fd << "did't exist, new it";
  TcpConnection::ptr conn = std::make_shared<TcpConnection>(this, io_thread, fd, 128, GetPeerAddr());
  clients_.insert(std::make_pair(fd, conn));
  return conn;
}

void TcpServer::FreshTcpConnection(TcpTimeWheel::TcpConnectionSlot::ptr slot) {
  auto cb = [slot, this]() mutable {
    this->GetTimeWheel()->Fresh(slot);
    slot.reset();
  };
  main_reactor_->AddTask(cb);
}

void TcpServer::ClearClientTimerFunc() {
  // LOG_DEBUG << "this IOThread loop timer excute";

  // delete Closed TcpConnection per loop
  // for free memory
  // LOG_DEBUG << "clients_.size=" << clients_.size();
  for (auto &i : clients_) {
    // TcpConnection::ptr s_conn = i.second;
    // LOG_DEBUG << "state = " << s_conn->GetState();
    if (i.second && i.second.use_count() > 0 && i.second->GetState() == Closed) {
      // need to delete TcpConnection
      LOG_DEBUG << "TcpConection [fd:" << i.first << "] will delete, state=" << i.second->GetState();
      (i.second).reset();
      // s_conn.reset();
    }
  }
}

auto TcpServer::GetPeerAddr() -> Address::ptr { return acceptor_->GetPeerAddr(); }

auto TcpServer::GetLocalAddr() -> Address::ptr { return addr_; }

auto TcpServer::GetTimeWheel() -> TcpTimeWheel::ptr { return time_wheel_; }

auto TcpServer::GetIoThreadPool() -> IOThreadPool::ptr { return io_pool_; }

auto TcpServer::GetDispatcher() -> AbstractDispatcher::ptr { return dispatcher_; }

auto TcpServer::GetCodec() -> AbstractCodeC::ptr { return codec_; }

}  // namespace tirpc
