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

#define FLORA_AGENT_CONFIG_URI 0
#define FLORA_AGENT_CONFIG_BUFSIZE 1
#define FLORA_AGENT_CONFIG_RECONN_INTERVAL 2

#ifdef __cplusplus

#include <chrono>
#include <condition_variable>
#include <functional>
#include <map>
#include <mutex>
#include <thread>

namespace flora {

typedef std::map<
    std::string,
    std::function<void(const char *, std::shared_ptr<Caps> &, uint32_t)>>
    PostHandlerMap;

typedef std::map<
    std::string,
    std::function<void(const char *, std::shared_ptr<Caps> &, Reply &)>>
    CallHandlerMap;

class Agent : public ClientCallback {
public:
  ~Agent();

  void config(uint32_t key, ...);

  void config(uint32_t key, va_list ap);

  void subscribe(const char *name,
                 std::function<void(const char *, std::shared_ptr<Caps> &,
                                    uint32_t)> &&cb);

  void subscribe(
      const char *name,
      std::function<void(const char *, std::shared_ptr<Caps> &, uint32_t)> &cb);

  void unsubscribe(const char *name);

  void declare_method(
      const char *name,
      std::function<void(const char *name, std::shared_ptr<Caps> &, Reply &)>
          &&cb);

  void declare_method(
      const char *name,
      std::function<void(const char *name, std::shared_ptr<Caps> &, Reply &)>
          &cb);

  void remove_method(const char *name);

  void start(bool block = false);

  void close();

  int32_t post(const char *name, std::shared_ptr<Caps> &msg,
               uint32_t msgtype = FLORA_MSGTYPE_INSTANT);

  int32_t call(const char *name, std::shared_ptr<Caps> &msg, const char *target,
               Response &response, uint32_t timeout = 0);

  int32_t call(const char *name, std::shared_ptr<Caps> &msg, const char *target,
               std::function<void(int32_t, Response &)> &&cb, uint32_t timeout);

  int32_t call(const char *name, std::shared_ptr<Caps> &msg, const char *target,
               std::function<void(int32_t, Response &)> &cb, uint32_t timeout);

  // override ClientCallback
  void recv_post(const char *name, uint32_t msgtype,
                 std::shared_ptr<Caps> &msg);

  void recv_call(const char *name, std::shared_ptr<Caps> &msg, Reply &reply);

  void disconnected();

private:
  void run();

  void subscribe_msgs(std::shared_ptr<Client> &cli);

  void destroy_client();

private:
  std::string uri;
  uint32_t bufsize = 0;
  std::chrono::milliseconds reconn_interval = std::chrono::milliseconds(10000);
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
typedef struct {
  int32_t ret_code;
  caps_t data;
} flora_reply_t;
typedef void (*flora_agent_subscribe_callback_t)(const char *name, caps_t msg,
                                                 uint32_t type, void *arg);
typedef void (*flora_agent_declare_method_callback_t)(const char *name,
                                                      caps_t msg,
                                                      flora_reply_t *reply,
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
