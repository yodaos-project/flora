#pragma once

#include <set>
#include <stdint.h>
#include <string>

typedef struct {
  void *data;
  uint32_t size;
} Frame;

class Adapter {
public:
  Adapter(uint32_t flags) : serialize_flags(flags) {}

  virtual ~Adapter() = default;

  virtual int32_t read() = 0;

  virtual int32_t next_frame(Frame &frame) = 0;

  virtual void write(const void *data, uint32_t size) = 0;

  virtual void close() = 0;

  virtual bool closed() = 0;

  bool declare_method(const std::string &name) {
    auto r = declared_methods.insert(name);
    return r.second;
  }

  void remove_method(const std::string &name) { declared_methods.erase(name); }

  bool has_method(const std::string &name) {
    return declared_methods.find(name) != declared_methods.end();
  }

private:
  std::set<std::string> declared_methods;

public:
  std::string auth_extra;
  uint32_t serialize_flags = 0;
#ifdef FLORA_DEBUG
  uint32_t recv_times = 0;
  uint32_t recv_bytes = 0;
#endif
};
