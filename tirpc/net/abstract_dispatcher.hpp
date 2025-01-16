#pragma once

#include <google/protobuf/service.h>
#include <memory>

#include "tirpc/net/abstract_data.hpp"

namespace tirpc {

class TcpConnection;

class AbstractDispatcher {
 public:
  using ptr = std::shared_ptr<AbstractDispatcher>;

  AbstractDispatcher() = default;

  virtual ~AbstractDispatcher() = default;

  virtual void Dispatch(AbstractData *data, TcpConnection *conn) = 0;
};

}  // namespace tirpc
