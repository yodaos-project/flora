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
// 在回调线程中调用call(阻塞模式)
#define FLORA_CLI_EDEADLOCK -9
// monitor模式下，不可调用subscribe/declare_method/post/call
#define FLORA_CLI_EMONITOR -10

#define FLORA_MSGTYPE_INSTANT 0
#define FLORA_MSGTYPE_PERSIST 1
#define FLORA_NUMBER_OF_MSGTYPE 2

#ifdef __cplusplus
#include <functional>
#include <memory>
#include <string>
#include <vector>

#define FLORA_CLI_FLAG_MONITOR 0x1
#define FLORA_CLI_FLAG_MONITOR_DETAIL_SUB 0x2
#define FLORA_CLI_FLAG_MONITOR_DETAIL_DECL 0x4
#define FLORA_CLI_FLAG_MONITOR_DETAIL_POST 0x8
#define FLORA_CLI_FLAG_MONITOR_DETAIL_CALL 0x10
#define FLORA_CLI_FLAG_KEEPALIVE 0x20

#define FLORA_CLI_DEFAULT_BEEP_INTERVAL 50000
#define FLORA_CLI_DEFAULT_NORESP_TIMEOUT 100000

namespace flora {

typedef struct {
  int32_t ret_code;
  std::shared_ptr<Caps> data;
  std::string extra;
} Response;

// NOTE: the Reply class methods not threadsafe
class Reply {
public:
  virtual ~Reply() = default;

  virtual void write_code(int32_t code) = 0;

  virtual void write_data(std::shared_ptr<Caps> &data) = 0;

  virtual void end() = 0;

  virtual void end(int32_t code) = 0;

  virtual void end(int32_t code, std::shared_ptr<Caps> &data) = 0;
};

class ClientCallback;
class MonitorCallback;

class ClientOptions {
public:
  uint32_t bufsize = 0;
  uint32_t flags = 0;
  // effective when FLORA_CLI_FLAG_KEEPALIVE is set
  uint32_t beep_interval = FLORA_CLI_DEFAULT_BEEP_INTERVAL;
  uint32_t noresp_timeout = FLORA_CLI_DEFAULT_NORESP_TIMEOUT;
};

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

  static int32_t connect(const char *uri, ClientCallback *ccb,
                         MonitorCallback *mcb, ClientOptions *opts,
                         std::shared_ptr<Client> &result);
};

class ClientCallback {
public:
  virtual ~ClientCallback() = default;

  virtual void recv_post(const char *name, uint32_t msgtype,
                         std::shared_ptr<Caps> &msg) {}

  virtual void recv_call(const char *name, std::shared_ptr<Caps> &msg,
                         std::shared_ptr<Reply> &reply) {}

  virtual void disconnected() {}
};

class MonitorListItem {
public:
  uint32_t id;
  int32_t pid;
  std::string name;
  uint32_t flags;
};

class MonitorSubscriptionItem {
public:
  std::string name;
  // id of MonitorListItem
  uint32_t id;
};

class MonitorPostInfo {
public:
  std::string name;
  // id of MonitorListItem, sender
  uint32_t from;
};

class MonitorCallInfo {
public:
  std::string name;
  // id of MonitorListItem, sender
  uint32_t from;
  std::string target;
  int32_t err = FLORA_CLI_SUCCESS;
};

typedef MonitorSubscriptionItem MonitorDeclarationItem;

class MonitorCallback {
public:
  virtual ~MonitorCallback() = default;

  virtual void list_all(std::vector<MonitorListItem> &items) {}

  virtual void list_add(MonitorListItem &item) {}

  virtual void list_remove(uint32_t id) {}

  virtual void sub_all(std::vector<MonitorSubscriptionItem> &items) {}

  virtual void sub_add(MonitorSubscriptionItem &item) {}

  virtual void sub_remove(MonitorSubscriptionItem &item) {}

  virtual void decl_all(std::vector<MonitorDeclarationItem> &items) {}

  virtual void decl_add(MonitorDeclarationItem &item) {}

  virtual void decl_remove(MonitorDeclarationItem &item) {}

  virtual void post(MonitorPostInfo &info) {}

