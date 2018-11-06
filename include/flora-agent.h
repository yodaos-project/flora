#pragma once

#include <chrono>
#include <functional>
#include <map>
#include <mutex>
#include <condition_variable>
#include "caps.h"
#include "flora-cli.h"

/**
 * flora client代理工具
 *
 * 包装flora client api，更易于使用
 * 自动重连
 * 更易用的消息回调
 * 仅支持c++
 */

namespace flora {

enum class AgentConfigKey {
  URI = 0,
  BUFSIZE,
  RECONN_INTERVAL
};

typedef std::map<std::string,
          std::function<void(std::shared_ptr<Caps>&, uint32_t, Reply*)>
        > MsgHandlerMap;

class Agent : public ClientCallback {
public:
  void config(AgentConfigKey key, ...);

  void subscribe(const char* name,
      std::function<void(std::shared_ptr<Caps>&, uint32_t, Reply*)>&& cb);

  void unsubscribe(const char* name);

  void start(bool block = false);

  void close();

  int32_t post(const char* name, std::shared_ptr<Caps>& msg, uint32_t msgtype);

  int32_t get(const char* name, std::shared_ptr<Caps>& msg,
      ResponseArray& responses, uint32_t timeout = 0);

  int32_t get(const char* name, std::shared_ptr<Caps>& msg,
      std::function<void(ResponseArray&)>&& cb, uint32_t timeout = 0);

  int32_t get(const char* name, std::shared_ptr<Caps>& msg,
      std::function<void(ResponseArray&)>& cb, uint32_t timeout = 0);

  // override ClientCallback
  void recv_post(const char* name, uint32_t msgtype,
      std::shared_ptr<Caps>& msg);

  void recv_get(const char* name, std::shared_ptr<Caps>& msg, Reply& reply);

  void disconnected();

private:
  void run();

  void subscribe_msgs(std::shared_ptr<Client>& cli);

  void destroy_client();

private:
  std::string uri;
  uint32_t bufsize = 0;
  std::chrono::milliseconds reconn_interval = std::chrono::milliseconds(10000);
  MsgHandlerMap msg_handlers;
  std::mutex conn_mutex;
  std::condition_variable conn_cond;
  std::shared_ptr<Client> flora_cli;
  bool working = false;
};

} // namespace flora
