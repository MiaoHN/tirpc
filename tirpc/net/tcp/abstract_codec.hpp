#pragma once

#include <memory>

#include "tirpc/net/tcp/abstract_data.hpp"

namespace tirpc {

enum ProtocalType { TinyPb_Protocal = 1, Http_Protocal = 2 };

class TcpBuffer;

class AbstractCodeC {
 public:
  using ptr = std::shared_ptr<AbstractCodeC>;

  AbstractCodeC() = default;

  virtual ~AbstractCodeC() = default;

  virtual void Encode(TcpBuffer *buf, AbstractData *data) = 0;

  virtual void Decode(TcpBuffer *buf, AbstractData *data) = 0;

  virtual auto GenDataPtr() -> AbstractData::ptr = 0;
};

}  // namespace tirpc