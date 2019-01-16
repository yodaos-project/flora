#pragma once

#include "conn.h"
#include <mutex>
#include <string>

class SocketConn : public Connection {
public:
  ~SocketConn();

  bool connect(const std::string &name);

  bool connect(const std::string &host, int32_t port);

  bool send(const void *data, uint32_t size) override;

  int32_t recv(void *data, uint32_t size) override;

  void close() override;

  bool closed() const override { return sock < 0; }

private:
  int sock = -1;
  bool sock_ready = false;
  std::mutex write_mutex;
};
