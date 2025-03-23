#include "tirpc/net/tcp/tcp_server.hpp"

#include <fcntl.h>
#include <sys/socket.h>
#include <cassert>
#include <cstring>
#include <utility>

#include "tirpc/common/config.hpp"
#include "tirpc/coroutine/coroutine.hpp"
#include "tirpc/coroutine/coroutine_pool.hpp"
#include "tirpc/net/tcp/io_thread.hpp"
#include "tirpc/net/tcp/tcp_connection.hpp"
#include "tirpc/net/tcp/tcp_connection_time_wheel.hpp"

namespace tirpc {

static ConfigVar<std::string>::ptr g_server_ip = Config::Lookup("server.ip", std::string("127.0.0.1"));
static ConfigVar<int>::ptr g_server_port = Config::Lookup("server.port", 19999);
static ConfigVar<std::string>::ptr g_server_protocal = Config::Lookup("server.protocal", std::string("TinyPB"));

static ConfigVar<int>::ptr g_iothread_num = Config::Lookup("iothread_num", 1, "IO thread number");
static ConfigVar<int>::ptr g_timewheel_bucket_num = Config::Lookup("time_wheel.bucket_num", 3, "TimeWheel bucket num");
static ConfigVar<int>::ptr g_timewheel_interval = Config::Lookup("time_wheel.interval", 5, "TimeWheel interval");

TcpServer::TcpServer() {
  addr_ = std::make_shared<IPAddress>(g_server_ip->GetValue(), g_server_port->GetValue());

  io_pool_ = std::make_shared<IOThreadPool>(g_iothread_num->GetValue());

  main_reactor_ = Reactor::GetReactor();
  main_reactor_->SetReactorType(MainReactor);

  time_wheel_ = std::make_shared<TcpTimeWheel>(main_reactor_, g_timewheel_bucket_num->GetValue(),
                                               g_timewheel_interval->GetValue());

  clear_clent_timer_event_ =
      std::make_shared<TimerEvent>(10000, true, std::bind(&TcpServer::ClearClientTimerFunc, this));
  main_reactor_->GetTimer()->AddTimerEvent(clear_clent_timer_event_);

  LOG_DEBUG << "TcpServer setup on [" << addr_->ToString() << "]";
}

TcpServer::TcpServer(Address::ptr addr) : addr_(std::move(addr)) {
  io_pool_ = std::make_shared<IOThreadPool>(g_iothread_num->GetValue());

  main_reactor_ = Reactor::GetReactor();
  main_reactor_->SetReactorType(MainReactor);

  time_wheel_ = std::make_shared<TcpTimeWheel>(main_reactor_, g_timewheel_bucket_num->GetValue(),
                                               g_timewheel_interval->GetValue());

  clear_clent_timer_event_ =
      std::make_shared<TimerEvent>(10000, true, std::bind(&TcpServer::ClearClientTimerFunc, this));
  main_reactor_->GetTimer()->AddTimerEvent(clear_clent_timer_event_);

  LOG_DEBUG << "TcpServer setup on [" << addr_->ToString() << "]";
}

void TcpServer::Start() {
  if (!start_info_.empty()) {
    std::cout << start_info_ << std::endl << std::endl;
  }
  acceptor_ = Socket::CreateTCP(addr_);
  acceptor_->Bind(addr_);
  acceptor_->Listen();
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
    Socket::ptr sock = acceptor_->Accept();
    if (sock == nullptr) {
      Coroutine::Yield();
      continue;
    }
    IOThread *io_thread = io_pool_->GetIoThread();
    TcpConnection::ptr conn = AddClient(io_thread, sock->GetFd());
    conn->InitServer();
    LOG_DEBUG << "tcpconnection address is " << conn.get() << ", and fd is" << sock->GetFd();

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

auto TcpServer::GetPeerAddr() -> Address::ptr { return acceptor_->GetRemoteAddr(); }

auto TcpServer::GetLocalAddr() -> Address::ptr { return addr_; }

auto TcpServer::GetTimeWheel() -> TcpTimeWheel::ptr { return time_wheel_; }

auto TcpServer::GetIoThreadPool() -> IOThreadPool::ptr { return io_pool_; }

auto TcpServer::GetDispatcher() -> AbstractDispatcher::ptr { return dispatcher_; }

auto TcpServer::GetCodec() -> AbstractCodeC::ptr { return codec_; }

}  // namespace tirpc
