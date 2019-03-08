#include "ser-helper.h"
#include "defs.h"

using namespace std;

namespace flora {
namespace internal {

int32_t RequestSerializer::serialize_auth(uint32_t version, const char *extra,
                                          int32_t pid, uint32_t flags,
                                          void *data, uint32_t size,
                                          uint32_t ser_flags) {
  shared_ptr<Caps> caps = Caps::new_instance();
  caps->write(CMD_AUTH_REQ);
  caps->write(version);
  caps->write(extra);
  caps->write(pid);
  caps->write(flags);
  int32_t r = caps->serialize(data, size, ser_flags);
  if (r < 0)
    return -1;
  if (r > size)
    return -1;
  return r;
}

int32_t RequestSerializer::serialize_subscribe(const char *name, void *data,
                                               uint32_t size, uint32_t flags) {
  shared_ptr<Caps> caps = Caps::new_instance();
  caps->write(CMD_SUBSCRIBE_REQ);
  caps->write(name);
  int32_t r = caps->serialize(data, size, flags);
  if (r < 0)
    return -1;
  if (r > size)
    return -1;
  return r;
}

int32_t RequestSerializer::serialize_unsubscribe(const char *name, void *data,
                                                 uint32_t size,
                                                 uint32_t flags) {
  shared_ptr<Caps> caps = Caps::new_instance();
  caps->write(CMD_UNSUBSCRIBE_REQ);
  caps->write(name);
  int32_t r = caps->serialize(data, size, flags);
  if (r < 0)
    return -1;
  if (r > size)
    return -1;
  return r;
}

int32_t RequestSerializer::serialize_declare_method(const char *name,
                                                    void *data, uint32_t size,
                                                    uint32_t flags) {
  shared_ptr<Caps> caps = Caps::new_instance();
  caps->write(CMD_DECLARE_METHOD_REQ);
  caps->write(name);
  int32_t r = caps->serialize(data, size, flags);
  if (r < 0)
    return -1;
  if (r > size)
    return -1;
  return r;
}

int32_t RequestSerializer::serialize_remove_method(const char *name, void *data,
                                                   uint32_t size,
                                                   uint32_t flags) {
  shared_ptr<Caps> caps = Caps::new_instance();
  caps->write(CMD_REMOVE_METHOD_REQ);
  caps->write(name);
  int32_t r = caps->serialize(data, size, flags);
  if (r < 0)
    return -1;
  if (r > size)
    return -1;
  return r;
}

int32_t RequestSerializer::serialize_post(const char *name, uint32_t msgtype,
                                          shared_ptr<Caps> &args, void *data,
                                          uint32_t size, uint32_t flags) {
  shared_ptr<Caps> caps = Caps::new_instance();
  caps->write(CMD_POST_REQ);
  caps->write(msgtype);
  caps->write(name);
  caps->write(args);
  int32_t r = caps->serialize(data, size, flags);
  if (r < 0)
    return -1;
  if (r > size)
    return -1;
  return r;
}

int32_t RequestSerializer::serialize_call(const char *name,
                                          shared_ptr<Caps> &args,
                                          const char *target, int32_t id,
                                          uint32_t timeout, void *data,
                                          uint32_t size, uint32_t flags) {
  shared_ptr<Caps> caps = Caps::new_instance();
  caps->write(CMD_CALL_REQ);
  caps->write(name);
  caps->write(target);
  caps->write(id);
  caps->write(timeout);
  caps->write(args);
  int32_t r = caps->serialize(data, size, flags);
  if (r < 0)
    return -1;
  if (r > size)
    return -1;
  return r;
}

int32_t RequestSerializer::serialize_reply(int32_t id, int32_t code,
                                           shared_ptr<Caps> &values, void *data,
                                           uint32_t size, uint32_t flags) {
  shared_ptr<Caps> caps = Caps::new_instance();
  caps->write(CMD_REPLY_REQ);
  caps->write(id);
  caps->write(code);
  caps->write(values);
  int32_t r = caps->serialize(data, size, flags);
  if (r < 0)
    return -1;
  if (r > size)
    return -1;
  return r;
}

int32_t RequestSerializer::serialize_ping(void *data, uint32_t size,
                                          uint32_t flags) {
  shared_ptr<Caps> caps = Caps::new_instance();
  caps->write(CMD_PING_REQ);
  int32_t r = caps->serialize(data, size, flags);
  if (r < 0 || r > size)
    return -1;
  return r;
}

int32_t ResponseSerializer::serialize_auth(int32_t result, void *data,
                                           uint32_t size, uint32_t flags) {
  shared_ptr<Caps> caps = Caps::new_instance();
  caps->write(CMD_AUTH_RESP);
  caps->write(result);
  int32_t r = caps->serialize(data, size, flags);
  if (r < 0)
    return -1;
  if (r > size)
    return -1;
  return r;
}

int32_t ResponseSerializer::serialize_post(const char *name, uint32_t msgtype,
                                           shared_ptr<Caps> &args, void *data,
                                           uint32_t size, uint32_t flags) {
  shared_ptr<Caps> caps = Caps::new_instance();
  caps->write(CMD_POST_RESP);
  caps->write(msgtype);
  caps->write(name);
  caps->write(args);
  int32_t r = caps->serialize(data, size, flags);
  if (r < 0)
    return -1;
  if (r > size)
    return -1;
  return r;
}

int32_t ResponseSerializer::serialize_call(const char *name,
                                           shared_ptr<Caps> &args, int32_t id,
                                           void *data, uint32_t size,
                                           uint32_t flags) {
  shared_ptr<Caps> caps = Caps::new_instance();
  caps->write(CMD_CALL_RESP);
  caps->write(id);
  caps->write(name);
  caps->write(args);
  int32_t r = caps->serialize(data, size, flags);
  if (r < 0)
    return -1;
  if (r > size)
    return -1;
  return r;
}

int32_t ResponseSerializer::serialize_reply(int32_t id, int32_t rescode,
                                            Response *reply, void *data,
                                            uint32_t size, uint32_t flags) {
  shared_ptr<Caps> caps = Caps::new_instance();
  caps->write(CMD_REPLY_RESP);
  caps->write(id);
  caps->write(rescode);
  if (rescode == FLORA_CLI_SUCCESS) {
    caps->write(reply->ret_code);
    caps->write(reply->data);
    caps->write(reply->extra.c_str());
  }
  int32_t r = caps->serialize(data, size, flags);
  if (r < 0)
    return -1;
  if (r > size)
    return -1;
  return r;
}

static shared_ptr<Caps> serialize_monitor_list_item(AdapterInfo &info) {
  shared_ptr<Caps> r = Caps::new_instance();
  r->write(info.id);
  r->write(info.pid);
  r->write(info.name);
  r->write(info.flags);
  return r;
}

int32_t ResponseSerializer::serialize_monitor_list_all(AdapterInfoMap &infos,
                                                       void *data,
                                                       uint32_t size,
                                                       uint32_t flags) {
  shared_ptr<Caps> p = Caps::new_instance();
  shared_ptr<Caps> s;

  p->write(CMD_MONITOR_RESP);
  p->write(MONITOR_LIST_ALL);
  p->write((int32_t)infos.size());
  auto it = infos.begin();
  while (it != infos.end()) {
    s = serialize_monitor_list_item(it->second);
    p->write(s);
    ++it;
  }
  int32_t r = p->serialize(data, size, flags);
  if (r < 0)
    return -1;
  if (r > size)
    return -1;
  return r;
}

int32_t ResponseSerializer::serialize_monitor_list_add(AdapterInfo &info,
                                                       void *data,
                                                       uint32_t size,
                                                       uint32_t flags) {
  shared_ptr<Caps> p = Caps::new_instance();
  shared_ptr<Caps> s;

  p->write(CMD_MONITOR_RESP);
  p->write(MONITOR_LIST_ADD);
  s = serialize_monitor_list_item(info);
  p->write(s);
  int32_t r = p->serialize(data, size, flags);
  if (r < 0)
    return -1;
  if (r > size)
    return -1;
  return r;
}

int32_t ResponseSerializer::serialize_monitor_list_remove(uint32_t id,
                                                          void *data,
                                                          uint32_t size,
                                                          uint32_t flags) {
  shared_ptr<Caps> p = Caps::new_instance();
  p->write(CMD_MONITOR_RESP);
  p->write(MONITOR_LIST_REMOVE);
  p->write(id);
  int32_t r = p->serialize(data, size, flags);
  if (r < 0)
    return -1;
  if (r > size)
    return -1;
  return r;
}

int32_t ResponseSerializer::serialize_monitor_sub_all(AdapterInfoMap &infos,
                                                      void *data, uint32_t size,
                                                      uint32_t flags) {
  return -1;
}

int32_t ResponseSerializer::serialize_monitor_sub_add(uint32_t id,
                                                      const string &name,
                                                      void *data, uint32_t size,
                                                      uint32_t flags) {
  return -1;
}

int32_t ResponseSerializer::serialize_monitor_sub_remove(uint32_t id,
                                                         const string &name,
                                                         void *data,
                                                         uint32_t size,
                                                         uint32_t flags) {
  return -1;
}

int32_t ResponseSerializer::serialize_monitor_decl_all(AdapterInfoMap &infos,
                                                       void *data,
                                                       uint32_t size,
                                                       uint32_t flags) {
  return -1;
}

int32_t ResponseSerializer::serialize_monitor_decl_add(uint32_t id,
                                                       const string &name,
                                                       void *data,
                                                       uint32_t size,
                                                       uint32_t flags) {
  return -1;
}

int32_t ResponseSerializer::serialize_monitor_decl_remove(uint32_t id,
                                                          const string &name,
                                                          void *data,
                                                          uint32_t size,
                                                          uint32_t flags) {
  return -1;
}

int32_t ResponseSerializer::serialize_monitor_post(uint32_t from,
                                                   const string &name,
                                                   void *data, uint32_t size,
                                                   uint32_t flags) {
  return -1;
}

int32_t ResponseSerializer::serialize_monitor_call(
    uint32_t from, const string &name, const string &target, int32_t err,
    void *data, uint32_t size, uint32_t flags) {
  return -1;
}

int32_t ResponseSerializer::serialize_pong(void *data, uint32_t size,
                                           uint32_t flags) {
  shared_ptr<Caps> p = Caps::new_instance();
  p->write(CMD_PONG_RESP);
  int32_t r = p->serialize(data, size, flags);
  if (r < 0 || r > size)
    return -1;
  return r;
}

int32_t RequestParser::parse_auth(shared_ptr<Caps> &caps, uint32_t &version,
                                  string &extra, int32_t &pid,
                                  uint32_t &flags) {
  if (caps->read(version) != CAPS_SUCCESS)
    return -1;
  if (caps->read(extra) != CAPS_SUCCESS)
    return -1;
  if (caps->read(pid) != CAPS_SUCCESS)
    pid = 0;
  if (caps->read(flags) != CAPS_SUCCESS)
    flags = 0;
  return 0;
}

int32_t RequestParser::parse_subscribe(shared_ptr<Caps> &caps, string &name) {
  if (caps->read(name) != CAPS_SUCCESS)
    return -1;
  return 0;
}

int32_t RequestParser::parse_unsubscribe(shared_ptr<Caps> &caps, string &name) {
  if (caps->read(name) != CAPS_SUCCESS)
    return -1;
  return 0;
}

int32_t RequestParser::parse_declare_method(shared_ptr<Caps> &caps,
                                            string &name) {
  if (caps->read(name) != CAPS_SUCCESS)
    return -1;
  return 0;
}

int32_t RequestParser::parse_remove_method(shared_ptr<Caps> &caps,
                                           string &name) {
  if (caps->read(name) != CAPS_SUCCESS)
    return -1;
  return 0;
}

int32_t RequestParser::parse_post(shared_ptr<Caps> &caps, string &name,
                                  uint32_t &msgtype, shared_ptr<Caps> &args) {
  if (caps->read(msgtype) != CAPS_SUCCESS)
    return -1;
  if (caps->read(name) != CAPS_SUCCESS)
    return -1;
  if (caps->read(args) != CAPS_SUCCESS)
    return -1;
  return 0;
}

int32_t RequestParser::parse_call(shared_ptr<Caps> &caps, string &name,
                                  shared_ptr<Caps> &args, string &target,
                                  int32_t &id, uint32_t &timeout) {
  if (caps->read(name) != CAPS_SUCCESS)
    return -1;
  if (caps->read(target) != CAPS_SUCCESS)
    return -1;
  if (caps->read(id) != CAPS_SUCCESS)
    return -1;
  if (caps->read(timeout) != CAPS_SUCCESS)
    return -1;
  if (caps->read(args) != CAPS_SUCCESS)
    return -1;
  return 0;
}

int32_t RequestParser::parse_reply(shared_ptr<Caps> &caps, int32_t &id,
                                   int32_t &code, shared_ptr<Caps> &values) {
  if (caps->read(id) != CAPS_SUCCESS)
    return -1;
  if (caps->read(code) != CAPS_SUCCESS)
    return -1;
  if (caps->read(values) != CAPS_SUCCESS)
    return -1;
  return 0;
}

int32_t ResponseParser::parse_auth(shared_ptr<Caps> &caps, int32_t &result) {
  if (caps->read(result) != CAPS_SUCCESS)
    return -1;
  return 0;
}

int32_t ResponseParser::parse_post(shared_ptr<Caps> &caps, string &name,
                                   uint32_t &msgtype, shared_ptr<Caps> &args) {
  if (caps->read(msgtype) != CAPS_SUCCESS)
    return -1;
  if (caps->read(name) != CAPS_SUCCESS)
    return -1;
  if (caps->read(args) != CAPS_SUCCESS)
    return -1;
  return 0;
}

int32_t ResponseParser::parse_call(shared_ptr<Caps> &caps, string &name,
                                   shared_ptr<Caps> &args, int32_t &id) {
  if (caps->read(id) != CAPS_SUCCESS)
    return -1;
  if (caps->read(name) != CAPS_SUCCESS)
    return -1;
  if (caps->read(args) != CAPS_SUCCESS)
    return -1;
  return 0;
}

int32_t ResponseParser::parse_reply(shared_ptr<Caps> &caps, int32_t &id,
                                    int32_t &rescode, Response &reply) {
  if (caps->read(id) != CAPS_SUCCESS)
    return -1;
  if (caps->read(rescode) != CAPS_SUCCESS)
    return -1;
  if (rescode == FLORA_CLI_SUCCESS) {
    if (caps->read(reply.ret_code) != CAPS_SUCCESS)
      return -1;
    if (caps->read(reply.data) != CAPS_SUCCESS)
      return -1;
    if (caps->read(reply.extra) != CAPS_SUCCESS)
      return -1;
  }
  return 0;
}

int32_t ResponseParser::parse_monitor_list_all(shared_ptr<Caps> &caps,
                                               vector<MonitorListItem> &infos) {
  uint32_t size;
  if (caps->read(size) != CAPS_SUCCESS)
    return -1;
  if (size == 0)
    return 0;

  shared_ptr<Caps> sub;
  infos.reserve(size);
  while (true) {
    if (caps->read(sub) != CAPS_SUCCESS)
      break;
    infos.emplace_back();
    MonitorListItem &i = infos.back();
    if (sub->read(i.id) != CAPS_SUCCESS)
      return -1;
    if (sub->read(i.pid) != CAPS_SUCCESS)
      return -1;
    if (sub->read(i.name) != CAPS_SUCCESS)
      return -1;
    if (sub->read(i.flags) != CAPS_SUCCESS)
      return -1;
  }
  return infos.size() == size ? 0 : -1;
}

int32_t ResponseParser::parse_monitor_list_add(shared_ptr<Caps> &caps,
                                               MonitorListItem &info) {
  shared_ptr<Caps> sub;
  if (caps->read(sub) != CAPS_SUCCESS)
    return -1;
  if (sub->read(info.id) != CAPS_SUCCESS)
    return -1;
  if (sub->read(info.pid) != CAPS_SUCCESS)
    return -1;
  if (sub->read(info.name) != CAPS_SUCCESS)
    return -1;
  if (sub->read(info.flags) != CAPS_SUCCESS)
    return -1;
  return 0;
}

int32_t ResponseParser::parse_monitor_list_remove(shared_ptr<Caps> &caps,
                                                  uint32_t &id) {
  if (caps->read(id) != CAPS_SUCCESS)
    return -1;
  return 0;
}

int32_t
ResponseParser::parse_monitor_sub_all(shared_ptr<Caps> &caps,
                                      vector<MonitorSubscriptionItem> &infos) {
  return -1;
}

int32_t ResponseParser::parse_monitor_sub_add(shared_ptr<Caps> &caps,
                                              MonitorSubscriptionItem &info) {
  return -1;
}

int32_t
ResponseParser::parse_monitor_sub_remove(shared_ptr<Caps> &caps,
                                         MonitorSubscriptionItem &info) {
  return -1;
}

int32_t
ResponseParser::parse_monitor_decl_all(shared_ptr<Caps> &caps,
                                       vector<MonitorDeclarationItem> &infos) {
  return -1;
}

int32_t ResponseParser::parse_monitor_decl_add(shared_ptr<Caps> &caps,
                                               MonitorDeclarationItem &info) {
  return -1;
}

int32_t
ResponseParser::parse_monitor_decl_remove(shared_ptr<Caps> &caps,
                                          MonitorDeclarationItem &info) {
  return -1;
}

int32_t ResponseParser::parse_monitor_post(shared_ptr<Caps> &caps,
                                           MonitorPostInfo &info) {
  return -1;
}

int32_t ResponseParser::parse_monitor_call(shared_ptr<Caps> &caps,
                                           MonitorCallInfo &info) {
  return -1;
}

bool is_valid_msgtype(uint32_t msgtype) {
  return msgtype < FLORA_NUMBER_OF_MSGTYPE;
}

} // namespace internal
} // namespace flora
