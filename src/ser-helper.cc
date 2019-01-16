#include "ser-helper.h"
#include "defs.h"

using namespace std;

namespace flora {
namespace internal {

int32_t RequestSerializer::serialize_auth(uint32_t version, const char *extra, int32_t pid,
                                          void *data, uint32_t size,
                                          uint32_t flags) {
  shared_ptr<Caps> caps = Caps::new_instance();
  caps->write(CMD_AUTH_REQ);
  caps->write(version);
  caps->write(extra);
  caps->write(pid);
  int32_t r = caps->serialize(data, size, flags);
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

int32_t RequestParser::parse_auth(shared_ptr<Caps> &caps, uint32_t &version,
                                  string &extra, int32_t &pid) {
  if (caps->read(version) != CAPS_SUCCESS)
    return -1;
  if (caps->read(extra) != CAPS_SUCCESS)
    return -1;
  if (caps->read(pid) != CAPS_SUCCESS) {
    pid = 0;
  }
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

int32_t ResponseParser::parse_auth(const void *data, uint32_t size,
                                   int32_t &result) {
  shared_ptr<Caps> caps;
  int32_t cmd;

  if (Caps::parse(data, size, caps) != CAPS_SUCCESS)
    return -1;
  if (caps->read(cmd) != CAPS_SUCCESS)
    return -1;
  if (cmd != CMD_AUTH_RESP)
    return -1;
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

bool is_valid_msgtype(uint32_t msgtype) {
  return msgtype < FLORA_NUMBER_OF_MSGTYPE;
}

} // namespace internal
} // namespace flora