  virtual void call(MonitorCallInfo &info) {}

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
typedef intptr_t flora_call_reply_t;

// 'call'请求的接收端
// 在此函数中填充'reply'变量，客户端将把'reply'数据发回给'call'请求发送端
typedef void (*flora_cli_recv_call_func_t)(const char *name, caps_t msg,
                                           void *arg, flora_call_reply_t reply);
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

typedef struct {
  uint32_t id;
  int32_t pid;
  const char *name;
} flora_cli_monitor_list_item;
typedef struct {
  const char *name;
  // id of flora_cli_monitor_list_item
  uint32_t id;
} flora_cli_monitor_sub_item;
typedef flora_cli_monitor_sub_item flora_cli_monitor_decl_item;
typedef struct {
  const char *name;
  // id of flora_cli_monitor_list_item, sender
  uint32_t from;
} flora_cli_monitor_post_info;
typedef struct {
  const char *name;
  // id of flora_cli_monitor_list_item, sender
  uint32_t from;
  const char *target;
} flora_cli_monitor_call_info;
typedef void (*flora_cli_monitor_list_all_func_t)(
    flora_cli_monitor_list_item *items, uint32_t size);
typedef void (*flora_cli_monitor_list_add_func_t)(
    flora_cli_monitor_list_item *item);
typedef void (*flora_cli_monitor_list_remove_func_t)(uint32_t id);
typedef void (*flora_cli_monitor_sub_all_func_t)(
    flora_cli_monitor_sub_item *items, uint32_t size);
typedef void (*flora_cli_monitor_sub_add_func_t)(
    flora_cli_monitor_sub_item *item);
typedef void (*flora_cli_monitor_sub_remove_func_t)(
    flora_cli_monitor_sub_item *item);
typedef void (*flora_cli_monitor_decl_all_func_t)(
    flora_cli_monitor_decl_item *items, uint32_t size);
typedef void (*flora_cli_monitor_decl_add_func_t)(
    flora_cli_monitor_decl_item *item);
typedef void (*flora_cli_monitor_decl_remove_func_t)(
    flora_cli_monitor_decl_item *item);
typedef void (*flora_cli_monitor_post_func_t)(
    flora_cli_monitor_post_info *info);
typedef void (*flora_cli_monitor_call_func_t)(
    flora_cli_monitor_call_info *info);
typedef struct {
  flora_cli_monitor_list_all_func_t list_all;
  flora_cli_monitor_list_add_func_t list_add;
  flora_cli_monitor_list_remove_func_t list_remove;
  flora_cli_monitor_sub_all_func_t sub_all;
  flora_cli_monitor_sub_add_func_t sub_add;
  flora_cli_monitor_sub_remove_func_t sub_remove;
  flora_cli_monitor_decl_all_func_t decl_all;
  flora_cli_monitor_decl_add_func_t decl_add;
  flora_cli_monitor_decl_remove_func_t decl_remove;
  flora_cli_monitor_post_func_t post;
  flora_cli_monitor_call_func_t call;
  flora_cli_disconnected_func_t disconnected;
} flora_cli_monitor_callback_t;

// uri: 支持的schema:
//     tcp://$(host):$(port)/<#extra>
//     unix:$(domain_name)<#extra>         (unix domain socket)
// #extra为客户端自定义字符串，用于粗略标识自身身份，不保证全局唯一性
// async: 异步模式开关
// result: 返回flora_cli_t对象
int32_t flora_cli_connect(const char *uri, flora_cli_callback_t *callback,
                          void *arg, uint32_t msg_buf_size,
                          flora_cli_t *result);

int32_t flora_cli_connect2(const char *uri,
                           flora_cli_monitor_callback_t *callback, void *arg,
                           uint32_t msg_buf_size, flora_cli_t *result);

typedef struct {
  uint32_t bufsize;
  uint32_t flags;
  uint32_t beep_interval;
  uint32_t noresp_timeout;
} flora_cli_options;
int32_t flora_cli_connect3(const char *uri, flora_cli_callback_t *ccb,
                           flora_cli_monitor_callback_t *mcb, void *arg,
                           flora_cli_options *opts, flora_cli_t *result);

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

void flora_call_reply_write_code(flora_call_reply_t reply, int32_t code);

void flora_call_reply_write_data(flora_call_reply_t reply, caps_t data);

void flora_call_reply_end(flora_call_reply_t reply);

#ifdef __cplusplus
} // namespace flora
#endif
