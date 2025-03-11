#include "tirpc/net/tcp/tcp_connection.hpp"

#include <asm-generic/errno.h>
#include <sys/socket.h>
#include <unistd.h>
#include <cstring>
#include <utility>

#include "tirpc/common/log.hpp"
#include "tirpc/coroutine/coroutine_hook.hpp"
#include "tirpc/coroutine/coroutine_pool.hpp"
#include "tirpc/net/base/timer.hpp"
#include "tirpc/net/rpc/rpc_codec.hpp"
#include "tirpc/net/rpc/rpc_data.hpp"
#include "tirpc/net/tcp/abstract_data.hpp"
#include "tirpc/net/tcp/abstract_slot.hpp"
#include "tirpc/net/tcp/tcp_client.hpp"
#include "tirpc/net/tcp/tcp_connection_time_wheel.hpp"
#include "tirpc/net/tcp/tcp_server.hpp"

namespace tirpc {

TcpConnection::TcpConnection(TcpServer *tcp_svr, IOThread *io_thread, int fd, int buff_size, Address::ptr peer_addr)
    : io_thread_(io_thread), fd_(fd), peer_addr_(std::move(peer_addr)) {
  reactor_ = io_thread_->GetReactor();

  // LOG_DEBUG << "state_=[" << state_ << "], =" << fd;
  tcp_svr_ = tcp_svr;

  codec_ = tcp_svr_->GetCodec();
  fd_event_ = FdEventContainer::GetFdContainer()->GetFdEvent(fd);
  fd_event_->SetReactor(reactor_);
  InitBuffer(buff_size);
  loop_cor_ = GetCoroutinePool()->GetCoroutineInstanse();
  state_ = Connected;
  LOG_DEBUG << "succ create tcp connection[" << state_ << "], fd=" << fd;
}

TcpConnection::TcpConnection(TcpClient *tcp_cli, Reactor *reactor, int fd, int buff_size, Address::ptr peer_addr)
    : fd_(fd), state_(NotConnected), connection_type_(ClientConnection), peer_addr_(std::move(peer_addr)) {
  reactor_ = reactor;

  tcp_cli_ = tcp_cli;

  codec_ = tcp_cli_->GetCodeC();

  fd_event_ = FdEventContainer::GetFdContainer()->GetFdEvent(fd);
  fd_event_->SetReactor(reactor_);
  InitBuffer(buff_size);

  LOG_DEBUG << "succ create tcp connection[NotConnected]";
}

void TcpConnection::InitServer() {
  RegisterToTimeWheel();
  loop_cor_->SetCallBack(std::bind(&TcpConnection::MainServerLoopCorFunc, this));
}

void TcpConnection::SetUpServer() { reactor_->AddCoroutine(loop_cor_); }

void TcpConnection::RegisterToTimeWheel() {
  auto cb = [](TcpConnection::ptr conn) { conn->ShutdownConnection(); };
  TcpTimeWheel::TcpConnectionSlot::ptr tmp = std::make_shared<AbstractSlot<TcpConnection>>(shared_from_this(), cb);
  weak_slot_ = tmp;
  tcp_svr_->FreshTcpConnection(tmp);
}

void TcpConnection::SetUpClient() { SetState(Connected); }

TcpConnection::~TcpConnection() {
  if (connection_type_ == ServerConnection) {
    GetCoroutinePool()->ReturnCoroutine(loop_cor_);
  }

  LOG_DEBUG << "~TcpConnection, fd=" << fd_;
}

void TcpConnection::InitBuffer(int size) {
  // 初始化缓冲区大小
  write_buffer_ = std::make_shared<TcpBuffer>(size);
  read_buffer_ = std::make_shared<TcpBuffer>(size);
}

void TcpConnection::MainServerLoopCorFunc() {
  while (!stop_) {
    Input();

    Execute();

    Output();
  }
  LOG_INFO << "this connection has already end loop";
}

void TcpConnection::Input() {
  if (is_over_time_) {
    LOG_INFO << "over timer, skip input progress";
    return;
  }
  TcpConnectionState state = GetState();
  if (state == Closed || state == NotConnected) {
    return;
  }

  bool read_all = false;
  bool close_flag = false;
  int count = 0;

  while (!read_all) {
    if (read_buffer_->Writeable() == 0) {
      read_buffer_->ResizeBuffer(2 * read_buffer_->GetSize());
    }

    int read_count = read_buffer_->Writeable();
    int write_index = read_buffer_->WriteIndex();

    LOG_DEBUG << "read_buffer_ size=" << read_buffer_->GetBufferVector().size() << "rd=" << read_buffer_->ReadIndex()
             << "wd=" << read_buffer_->WriteIndex();
    int rt = recv_hook(fd_, &(read_buffer_->buffer_[write_index]), read_count, 0);
    if (rt > 0) {
      read_buffer_->RecycleWrite(rt);
    }
    LOG_DEBUG << "read_buffer_ size=" << read_buffer_->GetBufferVector().size() << "rd=" << read_buffer_->ReadIndex()
             << "wd=" << read_buffer_->WriteIndex();

    LOG_DEBUG << "read data back, fd=" << fd_;
    count += rt;
    if (is_over_time_) {
      LOG_INFO << "over timer, now break read function";
      break;
    }
    if (rt <= 0) {
      LOG_DEBUG << "rt <= 0";
      close_flag = true;
      break;
    }
    if (rt == read_count) {
      LOG_DEBUG << "read_count == rt";
      // is is possible read more data, should continue read
      continue;
    }
    if (rt < read_count) {
      LOG_DEBUG << "read_count > rt";
      // read all data in socket buffer, skip out loop
      read_all = true;
      break;
    }
  }
  if (close_flag) {
    ClearClient();
    LOG_DEBUG << "peer close, now yield current coroutine, wait main thread clear this TcpConnection";
    // Coroutine::GetCurrentCoroutine()->SetCanResume(false);
    Coroutine::Yield();
    return;
  }

  if (is_over_time_) {
    return;
  }

  if (!read_all) {
    LOG_ERROR << "not read all data in socket buffer";
  }
  if (connection_type_ == ServerConnection) {
    TcpTimeWheel::TcpConnectionSlot::ptr tmp = weak_slot_.lock();
    if (tmp) {
      tcp_svr_->FreshTcpConnection(tmp);
    }
  }
}

void TcpConnection::Execute() {
  // LOG_DEBUG << "begin to do execute";

  // it only server do this
  while (read_buffer_->Readable() > 0) {
    auto data = codec_->GenDataPtr();

    codec_->Decode(read_buffer_.get(), data.get());
    // LOG_DEBUG << "parse service_name=" << pb_struct.service_full_name;
    if (!data->decode_succ_) {
      LOG_ERROR << "it parse request error of fd " << fd_;
      break;
    }
    // LOG_DEBUG << "it parse request success";
    if (connection_type_ == ServerConnection) {
      // LOG_DEBUG << "to dispatch this package";
      tcp_svr_->GetDispatcher()->Dispatch(data.get(), this);
      // LOG_DEBUG << "contine parse next package";
    } else if (connection_type_ == ClientConnection) {
      std::shared_ptr<TinyPbStruct> tmp = std::dynamic_pointer_cast<TinyPbStruct>(data);
      if (tmp) {
        reply_datas_.insert(std::make_pair(tmp->msg_seq_, tmp));
      }
    }
  }
}

void TcpConnection::Output() {
  if (is_over_time_) {
    LOG_INFO << "over timer, skip output progress";
    return;
  }
  while (true) {
    TcpConnectionState state = GetState();
    if (state != Connected) {
      break;
    }

    if (write_buffer_->Readable() == 0) {
      LOG_DEBUG << "app buffer of fd[" << fd_ << "] no data to write, to yiled this coroutine";
      break;
    }

    int total_size = write_buffer_->Readable();
    int read_index = write_buffer_->ReadIndex();
    int rt = send_hook(fd_, &(write_buffer_->buffer_[read_index]), total_size, 0);
    // LOG_INFO << "write end";
    if (rt <= 0) {
      LOG_ERROR << "write empty, error=" << strerror(errno);
    }

    LOG_DEBUG << "succ write " << rt << " bytes";
    write_buffer_->RecycleRead(rt);
    LOG_DEBUG << "recycle write index =" << write_buffer_->WriteIndex() << ", read_index =" << write_buffer_->ReadIndex()
             << "readable = " << write_buffer_->Readable();
    LOG_DEBUG << "send[" << rt << "] bytes data to [" << peer_addr_->ToString() << "], fd [" << fd_ << "]";
    if (write_buffer_->Readable() <= 0) {
      // LOG_INFO << "send all data, now unregister write event on reactor and yield Coroutine";
      LOG_DEBUG << "send all data, now unregister write event and break";
      // fd_event_->DelListenEvents(IOEvent::WRITE);
      break;
    }

    if (is_over_time_) {
      LOG_INFO << "over timer, now break write function";
      break;
    }
  }
}

void TcpConnection::ClearClient() {
  LOG_DEBUG << "clear client...";
  if (GetState() == Closed) {
    LOG_DEBUG << "this client has closed";
    return;
  }
  // first unregister epoll event
  fd_event_->UnregisterFromReactor();

  // stop read and write cor
  stop_ = true;

  ::close(fd_event_->GetFd());
  SetState(Closed);
}

void TcpConnection::ShutdownConnection() {
  TcpConnectionState state = GetState();
  if (state == Closed || state == NotConnected) {
    LOG_DEBUG << "this client has closed";
    return;
  }
  SetState(HalfClosing);
  LOG_DEBUG << "shutdown conn[" << peer_addr_->ToString() << "], fd=" << fd_;

  // call sys shutdown to send FIN
  // wait client done something, client will send FIN
  // and fd occur read event but byte count is 0
  // then will call clearClient to set CLOSED
  // IOThread::MainLoopTimerFunc will delete CLOSED connection
  shutdown(fd_event_->GetFd(), SHUT_RDWR);
}

auto TcpConnection::GetInBuffer() -> TcpBuffer * { return read_buffer_.get(); }

auto TcpConnection::GetOutBuffer() -> TcpBuffer * { return write_buffer_.get(); }

auto TcpConnection::GetResPackageData(const std::string &msg_req, TinyPbStruct::pb_ptr &pb_struct) -> bool {
  auto it = reply_datas_.find(msg_req);
  if (it != reply_datas_.end()) {
    LOG_DEBUG << "return a resdata";
    pb_struct = it->second;
    reply_datas_.erase(it);
    return true;
  }
  LOG_DEBUG << msg_req << "|reply data not exist";
  return false;
}

auto TcpConnection::GetCodec() const -> AbstractCodeC::ptr { return codec_; }

auto TcpConnection::GetState() -> TcpConnectionState {
  RWMutex::ReadLocker lock(mutex_);
  return state_;
}

void TcpConnection::SetState(const TcpConnectionState &state) {
  RWMutex::WriteLocker lock(mutex_);
  state_ = std::move(state);
}

void TcpConnection::SetOverTimeFlag(bool value) { is_over_time_ = value; }

auto TcpConnection::GetOverTimerFlag() -> bool { return is_over_time_; }

auto TcpConnection::GetCoroutine() -> Coroutine::ptr { return loop_cor_; }

}  // namespace tirpc
