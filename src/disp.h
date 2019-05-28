#pragma once

#include "adap.h"
#include "caps.h"
#include "defs.h"
#include "flora-svc.h"
#include <chrono>
#include <condition_variable>
#include <list>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <thread>

namespace flora {
namespace internal {

typedef std::list<std::shared_ptr<Adapter>> AdapterList;
typedef std::map<std::string, AdapterList> SubscriptionMap;
typedef struct {
  std::shared_ptr<Caps> data;
} PersistMsg;
typedef std::map<std::string, PersistMsg> PersistMsgMap;
typedef std::map<std::string, std::shared_ptr<Adapter>> NamedAdapterMap;
typedef std::pair<std::shared_ptr<Caps>, std::shared_ptr<Adapter>> CmdPacket;
typedef std::list<CmdPacket> CmdPacketList;
typedef struct {
  int32_t svrid;
  int32_t cliid;
  std::shared_ptr<Adapter> sender;
  std::shared_ptr<Adapter> target;
  std::chrono::steady_clock::time_point discard_tp;
} PendingCall;
typedef std::list<PendingCall> PendingCallList;
typedef std::map<intptr_t, AdapterInfo> AdapterInfoMap;

class Dispatcher : public flora::Dispatcher {
public:
  Dispatcher(uint32_t flags, uint32_t bufsize);

  ~Dispatcher() noexcept;

  bool put(const void *data, uint32_t size, std::shared_ptr<Adapter> &sender);

  void run(bool blocking);

  void close();

  inline uint32_t max_msg_size() const { return buf_size; }

  void erase_adapter(std::shared_ptr<Adapter> &adapter);

private:
  bool handle_auth_req(std::shared_ptr<Caps> &msg_caps,
                       std::shared_ptr<Adapter> &sender);

  bool handle_subscribe_req(std::shared_ptr<Caps> &msg_caps,
                            std::shared_ptr<Adapter> &sender);

  bool handle_unsubscribe_req(std::shared_ptr<Caps> &msg_caps,
                              std::shared_ptr<Adapter> &sender);

  bool handle_declare_method(std::shared_ptr<Caps> &msg_caps,
                             std::shared_ptr<Adapter> &sender);

  bool handle_remove_method(std::shared_ptr<Caps> &msg_caps,
                            std::shared_ptr<Adapter> &sender);

  bool handle_post_req(std::shared_ptr<Caps> &msg_caps,
                       std::shared_ptr<Adapter> &sender);

  bool handle_call_req(std::shared_ptr<Caps> &msg_caps,
                       std::shared_ptr<Adapter> &sender);

  bool handle_reply_req(std::shared_ptr<Caps> &msg_caps,
                        std::shared_ptr<Adapter> &sender);

  bool handle_ping_req(std::shared_ptr<Caps> &msg_caps,
                       std::shared_ptr<Adapter> &sender);

  bool add_adapter(const std::string &name, int32_t pid, uint32_t flags,
                   std::shared_ptr<Adapter> &adapter);

  void add_monitor(const std::string &name, int32_t pid, uint32_t flags,
                   std::shared_ptr<Adapter> &adapter);

  void handle_cmds();

  void handle_cmd(std::shared_ptr<Caps> &caps,
                  std::shared_ptr<Adapter> &sender);

  void add_pending_call(int32_t svrid, int32_t cliid,
                        std::shared_ptr<Adapter> &sender,
                        std::shared_ptr<Adapter> &target, uint32_t timeout);

  void pending_call_timeout(PendingCall &pc);

  void discard_pending_calls();

  bool post_msg(const std::string &name, uint32_t type,
                std::shared_ptr<Caps> &args, Adapter *sender);

  void write_post_msg_to_adapters(const std::string &name, uint32_t type,
                                  std::shared_ptr<Caps> &args, uint32_t flags,
                                  AdapterList &adapters,
                                  const char *sender_name);

  void do_erase_adapter(std::shared_ptr<Adapter> &sender);

  void write_monitor_data(uint32_t flags, std::shared_ptr<Adapter> &adapter);

  void write_monitor_list(std::shared_ptr<Adapter> &adapter);

  void write_monitor_list_add(std::shared_ptr<Adapter> &newitem,
                              Adapter *monitor);

  void write_monitor_list_remove(uint32_t id, Adapter *monitor);

  void write_monitor_list_add(std::shared_ptr<Adapter> &newitem);

  void write_monitor_list_remove(uint32_t id);

private:
  SubscriptionMap subscriptions;
  PersistMsgMap persist_msgs;
  NamedAdapterMap named_adapters;
  int8_t *buffer;
  uint32_t buf_size;
  CmdPacketList cmd_packets;
  std::mutex cmd_mutex;
  std::condition_variable cmd_cond;
  std::thread run_thread;
  PendingCallList pending_calls;
  int32_t reqseq = 0;
  AdapterInfoMap adapter_infos;
  AdapterInfoMap monitors;
  uint32_t flags;
  bool working = false;

  static bool (Dispatcher::*msg_handlers[MSG_HANDLER_COUNT])(
      std::shared_ptr<Caps> &, std::shared_ptr<Adapter> &);
};

} // namespace internal
} // namespace flora
