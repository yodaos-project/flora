#pragma once

#include <string>
#include "conn.h"

class TCPConn : public Connection {
public:
  bool connect(const std::string& host, int32_t port);

  bool send(const void* data, uint32_t size);

  int32_t recv(void* data, uint32_t size);

  void close();

private:
  int sock = -1;
};
