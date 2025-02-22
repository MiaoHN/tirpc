#pragma once

#include <memory>
#include <string>
#include <vector>
namespace tirpc {

class TcpBuffer {
 public:
  using ptr = std::shared_ptr<TcpBuffer>;

  explicit TcpBuffer(int size);

  ~TcpBuffer();

  auto Readable() -> int;

  auto Writeable() -> int;

  auto ReadIndex() const -> int;

  auto WriteIndex() const -> int;

  void WriteToBuffer(const char *buf, int size);

  void ReadFromBuffer(std::vector<char> &re, int size);

  void ResizeBuffer(int size);

  void ClearBuffer();

  auto GetSize() -> int;

  auto GetBufferVector() -> std::vector<char>;

  auto GetBufferString() -> std::string;

  void RecycleRead(int index);

  void RecycleWrite(int index);

  void AdjustBuffer();

 private:
  int read_index_{0};
  int write_index_{0};
  int size_{0};

 public:
  std::vector<char> buffer_;
};

}  // namespace tirpc