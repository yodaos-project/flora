#pragma once

#include "caps.h"

#define FLORA_CLI_SUCCESS 0
// 认证过程失败(版本号不支持)
#define FLORA_CLI_EAUTH -1
// 参数不合法
#define FLORA_CLI_EINVAL -2
// 连接错误
#define FLORA_CLI_ECONN -3
// 'get'请求超时
#define FLORA_CLI_ETIMEOUT -4
// 'get'请求目标不存在
#define FLORA_CLI_ENEXISTS -5
// 客户端id已被占用
// uri '#' 字符后面的字符串是客户端id
#define FLORA_CLI_EDUPID -6
// 缓冲区大小不足
#define FLORA_CLI_EINSUFF_BUF -7
// 客户端主动关闭
#define FLORA_CLI_ECLOSED -8

#define FLORA_MSGTYPE_INSTANT 0
#define FLORA_MSGTYPE_PERSIST 1
#define FLORA_NUMBER_OF_MSGTYPE 2

#ifdef __cplusplus
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace flora {

typedef struct {
  int32_t ret_code;
  std::shared_ptr<Caps> data;
  std::string extra;
} Response;

typedef std::vector<Response> ResponseArray;

class Reply {
public:
  int32_t ret_code = FLORA_CLI_SUCCESS;
  std::shared_ptr<Caps> data;
};

class ClientCallback;

class Client {
public:
  virtual ~Client() = default;

  virtual int32_t subscribe(const char *name) = 0;

  virtual int32_t unsubscribe(const char *name) = 0;

  virtual int32_t declare_method(const char *name) = 0;

  virtual int32_t remove_method(const char *name) = 0;

  virtual int32_t post(const char *name, std::shared_ptr<Caps> &msg,
                       uint32_t msgtype) = 0;

  virtual int32_t call(const char *name, std::shared_ptr<Caps> &msg,
                       const char *target, Response &reply,
                       uint32_t timeout = 0) = 0;

  virtual int32_t call(const char *name, std::shared_ptr<Caps> &msg,
                       const char *target,
                       std::function<void(int32_t, Response &)> &&cb,
                       uint32_t timeout = 0) = 0;

  virtual int32_t call(const char *name, std::shared_ptr<Caps> &msg,
                       const char *target,
                       std::function<void(int32_t, Response &)> &cb,
                       uint32_t timeout = 0) = 0;

  static int32_t connect(const char *uri, ClientCallback *cb,
                         uint32_t msg_buf_size,
                         std::shared_ptr<Client> &result);
};

class ClientCallback {
public:
  virtual ~ClientCallback() = default;

  virtual void recv_post(const char *name, uint32_t msgtype,
                         std::shared_ptr<Caps> &msg) {}

  virtual void recv_call(const char *name, std::shared_ptr<Caps> &msg,
                         Reply &reply) {}

  virtual void disconnected() {}
};

} // namespace flora

extern "C" {
#endif

typedef intptr_t flora_cli_t;
typedef void (*flora_cli_disconnected_func_t)(void *arg);
// msgtype: INSTANT | PERSIST
// 'msg': 注意，必须在函数未返回时读取msg，函数返回后读取msg非法
typedef void (*flora_cli_recv_post_func_t)(const char *name, uint32_t msgtype,
                                           caps_t msg, void *arg);
typedef struct {
  int32_t ret_code;
  caps_t data;
} flora_call_reply;
// 'call'请求的接收端
// 在此函数中填充'reply'变量，客户端将把'reply'数据发回给'call'请求发送端
typedef void (*flora_cli_recv_call_func_t)(const char *name, caps_t msg,
                                           void *arg, flora_call_reply *reply);
typedef struct {
  flora_cli_recv_post_func_t recv_post;
  flora_cli_recv_call_func_t recv_call;
  flora_cli_disconnected_func_t disconnected;
} flora_cli_callback_t;
typedef struct {
  // FLORA_CLI_SUCCESS: 成功
  // 其它: 此请求服务端自定义错误码
  int32_t ret_code;
  caps_t data;
  // 回应请求的服务端自定义标识
  // 可能为空字串
  char *extra;
} flora_call_result;
typedef void (*flora_call_callback_t)(int32_t rescode,
                                      flora_call_result *result, void *arg);

// uri: 支持的schema:
//     tcp://$(host):$(port)/<#extra>
//     unix:$(domain_name)<#extra>         (unix domain socket)
// #extra为客户端自定义字符串，用于粗略标识自身身份，不保证全局唯一性
// async: 异步模式开关
// result: 返回flora_cli_t对象
int32_t flora_cli_connect(const char *uri, /*int32_t async,*/
                          flora_cli_callback_t *callback, void *arg,
                          uint32_t msg_buf_size, flora_cli_t *result);

void flora_cli_delete(flora_cli_t handle);

int32_t flora_cli_subscribe(flora_cli_t handle, const char *name);

int32_t flora_cli_unsubscribe(flora_cli_t handle, const char *name);

int32_t flora_cli_declare_method(flora_cli_t handle, const char *name);

int32_t flora_cli_remove_method(flora_cli_t handle, const char *name);

// msgtype: INSTANT | PERSIST
int32_t flora_cli_post(flora_cli_t handle, const char *name, caps_t msg,
                       uint32_t msgtype);

int32_t flora_cli_call(flora_cli_t handle, const char *name, caps_t msg,
                       const char *target, flora_call_result *result,
                       uint32_t timeout);

int32_t flora_cli_call_nb(flora_cli_t handle, const char *name, caps_t msg,
                          const char *target, flora_call_callback_t cb,
                          void *arg, uint32_t timeout);

void flora_result_delete(flora_call_result *result);

#ifdef __cplusplus
} // namespace flora
#endif
