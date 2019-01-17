#include "disp.h"
#include "caps.h"
#include "flora-cli.h"
#include "flora-svc.h"
#include "rlog.h"
#include "ser-helper.h"
#include <sys/mman.h>

using namespace std;
using namespace std::chrono;

#define TAG "flora.Dispatcher"
#define DEFAULT_CALL_TIMEOUT 200

namespace flora {
namespace internal {

bool (Dispatcher::*(Dispatcher::msg_handlers[MSG_HANDLER_COUNT]))(
    shared_ptr<Caps> &, std::shared_ptr<Adapter> &) = {
    &Dispatcher::handle_auth_req,        &Dispatcher::handle_subscribe_req,
    &Dispatcher::handle_unsubscribe_req, &Dispatcher::handle_post_req,
    &Dispatcher::handle_reply_req,       &Dispatcher::handle_declare_method,
    &Dispatcher::handle_remove_method,   &Dispatcher::handle_call_req};

Dispatcher::Dispatcher(uint32_t bufsize) {
  monitor_persist_msg_name = "--dfdc9571857b9306326d9bb3ef5fe71299796858--";

  buf_size = bufsize > DEFAULT_MSG_BUF_SIZE ? bufsize : DEFAULT_MSG_BUF_SIZE;
  buffer = (int8_t *)mmap(NULL, buf_size, PROT_READ | PROT_WRITE,
                          MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
}

Dispatcher::~Dispatcher() noexcept {
  subscriptions.clear();
  munmap(buffer, buf_size);
  close();
}

bool Dispatcher::put(const void *data, uint32_t size,
                     std::shared_ptr<Adapter> &sender) {
  shared_ptr<Caps> msg_caps;

  if (Caps::parse(data, size, msg_caps) != CAPS_SUCCESS) {
    KLOGE(TAG, "msg caps parse failed");
    return false;
  }

  cmd_mutex.lock();
  cmd_packets.push_back(make_pair(msg_caps, sender));
  cmd_cond.notify_one();
  cmd_mutex.unlock();
  return true;
}

void Dispatcher::run(bool blocking) {
  working = true;
  if (blocking) {
    handle_cmds();
  } else {
    run_thread = thread([this]() { this->handle_cmds(); });
  }
}

void Dispatcher::handle_cmds() {
  unique_lock<mutex> locker(cmd_mutex, defer_lock);
  CmdPacketList pending_cmds;
  CmdPacketList::iterator it;

  while (true) {
    locker.lock();

  loop_begin_locked:
    if (!working) {
      KLOGI(TAG, "Dispatcher closed, thread exit");
      break;
    }
    pending_cmds.splice(pending_cmds.begin(), cmd_packets);
    if (pending_cmds.empty()) {
      discard_pending_calls();
      if (pending_calls.empty()) {
        cmd_cond.wait(locker);
        goto loop_begin_locked;
      }
      cmd_cond.wait_until(locker, pending_calls.front().discard_tp);
      goto loop_begin_locked;
    }
    locker.unlock();

    // handle commands
    // may use thread poll at multi-core platform in future
    for (it = pending_cmds.begin(); it != pending_cmds.end(); ++it) {
      handle_cmd(it->first, it->second);
    }
    pending_cmds.clear();
  }
}

void Dispatcher::handle_cmd(shared_ptr<Caps> &msg_caps,
                            shared_ptr<Adapter> &sender) {
  // empty caps msg, erase adapter
  if (msg_caps == nullptr) {
    if (sender->auth_extra.length() > 0) {
      named_adapters.erase(sender->auth_extra);
    }
    erase_adapter_debug_info(sender);
    return;
  }

  int32_t cmd;
  if (msg_caps->read(cmd) != CAPS_SUCCESS) {
    KLOGE(TAG, "read msg cmd failed");
    sender->close();
    return;
  }
  if (cmd < 0 || cmd >= MSG_HANDLER_COUNT) {
    KLOGE(TAG, "msg cmd invalid: %d", cmd);
    sender->close();
    return;
  }
  if (!(this->*(msg_handlers[cmd]))(msg_caps, sender))
    sender->close();
}

void Dispatcher::pending_call_timeout(PendingCall &pc) {
  int32_t c = ResponseSerializer::serialize_reply(pc.cliid, FLORA_CLI_ETIMEOUT,
                                                  nullptr, buffer, buf_size,
                                                  pc.sender->serialize_flags);
  pc.sender->write(buffer, c);
}

void Dispatcher::discard_pending_calls() {
  auto tp = steady_clock::now();
  while (!pending_calls.empty()) {
    auto it = pending_calls.begin();
    if (tp < (*it).discard_tp)
      break;
    pending_call_timeout(*it);
    pending_calls.erase(it);
  }
}

void Dispatcher::close() {
  cmd_mutex.lock();
  working = false;
  cmd_cond.notify_one();
  cmd_mutex.unlock();

  if (run_thread.joinable()) {
    run_thread.join();
  }
}

void Dispatcher::erase_adapter(shared_ptr<Adapter> &&adapter) {
  lock_guard<mutex> locker(cmd_mutex);
  shared_ptr<Caps> empty;
  // add empty caps to queue for erase adapter
  cmd_packets.push_back(make_pair(empty, adapter));
  cmd_cond.notify_one();
}

bool Dispatcher::handle_auth_req(shared_ptr<Caps> &msg_caps,
                                 shared_ptr<Adapter> &sender) {
  uint32_t version;
  string extra;
  int32_t pid;

  if (RequestParser::parse_auth(msg_caps, version, extra, pid) != 0)
    return false;
  KLOGI(TAG, "<<< %s: auth ver %u", extra.c_str(), version);
  int32_t result = FLORA_CLI_SUCCESS;
  if (version != FLORA_VERSION) {
    result = FLORA_CLI_EAUTH;
    KLOGE(TAG, "<<< %s: auth failed. version not supported, excepted %u",
          extra.c_str(), FLORA_VERSION);
  } else if (!add_adapter(extra, sender)) {
    result = FLORA_CLI_EDUPID;
    KLOGE(TAG, "<<< %s: auth failed. client id already used", extra.c_str());
  }
  int32_t c = ResponseSerializer::serialize_auth(result, buffer, buf_size,
                                                 sender->serialize_flags);
  if (c < 0)
    return false;
  sender->write(buffer, c);
  add_adapter_debug_info(pid, sender);
  return result == FLORA_CLI_SUCCESS;
}

void Dispatcher::add_adapter_debug_info(int32_t pid,
                                        shared_ptr<Adapter> &adapter) {
  AdapterDebugInfo info;
  info.pid = pid;
  info.adapter = adapter;
  adapter_debug_infos.insert(
      make_pair(reinterpret_cast<intptr_t>(adapter.get()), info));

  update_adapter_debug_infos();
}

void Dispatcher::erase_adapter_debug_info(shared_ptr<Adapter> &sender) {
  adapter_debug_infos.erase(reinterpret_cast<intptr_t>(sender.get()));

  update_adapter_debug_infos();
}

void Dispatcher::update_adapter_debug_infos() {
  shared_ptr<Caps> data = Caps::new_instance();
  shared_ptr<Caps> sub;
  AdapterDebugInfoMap::iterator it;

  for (it = adapter_debug_infos.begin(); it != adapter_debug_infos.end();
       ++it) {
    sub = Caps::new_instance();
    sub->write(it->second.pid);
    sub->write(it->second.adapter->auth_extra);
    data->write(sub);
  }
  post_msg(monitor_persist_msg_name, FLORA_MSGTYPE_PERSIST, data, nullptr);
}

bool Dispatcher::handle_subscribe_req(shared_ptr<Caps> &msg_caps,
                                      shared_ptr<Adapter> &sender) {
  string name;

  if (RequestParser::parse_subscribe(msg_caps, name) != 0)
    return false;
  KLOGI(TAG, "<<< %s: subscribe %s", sender->auth_extra.c_str(), name.c_str());
  if (name.length() == 0)
    return false;
  AdapterList &adapters = subscriptions[name];
  AdapterList::iterator it;
  for (it = adapters.begin(); it != adapters.end(); ++it) {
    if ((*it).get() == sender.get())
      return true;
  }
  adapters.push_back(sender);

  // post persist messge to client
  PersistMsgMap::iterator pit = persist_msgs.find(name);
  if (pit != persist_msgs.end()) {
    int32_t c = ResponseSerializer::serialize_post(
        name.c_str(), FLORA_MSGTYPE_PERSIST, pit->second.data, buffer, buf_size,
        sender->serialize_flags);
    if (c > 0) {
      KLOGI(TAG, ">>> %s: dispatch persist msg %s", sender->auth_extra.c_str(),
            name.c_str());
      sender->write(buffer, c);
    }
  }
  return true;
}

bool Dispatcher::handle_unsubscribe_req(shared_ptr<Caps> &msg_caps,
                                        shared_ptr<Adapter> &sender) {
  string name;

  if (RequestParser::parse_unsubscribe(msg_caps, name) != 0)
    return false;
  KLOGI(TAG, "<<< %s: unsubscribe %s", sender->auth_extra.c_str(),
        name.c_str());
  if (name.length() == 0)
    return false;
  AdapterList &adapters = subscriptions[name];
  AdapterList::iterator it;
  for (it = adapters.begin(); it != adapters.end(); ++it) {
    if ((*it).get() == sender.get()) {
      adapters.erase(it);
      break;
    }
  }
  return true;
}

bool Dispatcher::handle_declare_method(shared_ptr<Caps> &msg_caps,
                                       shared_ptr<Adapter> &sender) {
  string name;
  if (RequestParser::parse_declare_method(msg_caps, name) != 0)
    return false;
  KLOGI(TAG, "<<< %s: declare method %s", sender->auth_extra.c_str(),
        name.c_str());
  if (name.length() == 0)
    return false;
  return sender->declare_method(name);
}

bool Dispatcher::handle_remove_method(shared_ptr<Caps> &msg_caps,
                                      shared_ptr<Adapter> &sender) {
  string name;
  if (RequestParser::parse_remove_method(msg_caps, name) != 0)
    return false;
  KLOGI(TAG, "<<< %s: remove method %s", sender->auth_extra.c_str(),
        name.c_str());
  if (name.length() == 0)
    return false;
  sender->remove_method(name);
  return true;
}

bool Dispatcher::handle_post_req(shared_ptr<Caps> &msg_caps,
                                 shared_ptr<Adapter> &sender) {
  uint32_t msgtype;
  string name;
  shared_ptr<Caps> args;

  if (RequestParser::parse_post(msg_caps, name, msgtype, args) != 0)
    return false;
  return post_msg(name, msgtype, args, sender.get());
}

bool Dispatcher::post_msg(const string &name, uint32_t type,
                          shared_ptr<Caps> &args, Adapter *sender) {
  if (!is_valid_msgtype(type))
    return false;
  const char *cli_name = sender ? sender->auth_extra.c_str() : "";
  KLOGI(TAG, "<<< %s: post %u..%s", cli_name, type, name.c_str());
  if (name.length() == 0)
    return false;

  SubscriptionMap::iterator sit;
  sit = subscriptions.find(name);
  if (sit != subscriptions.end() && !sit->second.empty()) {
    AdapterList nobo_adapters; // no net byteorder
    AdapterList bo_adapters;   // net byteorder
    AdapterList::iterator ait;
    ait = sit->second.begin();
    while (ait != sit->second.end()) {
      if ((*ait)->closed()) {
        AdapterList::iterator dit = ait;
        ++ait;
        sit->second.erase(dit);
        continue;
      }
      if ((*ait)->serialize_flags == CAPS_FLAG_NET_BYTEORDER) {
        bo_adapters.push_back(*ait);
      } else {
        nobo_adapters.push_back(*ait);
      }
      ++ait;
    }
    write_post_msg_to_adapters(name, type, args, 0, nobo_adapters, cli_name);
    write_post_msg_to_adapters(name, type, args, CAPS_FLAG_NET_BYTEORDER,
                               bo_adapters, cli_name);
  }

  if (type == FLORA_MSGTYPE_PERSIST) {
    PersistMsg &pmsg = persist_msgs[name];
    pmsg.data = args;
  }
  return true;
}

void Dispatcher::write_post_msg_to_adapters(const string &name, uint32_t type,
                                            shared_ptr<Caps> &args,
                                            uint32_t flags,
                                            AdapterList &adapters,
                                            const char *sender_name) {
  int32_t c = ResponseSerializer::serialize_post(name.c_str(), type, args,
                                                 buffer, buf_size, flags);
  if (c < 0)
    return;
  AdapterList::iterator ait;
  ait = adapters.begin();
  while (ait != adapters.end()) {
    KLOGI(TAG, "%s >>> %s: post %u..%s", sender_name,
          (*ait)->auth_extra.c_str(), type, name.c_str());
    (*ait)->write(buffer, c);
    ++ait;
  }
}

void Dispatcher::add_pending_call(int32_t svrid, int32_t cliid,
                                  shared_ptr<Adapter> &sender,
                                  shared_ptr<Adapter> &target,
                                  uint32_t timeout) {
  if (timeout == 0)
    timeout = DEFAULT_CALL_TIMEOUT;
  steady_clock::time_point tp = steady_clock::now() + milliseconds(timeout);
  PendingCallList::iterator it;
  // pending_calls is sorted by discard_tp
  for (it = pending_calls.begin(); it != pending_calls.end(); ++it) {
    if ((*it).discard_tp >= tp) {
      break;
    }
  }
  it = pending_calls.emplace(it);
  (*it).svrid = svrid;
  (*it).cliid = cliid;
  (*it).sender = sender;
  (*it).target = target;
  (*it).discard_tp = tp;
}

bool Dispatcher::handle_call_req(shared_ptr<Caps> &msg_caps,
                                 shared_ptr<Adapter> &sender) {
  string name;
  string target;
  shared_ptr<Caps> args;
  int32_t cliid;
  uint32_t timeout;
  if (RequestParser::parse_call(msg_caps, name, args, target, cliid, timeout) !=
      0)
    return false;
  KLOGI(TAG, "%s <<< %s: call %d/%s, timeout %u", target.c_str(),
        sender->auth_extra.c_str(), cliid, name.c_str(), timeout);
  NamedAdapterMap::iterator it = named_adapters.find(target);
  int32_t c;
  if (it == named_adapters.end() || !it->second->has_method(name)) {
    c = ResponseSerializer::serialize_reply(cliid, FLORA_CLI_ENEXISTS, nullptr,
                                            buffer, buf_size,
                                            sender->serialize_flags);
    if (c < 0)
      return false;
    KLOGI(TAG, ">>> %s: call %d/%s failed. target %s not existed",
          sender->auth_extra.c_str(), cliid, name.c_str(), target.c_str());
    sender->write(buffer, c);
    return true;
  }
  int32_t svrid = ++reqseq;
  add_pending_call(svrid, cliid, sender, it->second, timeout);
  c = ResponseSerializer::serialize_call(name.c_str(), args, svrid, buffer,
                                         buf_size, it->second->serialize_flags);
  if (c < 0)
    return false;
  KLOGI(TAG, "%s >>> %s: call %d/%s", sender->auth_extra.c_str(),
        target.c_str(), svrid, name.c_str());
  it->second->write(buffer, c);
  return true;
}

bool Dispatcher::handle_reply_req(shared_ptr<Caps> &msg_caps,
                                  shared_ptr<Adapter> &sender) {
  shared_ptr<Caps> args;
  int32_t svrid;
  int32_t ret_code;
  shared_ptr<Caps> data;

  if (RequestParser::parse_reply(msg_caps, svrid, ret_code, data) != 0)
    return false;
  KLOGI(TAG, "<<< %s: reply %d", sender->auth_extra.c_str(), svrid);
  PendingCallList::iterator it;
  for (it = pending_calls.begin(); it != pending_calls.end(); ++it) {
    if ((*it).svrid == svrid && (*it).target == sender) {
      break;
    }
  }
  if (it == pending_calls.end()) {
    KLOGW(TAG, "<<< %s: reply %d failed. not found pending call",
          sender->auth_extra.c_str(), svrid);
    return true;
  }
  Response resp;
  resp.ret_code = ret_code;
  resp.data = data;
  resp.extra = sender->auth_extra;
  int32_t c = ResponseSerializer::serialize_reply(
      (*it).cliid, FLORA_CLI_SUCCESS, &resp, buffer, buf_size,
      (*it).sender->serialize_flags);
  if (c < 0)
    return false;
  KLOGI(TAG, "%s >>> %s: reply %d", sender->auth_extra.c_str(),
        (*it).sender->auth_extra.c_str(), (*it).cliid);
  (*it).sender->write(buffer, c);
  pending_calls.erase(it);
  return true;
}

bool Dispatcher::add_adapter(const string &name, shared_ptr<Adapter> &adapter) {
  if (name.length() == 0)
    return true;
  auto r = named_adapters.insert(make_pair(name, adapter));
  if (r.second)
    adapter->auth_extra = name;
  return r.second;
}

} // namespace internal
} // namespace flora

shared_ptr<flora::Dispatcher>
flora::Dispatcher::new_instance(uint32_t msg_buf_size) {
  return static_pointer_cast<flora::Dispatcher>(
      make_shared<flora::internal::Dispatcher>(msg_buf_size));
}

flora_dispatcher_t flora_dispatcher_new(uint32_t bufsize) {
  return reinterpret_cast<flora_dispatcher_t>(
      new flora::internal::Dispatcher(bufsize));
}

void flora_dispatcher_delete(flora_dispatcher_t handle) {
  if (handle) {
    delete reinterpret_cast<flora::Dispatcher *>(handle);
  }
}

void flora_dispatcher_run(flora_dispatcher_t handle, int32_t block) {
  if (handle) {
    reinterpret_cast<flora::Dispatcher *>(handle)->run(block != 0);
  }
}

void flora_dispatcher_close(flora_dispatcher_t handle) {
  if (handle) {
    reinterpret_cast<flora::Dispatcher *>(handle)->close();
  }
}
