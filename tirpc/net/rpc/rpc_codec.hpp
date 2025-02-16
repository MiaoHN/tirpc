#pragma once

#include <cstdint>

#include "tirpc/net/rpc/rpc_data.hpp"
#include "tirpc/net/tcp/abstract_codec.hpp"
#include "tirpc/net/tcp/abstract_data.hpp"

namespace tirpc {

class TinyPbCodeC : public AbstractCodeC {
 public:
  // typedef std::shared_ptr<TinyPbCodeC> ptr;

  TinyPbCodeC();

  ~TinyPbCodeC() override;

  // overwrite
  void Encode(TcpBuffer *buf, AbstractData *data) override;

  // overwrite
  void Decode(TcpBuffer *buf, AbstractData *data) override;

  // overwrite
  auto GenDataPtr() -> AbstractData::ptr override;

  auto EncodePbData(TinyPbStruct *data, int &len) -> const char *;
};

}  // namespace tirpc
