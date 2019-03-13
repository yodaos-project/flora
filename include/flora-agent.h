#pragma once

#include "caps.h"
#include "flora-cli.h"
#include <stdarg.h>

/**
 * flora client代理工具
 *
 * 包装flora client api，更易于使用
 * 自动重连
 * 更易用的消息回调
 */

// config(KEY, string uri)
#define FLORA_AGENT_CONFIG_URI 0
// config(KEY, uint32_t bufsize)
#define FLORA_AGENT_CONFIG_BUFSIZE 1
// config(KEY, uint32_t interval)
#define FLORA_AGENT_CONFIG_RECONN_INTERVAL 2
// config(KEY, uint32_t flags, MonitorCallback *cb)
//   flags: FLORA_CLI_FLAG_MONITOR_*  (see flora-cli.h)
#define FLORA_AGENT_CONFIG_MONITOR 3
// config(KEY, uint32_t interval, uint32_t timeout)
//   interval: interval of send beep packet
//   timeout: timeout of flora service no response
#define FLORA_AGENT_CONFIG_KEEPALIVE 4

#ifdef __cplusplus

#include <chrono>
#include <condition_variable>
#include <functional>
#include <map>
#include <mutex>
#include <thread>

namespace flora {

typedef std::function<void(const char *, std::shared_ptr<Caps> &, uint32_t)>
    PostHandler;
typedef std::function<void(const char *, std::shared_ptr<Caps> &,
                           std::shared_ptr<Reply> &)>
    CallHandler;
typedef std::map<std::string, PostHandler> PostHandlerMap;
typedef std::map<std::string, CallHandler> CallHandlerMap;

class Agent : public ClientCallback {
public:
  ~Agent();

  void config(uint32_t key, ...);

  void config(uint32_t key, va_list ap);

  void subscribe(const char *name, PostHandler &&cb);

  void subscribe(const char *name, PostHandler &cb);

  void unsubscribe(const char *name);

  void declare_method(const char *name, CallHandler &&cb);

  void declare_method(const char *name, CallHandler &cb);

  void remove_method(const char *name);

  void start(bool block = false);

  void close();

  int32_t post(const char *name, std::shared_ptr<Caps> &msg,
               uint32_t msgtype = FLORA_MSGTYPE_INSTANT);

  int32_t call(const char *name, std::shared_ptr<Caps> &msg, const char *target,
               Response &response, uint32_t timeout = 0);

  int32_t call(const char *name, std::shared_ptr<Caps> &msg, const char *target,
               std::function<void(int32_t, Response &)> &&cb,
               uint32_t timeout = 0);

  int32_t call(const char *name, std::shared_ptr<Caps> &msg, const char *target,
               std::function<void(int32_t, Response &)> &cb,
               uint32_t timeout = 0);

  // override ClientCallback
  void recv_post(const char *name, uint32_t msgtype,
                 std::shared_ptr<Caps> &msg);

  void recv_call(const char *name, std::shared_ptr<Caps> &msg,
                 std::shared_ptr<Reply> &reply);

  void disconnected();

private:
  void run();

  void init_cli(std::shared_ptr<Client> &cli);

  void destroy_client();

private:
  class Options {
  public:
    std::string uri;
    uint32_t bufsize = 0;
    std::chrono::milliseconds reconn_interval =
        std::chrono::milliseconds(10000);
    uint32_t flags = 0;
    MonitorCallback *mon_callback = nullptr;
    uint32_t beep_interval = FLORA_CLI_DEFAULT_BEEP_INTERVAL;
    uint32_t noresp_timeout = FLORA_CLI_DEFAULT_NORESP_TIMEOUT;
  };

  Options options;
  PostHandlerMap post_handlers;
  CallHandlerMap call_handlers;
  std::mutex conn_mutex;
  std::condition_variable conn_cond;
  // 'start' and try connect flora service once
  std::condition_variable start_cond;
  std::shared_ptr<Client> flora_cli;
  std::thread run_thread;
  bool working = false;
};

} // namespace flora

extern "C" {
#endif // __cplusplus

typedef intptr_t flora_agent_t;
typedef void (*flora_agent_subscribe_callback_t)(const char *name, caps_t msg,
                                                 uint32_t type, void *arg);
typedef void (*flora_agent_declare_method_callback_t)(const char *name,
                                                      caps_t msg,
                                                      flora_call_reply_t reply,
                                                      void *arg);

flora_agent_t flora_agent_create();

void flora_agent_config(flora_agent_t agent, uint32_t key, ...);

void flora_agent_subscribe(flora_agent_t agent, const char *name,
                           flora_agent_subscribe_callback_t cb, void *arg);

void flora_agent_unsubscribe(flora_agent_t agent, const char *name);

void flora_agent_declare_method(flora_agent_t agent, const char *name,
                                flora_agent_declare_method_callback_t cb,
                                void *arg);

void flora_agent_remove_method(flora_agent_t agent, const char *name);

void flora_agent_start(flora_agent_t agent, int32_t block);

void flora_agent_close(flora_agent_t agent);

int32_t flora_agent_post(flora_agent_t agent, const char *name, caps_t msg,
                         uint32_t msgtype);

int32_t flora_agent_call(flora_agent_t agent, const char *name, caps_t msg,
                         const char *target, flora_call_result *result,
                         uint32_t timeout);

int32_t flora_agent_call_nb(flora_agent_t agent, const char *name, caps_t msg,
                            const char *target, flora_call_callback_t cb,
                            void *arg, uint32_t timeout);

void flora_agent_delete(flora_agent_t agent);

#ifdef __cplusplus
} // extern "C"
#endif // __cplusplus
