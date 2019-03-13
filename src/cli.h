#pragma once

#include "caps.h"
#include "conn.h"
#include "defs.h"
#include "flora-cli.h"
#include <chrono>
#include <condition_variable>
#include <list>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace flora {
namespace internal {

typedef std::function<void(int32_t, Response &)> RespCallback;
typedef struct {
  int32_t id;
  int32_t rcode;
  Response *result;
  RespCallback callback;
} PendingRequest;
typedef std::list<PendingRequest> PendingRequestList;

class Client : public flora::Client {
public:
  explicit Client(flora::ClientOptions *opts);

  ~Client() noexcept;

  int32_t connect(const char *uri, ClientCallback *ccb, MonitorCallback *mcb);

  int32_t close(bool passive);

  void set_weak_ptr(std::shared_ptr<Client> &ptr) { this_weak_ptr = ptr; }

  void send_reply(int32_t callid, int32_t code, std::shared_ptr<Caps> &data);

  // implementation of flora::Client
  int32_t subscribe(const char *name);

  int32_t unsubscribe(const char *name);

  int32_t declare_method(const char *name);

  int32_t remove_method(const char *name);

  int32_t post(const char *name, std::shared_ptr<Caps> &msg, uint32_t msgtype);

  int32_t call(const char *name, std::shared_ptr<Caps> &msg, const char *target,
               Response &reply, uint32_t timeout);

  int32_t call(const char *name, std::shared_ptr<Caps> &msg, const char *target,
               std::function<void(int32_t, Response &)> &&cb, uint32_t timeout);

  int32_t call(const char *name, std::shared_ptr<Caps> &msg, const char *target,
               std::function<void(int32_t, Response &)> &cb, uint32_t timeout);

private:
  int32_t auth(const std::string &extra, uint32_t flags);

  void recv_loop();

  void keepalive_loop();

  bool handle_received(int32_t size);

  bool handle_cmd_before_auth(int32_t cmd, std::shared_ptr<Caps> &resp);

  bool handle_cmd_after_auth(int32_t cmd, std::shared_ptr<Caps> &resp);

  void iclose(bool passive, int32_t err);

  bool handle_monitor_list_all(std::shared_ptr<Caps> &resp);
  bool handle_monitor_list_add(std::shared_ptr<Caps> &resp);
  bool handle_monitor_list_remove(std::shared_ptr<Caps> &resp);
  bool handle_monitor_sub_all(std::shared_ptr<Caps> &resp);
  bool handle_monitor_sub_add(std::shared_ptr<Caps> &resp);
  bool handle_monitor_sub_remove(std::shared_ptr<Caps> &resp);
  bool handle_monitor_decl_all(std::shared_ptr<Caps> &resp);
  bool handle_monitor_decl_add(std::shared_ptr<Caps> &resp);
  bool handle_monitor_decl_remove(std::shared_ptr<Caps> &resp);
  bool handle_monitor_post(std::shared_ptr<Caps> &resp);
  bool handle_monitor_call(std::shared_ptr<Caps> &resp);

  void ping();

private:
  int8_t *sbuffer = nullptr;
  int8_t *rbuffer = nullptr;
  uint32_t rbuf_off = 0;
  ClientOptions options;
  std::shared_ptr<Connection> connection;
  std::thread recv_thread;
  std::mutex req_mutex;
  std::condition_variable req_reply_cond;
  PendingRequestList pending_requests;
  std::thread keepalive_thread;
  std::mutex ka_mutex;
  std::condition_variable ka_cond;
  int32_t reqseq = 0;
  uint32_t serialize_flags = 0;
  int32_t close_reason = 0;
  std::weak_ptr<Client> this_weak_ptr;
  std::thread::id callback_thr_id;
  MonitorCallback *mon_callback = nullptr;
  typedef bool (flora::internal::Client::*CmdHandler)(
      int32_t cmd, std::shared_ptr<Caps> &resp);
  CmdHandler cmd_handler = &Client::handle_cmd_before_auth;
  class AuthResult {
  public:
    std::mutex amutex;
    std::condition_variable acond;
    int32_t result = FLORA_CLI_EAUTH;
  };
  AuthResult *auth_result = nullptr;

  typedef bool (flora::internal::Client::*MonitorHandler)(
      std::shared_ptr<Caps> &);
  static MonitorHandler monitor_handlers[MONITOR_SUBTYPE_NUM];

public:
  std::string auth_extra;
  ClientCallback *cli_callback = nullptr;
#ifdef FLORA_DEBUG
  uint32_t post_times = 0;
  uint32_t post_bytes = 0;
  uint32_t req_times = 0;
  uint32_t req_bytes = 0;
  uint32_t send_times = 0;
  uint32_t send_bytes = 0;
#endif
};

class ReplyImpl : public flora::Reply {
public:
  ReplyImpl(std::shared_ptr<Client> &&c, int32_t id);

  ~ReplyImpl() noexcept;

  void write_code(int32_t code);

  void write_data(std::shared_ptr<Caps> &data);

  void end();

  void end(int32_t code);

  void end(int32_t code, std::shared_ptr<Caps> &data);

private:
  std::shared_ptr<Client> client;
  int32_t ret_code = FLORA_CLI_SUCCESS;
  std::shared_ptr<Caps> data;
  int32_t callid = 0;
};

} // namespace internal
} // namespace flora

typedef struct {
  std::shared_ptr<flora::Reply> cxxreply;
} CReply;
