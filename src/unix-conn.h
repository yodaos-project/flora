#pragma once

#include <string>
#include "conn.h"

class UnixConn : public Connection {
public:
  bool connect(const std::string& name);

  bool send(const void* data, uint32_t size);

  int32_t recv(void* data, uint32_t size);

  void close();

private:
  int sock = -1;
};
