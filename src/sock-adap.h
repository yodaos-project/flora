#pragma once

#include "adap.h"
#include <mutex>

#define HEADER_SIZE 8

#define SOCK_ADAPTER_SUCCESS 0
// socket已关闭或出错
#define SOCK_ADAPTER_ECLOSED -10001
// 读取的数据不足
#define SOCK_ADAPTER_ENOMORE -10002
// 数据协议格式错误
#define SOCK_ADAPTER_EPROTO -10003
// buf空间不足
#define SOCK_ADAPTER_ENOBUF -10004

class SocketAdapter : public Adapter {
public:
  SocketAdapter(int sock, uint32_t bufsize, uint32_t flags);

  ~SocketAdapter();

  // return:  = 0   success
  //          < 0   error
  int32_t read() override;

  int32_t next_frame(Frame &frame) override;

  void write(const void *data, uint32_t size) override;

  void close() override;

  bool closed() override;

  int socket() const { return socketfd; }

private:
  void close_nolock();

private:
  int socketfd;
  int8_t *buffer = nullptr;
  uint32_t buf_size;
  uint32_t cur_size = 0;
  uint32_t frame_begin = 0;
  std::mutex write_mutex;
};
