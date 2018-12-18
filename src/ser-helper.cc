#include "ser-helper.h"
#include "defs.h"

using namespace std;

namespace flora {
namespace internal {

int32_t RequestSerializer::serialize_auth(uint32_t version, const char *extra,
                                          void *data, uint32_t size,
                                          uint32_t flags) {
  shared_ptr<Caps> caps = Caps::new_instance();
  caps->write(CMD_AUTH_REQ);
  caps->write((int32_t)version);
  caps->write(extra);
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

int32_t RequestSerializer::serialize_post(const char *name, uint32_t msgtype,
                                          shared_ptr<Caps> &args, int32_t id,
                                          uint32_t timeout, void *data,
                                          uint32_t size, uint32_t flags) {
  shared_ptr<Caps> caps = Caps::new_instance();
  caps->write(CMD_POST_REQ);
  caps->write((int32_t)msgtype);
  caps->write(id);
  caps->write((int32_t)timeout);
  caps->write(name);
  caps->write(args);
  int32_t r = caps->serialize(data, size, flags);
  if (r < 0)
    return -1;
  if (r > size)
    return -1;
  return r;
}

int32_t RequestSerializer::serialize_reply(const char *name,
                                           shared_ptr<Caps> &args, int32_t id,
                                           int32_t retcode, void *data,
                                           uint32_t size, uint32_t flags) {
  shared_ptr<Caps> caps = Caps::new_instance();
  caps->write(CMD_REPLY_REQ);
  caps->write(id);
  caps->write(retcode);
  caps->write(name);
  caps->write(args);
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
                                           shared_ptr<Caps> &args, int32_t id,
                                           void *data, uint32_t size,
                                           uint32_t flags) {
  shared_ptr<Caps> caps = Caps::new_instance();
  caps->write(CMD_POST_RESP);
  caps->write((int32_t)msgtype);
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

int32_t ResponseSerializer::serialize_reply(const char *name, int32_t id,
                                            ResponseArray &datas, void *data,
                                            uint32_t size, uint32_t flags) {
  shared_ptr<Caps> caps = Caps::new_instance();
  caps->write(CMD_REPLY_RESP);
  caps->write(id);
  caps->write(name);
  caps->write((int32_t)datas.size());
  size_t i;
  for (i = 0; i < datas.size(); ++i) {
    caps->write(datas[i].ret_code);
    caps->write(datas[i].data);
    caps->write(datas[i].extra.c_str());
  }
  int32_t r = caps->serialize(data, size, flags);
  if (r < 0)
    return -1;
  if (r > size)
    return -1;
  return r;
}

int32_t RequestParser::parse_auth(shared_ptr<Caps> &caps, uint32_t &version,
                                  string &extra) {
  int32_t v;
  if (caps->read(v) != CAPS_SUCCESS)
    return -1;
  version = v;
  if (caps->read_string(extra) != CAPS_SUCCESS)
    return -1;
  return 0;
}

int32_t RequestParser::parse_subscribe(shared_ptr<Caps> &caps, string &name) {
  int32_t v;
  if (caps->read_string(name) != CAPS_SUCCESS)
    return -1;
  return 0;
}

int32_t RequestParser::parse_unsubscribe(shared_ptr<Caps> &caps, string &name) {
  if (caps->read_string(name) != CAPS_SUCCESS)
    return -1;
  return 0;
}

int32_t RequestParser::parse_post(shared_ptr<Caps> &caps, string &name,
                                  uint32_t &msgtype, shared_ptr<Caps> &args,
                                  int32_t &id, uint32_t &timeout) {
  int32_t v;
  if (caps->read(v) != CAPS_SUCCESS)
    return -1;
  msgtype = v;
  if (caps->read(id) != CAPS_SUCCESS)
    return -1;
  if (caps->read(v) != CAPS_SUCCESS)
    return -1;
  timeout = v;
  if (caps->read_string(name) != CAPS_SUCCESS)
    return -1;
  if (caps->read(args) != CAPS_SUCCESS)
    return -1;
  return 0;
}

int32_t RequestParser::parse_reply(shared_ptr<Caps> &caps, string &name,
                                   shared_ptr<Caps> &args, int32_t &id,
                                   int32_t &retcode) {
  if (caps->read(id) != CAPS_SUCCESS)
    return -1;
  if (caps->read(retcode) != CAPS_SUCCESS)
    return -1;
  if (caps->read_string(name) != CAPS_SUCCESS)
    return -1;
  if (caps->read(args) != CAPS_SUCCESS)
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
                                   uint32_t &msgtype, shared_ptr<Caps> &args,
                                   int32_t &id) {
  int32_t v;
  if (caps->read(v) != CAPS_SUCCESS)
    return -1;
  msgtype = v;
  if (caps->read(id) != CAPS_SUCCESS)
    return -1;
  if (caps->read_string(name) != CAPS_SUCCESS)
    return -1;
  if (caps->read(args) != CAPS_SUCCESS)
    return -1;
  return 0;
}

int32_t ResponseParser::parse_reply(shared_ptr<Caps> &caps, string &name,
                                    ResponseArray &replys) {
  int32_t count;
  const char *s;

  if (caps->read_string(name) != CAPS_SUCCESS)
    return -1;
  if (caps->read(count) != CAPS_SUCCESS)
    return -1;
  replys.reserve(count);
  while (count) {
    replys.emplace_back();
    if (caps->read(replys.back().ret_code) != CAPS_SUCCESS)
      return -1;
    if (caps->read(replys.back().data) != CAPS_SUCCESS)
      return -1;
    if (caps->read_string(replys.back().extra) != CAPS_SUCCESS)
      return -1;
    --count;
  }
  return 0;
}

bool is_valid_msgtype(uint32_t msgtype) {
  return msgtype <= FLORA_MSGTYPE_REQUEST;
}

} // namespace internal
} // namespace flora
