#pragma once

#include "caps.h"

#define FLORA_POLL_SUCCESS 0
#define FLORA_POLL_ALREADY_START -1
#define FLORA_POLL_SYSERR -2
#define FLORA_POLL_INVAL -3
#define FLORA_POLL_UNSUPP -4

// options of 'config'
#define FLORA_POLL_OPT_KEEPALIVE_TIMEOUT 1

#define FLORA_DISP_FLAG_MONITOR 1

#ifdef __cplusplus
#include <memory>

namespace flora {

class Dispatcher {
public:
  virtual ~Dispatcher() = default;

  virtual void run(bool blocking = false) = 0;

  virtual void close() = 0;

  static std::shared_ptr<Dispatcher> new_instance(uint32_t flags = 0,
                                                  uint32_t msg_buf_size = 0);
};

class Poll {
public:
  virtual ~Poll() = default;

  virtual int32_t start(std::shared_ptr<Dispatcher> &dispathcer) = 0;

  virtual void stop() = 0;

  virtual void config(uint32_t opt, ...) = 0;

  static std::shared_ptr<Poll> new_instance(const char *uri);
};

} // namespace flora

extern "C" {
#endif

typedef intptr_t flora_poll_t;
typedef intptr_t flora_dispatcher_t;
typedef void (*flora_received_func_t)(flora_dispatcher_t handle,
                                      const char *name, caps_t msg, void *arg);

// --flora dispatcher functions--
// 消息转发

flora_dispatcher_t flora_dispatcher_new(uint32_t flags, uint32_t msg_buf_size);

void flora_dispatcher_delete(flora_dispatcher_t handle);

void flora_dispatcher_run(flora_dispatcher_t handle, int32_t block);

void flora_dispatcher_close(flora_dispatcher_t handle);

// void flora_dispatcher_forward_msg(flora_dispatcher_t handle, const char
// *name,
//                                   caps_t msg);

// void flora_dispatcher_subscribe(flora_dispatcher_t handle, const char *name,
//                                flora_received_func_t callback, void *arg);

// void flora_dispatcher_unsubscribe(flora_dispatcher_t handle, const char
// *name);

// --flora poll functions--
// 侦听连接
// 封装网络通信协议
// 轮询客户端连接消息

// uri: 支持的schema:
//     tcp://$(host):$(port)
//     unix:$(domain_name)         (unix domain socket)
// return:
//      SUCESS
//      INVAL: uri参数不合法
//      UNSUPP: uri指定的协议不支持
int32_t flora_poll_new(const char *uri, flora_poll_t *result);

void flora_poll_delete(flora_poll_t handle);

// msg_buf_size: 大于等于单个msg二进制数据最大长度
// return:
//        SUCCESS
//        INVAL: handle或dispatcher参数为0
//        SYSERR: socket初始化失败
int32_t flora_poll_start(flora_poll_t handle, flora_dispatcher_t dispatcher);

void flora_poll_stop(flora_poll_t handle);

#ifdef __cplusplus
} // extern "C"
#endif
