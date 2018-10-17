#pragma once

#include <stdint.h>
#include <vector>
#include "caps.h"
#include "flora-cli.h"

namespace flora {
namespace internal {

class RequestSerializer {
public:
  static int32_t serialize_auth(uint32_t version, const char* extra,
      void* data, uint32_t size);

  static int32_t serialize_subscribe(const char* name, void* data,
      uint32_t size);

  static int32_t serialize_unsubscribe(const char* name, void* data,
      uint32_t size);

  // when 'msgtype' == FLORA_MSGTYPE_REQUEST, 'id', 'timeout'参数有效
  static int32_t serialize_post(const char* name, uint32_t msgtype,
      std::shared_ptr<Caps>& args, int32_t id, uint32_t timeout,
      void* data, uint32_t size);

  static int32_t serialize_reply(const char* name, std::shared_ptr<Caps>& args,
      int32_t id, int32_t retcode, void* data, uint32_t size);
};

class ResponseSerializer {
public:
  static int32_t serialize_auth(int32_t result, void* data, uint32_t size);

  static int32_t serialize_post(const char* name, uint32_t msgtype,
      std::shared_ptr<Caps>& args, int32_t id, void* data, uint32_t size);

  static int32_t serialize_reply(const char* name, int32_t id,
      std::vector<Reply>& datas, void* data, uint32_t size);
};

class RequestParser {
public:
  static int32_t parse_auth(std::shared_ptr<Caps>& caps,
      uint32_t& version, std::string& extra);

  static int32_t parse_subscribe(std::shared_ptr<Caps>& caps,
      std::string& name);

  static int32_t parse_unsubscribe(std::shared_ptr<Caps>& caps,
      std::string& name);

  static int32_t parse_post(std::shared_ptr<Caps>& caps, std::string& name,
      uint32_t& msgtype, std::shared_ptr<Caps>& args, int32_t& id,
      uint32_t& timeout);

  static int32_t parse_reply(std::shared_ptr<Caps>& caps,
      std::string& name, std::shared_ptr<Caps>& args, int32_t& id,
      int32_t& retcode);
};

class ResponseParser {
public:
  static int32_t parse_auth(const void* data, uint32_t size,
      int32_t& result);

  static int32_t parse_post(std::shared_ptr<Caps>& caps, std::string& name,
      uint32_t& msgtype, std::shared_ptr<Caps>& args, int32_t& id);

  static int32_t parse_reply(std::shared_ptr<Caps>& caps,
      std::string& name, std::vector<Reply>& replys);
};

bool is_valid_msgtype(uint32_t msgtype);

} // namespace internal
} // namespace flora
