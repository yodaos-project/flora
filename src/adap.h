#pragma once

#include <stdint.h>
#include <string>

typedef struct {
  void* data;
  uint32_t size;
} Frame;

class Adapter {
public:
  virtual ~Adapter() = default;

  virtual int32_t read() = 0;

  virtual int32_t next_frame(Frame& frame) = 0;

  virtual void write(const void* data, uint32_t size) = 0;

  virtual void close() = 0;

  virtual bool closed() const = 0;

public:
  std::string auth_extra;
#ifdef FLORA_DEBUG
  uint32_t recv_times = 0;
  uint32_t recv_bytes = 0;
#endif
};
