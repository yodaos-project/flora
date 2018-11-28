#pragma once

#include "adap.h"

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
  int32_t read();

  int32_t next_frame(Frame& frame);

  void write(const void* data, uint32_t size);

  void close();

  bool closed() const;

private:
  int socket;
  int8_t* buffer = nullptr;
  uint32_t buf_size;
  uint32_t cur_size = 0;
  uint32_t frame_begin = 0;
};
