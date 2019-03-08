#pragma once

#include "conn.h"
#include <mutex>
#include <string>

class SocketConn : public Connection {
public:
  SocketConn(uint32_t rtmo);

  ~SocketConn();

  bool connect(const std::string &name);

  bool connect(const std::string &host, int32_t port);

  bool send(const void *data, uint32_t size) override;

  // return: -1  socket error
  //         -2  read timeout
  int32_t recv(void *data, uint32_t size) override;

  void close() override;

  bool closed() const override { return !sock_ready; }

private:
  int sock = -1;
  uint32_t recv_timeout;
  bool sock_ready = false;
  std::mutex write_mutex;
};
