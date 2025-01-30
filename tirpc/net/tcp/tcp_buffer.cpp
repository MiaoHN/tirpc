#include "tirpc/net/tcp/tcp_buffer.hpp"

#include <unistd.h>
#include <cstring>

#include "tirpc/common/log.hpp"

namespace tirpc {

TcpBuffer::TcpBuffer(int size) { buffer_.resize(size); }

TcpBuffer::~TcpBuffer() = default;

auto TcpBuffer::Readable() -> int { return write_index_ - read_index_; }

auto TcpBuffer::Writeable() -> int { return buffer_.size() - write_index_; }

auto TcpBuffer::ReadIndex() const -> int { return read_index_; }

auto TcpBuffer::WriteIndex() const -> int { return write_index_; }

// int TcpBuffer::readFromSocket(int sockfd) {
// if (Writeable() == 0) {
// buffer_.resize(2 * size_);
// }
// int rt = read(sockfd, &buffer_[write_index_], Writeable());
// if (rt >= 0) {
// write_index_ += rt;
// }
// return rt;
// }

void TcpBuffer::ResizeBuffer(int size) {
  std::vector<char> tmp(size);
  int c = std::min(size, Readable());
  memcpy(&tmp[0], &buffer_[read_index_], c);

  buffer_.swap(tmp);
  read_index_ = 0;
  write_index_ = read_index_ + c;
}

void TcpBuffer::WriteToBuffer(const char *buf, int size) {
  if (size > Writeable()) {
    int new_size = static_cast<int>(1.5 * (write_index_ + size));
    ResizeBuffer(new_size);
  }
  memcpy(&buffer_[write_index_], buf, size);
  write_index_ += size;
}

void TcpBuffer::ReadFromBuffer(std::vector<char> &re, int size) {
  if (Readable() == 0) {
    DebugLog << "read buffer empty!";
    return;
  }
  int read_size = Readable() > size ? size : Readable();
  std::vector<char> tmp(read_size);

  // std::copy(read_index_, read_index_ + read_size, tmp);
  memcpy(&tmp[0], &buffer_[read_index_], read_size);
  re.swap(tmp);
  read_index_ += read_size;
  AdjustBuffer();
}

void TcpBuffer::AdjustBuffer() {
  if (read_index_ > static_cast<int>(buffer_.size() / 3)) {
    std::vector<char> new_buffer(buffer_.size());

    int count = Readable();
    // std::copy(&buffer_[read_index_], Readable(), &new_buffer);
    memcpy(&new_buffer[0], &buffer_[read_index_], count);

    buffer_.swap(new_buffer);
    write_index_ = count;
    read_index_ = 0;
    new_buffer.clear();
  }
}

auto TcpBuffer::GetSize() -> int { return buffer_.size(); }

void TcpBuffer::ClearBuffer() {
  buffer_.clear();
  read_index_ = 0;
  write_index_ = 0;
}

void TcpBuffer::RecycleRead(int index) {
  int j = read_index_ + index;
  if (j > static_cast<int>(buffer_.size())) {
    ErrorLog << "recycleRead error";
    return;
  }
  read_index_ = j;
  AdjustBuffer();
}

void TcpBuffer::RecycleWrite(int index) {
  int j = write_index_ + index;
  if (j > static_cast<int>(buffer_.size())) {
    ErrorLog << "recycleWrite error";
    return;
  }
  write_index_ = j;
  AdjustBuffer();
}

// const char* TcpBuffer::getBuffer() {
//   char* tmp;
//   memcpy(&tmp, &buffer_[read_index_], Readable());
//   return tmp;
// }

auto TcpBuffer::GetBufferString() -> std::string {
  std::string re(Readable(), '0');
  memcpy(&re[0], &buffer_[read_index_], Readable());
  return re;
}

auto TcpBuffer::GetBufferVector() -> std::vector<char> { return buffer_; }

}  // namespace tirpc
