#pragma once

#include "conn.h"
#include <string>

class SocketConn : public Connection {
public:
  bool connect(const std::string &name);

  bool connect(const std::string &host, int32_t port);

  bool send(const void *data, uint32_t size) override;

  int32_t recv(void *data, uint32_t size) override;

  void close() override;

private:
  int sock = -1;
};
