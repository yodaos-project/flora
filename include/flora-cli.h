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

#define FLORA_MSGTYPE_INSTANT 0
#define FLORA_MSGTYPE_PERSIST 1
#define FLORA_MSGTYPE_REQUEST 2
#define FLORA_NUMBER_OF_MSGTYPE 3

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

  virtual int32_t post(const char *name, std::shared_ptr<Caps> &msg,
                       uint32_t msgtype) = 0;

  virtual int32_t get(const char *name, std::shared_ptr<Caps> &msg,
                      ResponseArray &replys, uint32_t timeout = 0) = 0;

  virtual int32_t get(const char *name, std::shared_ptr<Caps> &msg,
                      std::function<void(ResponseArray &)> &&cb) = 0;

  virtual int32_t get(const char *name, std::shared_ptr<Caps> &msg,
                      std::function<void(ResponseArray &)> &cb) = 0;

  static int32_t connect(const char *uri, ClientCallback *cb,
                         uint32_t msg_buf_size,
                         std::shared_ptr<Client> &result);
};

class ClientCallback {
public:
  virtual ~ClientCallback() = default;

  virtual void recv_post(const char *name, uint32_t msgtype,
                         std::shared_ptr<Caps> &msg) {}

  virtual void recv_get(const char *name, std::shared_ptr<Caps> &msg,
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
// 'get'请求的接收端
// 在此函数中填充'resp'变量，客户端将把'resp'数据发回给'get'请求发送端
//
// return:
//     FLORA_CLI_SUCCESS,  客户端将会把'resp'指定的对象发回给'get'请求发送端
//     其它,  返回值为自定义错误码，客户端将把此错误码发回给'get'请求发送端
typedef int32_t (*flora_cli_recv_get_func_t)(const char *name, caps_t msg,
                                             void *arg, caps_t *resp);
typedef struct {
  flora_cli_recv_post_func_t recv_post;
  flora_cli_recv_get_func_t recv_get;
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
} flora_get_result;
typedef void (*flora_get_callback_t)(flora_get_result *results, uint32_t count);

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

// msgtype: INSTANT | PERSIST | REQUEST
int32_t flora_cli_subscribe(flora_cli_t handle, const char *name);

// msgtype: INSTANT | PERSIST | REQUEST
int32_t flora_cli_unsubscribe(flora_cli_t handle, const char *name);

// msgtype: INSTANT | PERSIST
int32_t flora_cli_post(flora_cli_t handle, const char *name, caps_t msg,
                       uint32_t msgtype);

// send REQUEST msg and recv results
int32_t flora_cli_get(flora_cli_t handle, const char *name, caps_t msg,
                      flora_get_result **results, uint32_t *res_buf_size,
                      uint32_t timeout);

int32_t flora_cli_get_nb(flora_cli_t handle, const char *name, caps_t msg,
                         flora_get_callback_t cb);

void flora_result_delete(flora_get_result *results, uint32_t size);

#ifdef __cplusplus
} // namespace flora
#endif
