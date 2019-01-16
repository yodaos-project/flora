#pragma once

#include <stdint.h>

class Connection {
public:
  virtual ~Connection() = default;

  virtual bool send(const void *data, uint32_t size) = 0;

  virtual int32_t recv(void *data, uint32_t size) = 0;

  virtual void close() = 0;

  virtual bool closed() const = 0;
};
