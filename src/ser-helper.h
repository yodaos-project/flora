#pragma once

#include "caps.h"
#include "defs.h"
#include "disp.h"
#include "flora-cli.h"
#include <arpa/inet.h>
#include <stdint.h>
#include <vector>

namespace flora {
namespace internal {

class RequestSerializer {
public:
  static int32_t serialize_auth(uint32_t version, const char *extra,
                                int32_t pid, uint32_t flags, void *data,
                                uint32_t size, uint32_t ser_flags);

  static int32_t serialize_subscribe(const char *name, void *data,
                                     uint32_t size, uint32_t flags);

  static int32_t serialize_unsubscribe(const char *name, void *data,
                                       uint32_t size, uint32_t flags);

  static int32_t serialize_declare_method(const char *name, void *data,
                                          uint32_t size, uint32_t flags);

  static int32_t serialize_remove_method(const char *name, void *data,
                                         uint32_t size, uint32_t flags);

  static int32_t serialize_post(const char *name, uint32_t msgtype,
                                std::shared_ptr<Caps> &args, void *data,
                                uint32_t size, uint32_t flags);

  static int32_t serialize_call(const char *name, std::shared_ptr<Caps> &args,
                                const char *target, int32_t id,
                                uint32_t timeout, void *data, uint32_t size,
                                uint32_t flags);

  static int32_t serialize_reply(int32_t id, int32_t code,
                                 std::shared_ptr<Caps> &values, void *data,
                                 uint32_t size, uint32_t flags);

  static int32_t serialize_ping(void *data, uint32_t size, uint32_t flags);
};

class ResponseSerializer {
public:
  static int32_t serialize_auth(int32_t result, uint32_t version, void *data,
                                uint32_t size, uint32_t flags);

  static int32_t serialize_post(const char *name, uint32_t msgtype,
                                std::shared_ptr<Caps> &args, uint64_t tag,
                                const char *cliname, void *data, uint32_t size,
                                uint32_t flags);

  static int32_t serialize_call(const char *name, std::shared_ptr<Caps> &args,
                                int32_t id, uint64_t tag, const char *cliname,
                                void *data, uint32_t size, uint32_t flags);

  static int32_t serialize_reply(int32_t id, int32_t rescode, Response *reply,
                                 uint64_t tag, void *data, uint32_t size,
                                 uint32_t flags);

  static int32_t serialize_monitor_list_all(AdapterInfoMap &infos, void *data,
                                            uint32_t size, uint32_t flags);

  static int32_t serialize_monitor_list_add(AdapterInfo &info, void *data,
                                            uint32_t size, uint32_t flags);

  static int32_t serialize_monitor_list_remove(uint32_t id, void *data,
                                               uint32_t size, uint32_t flags);

  static int32_t serialize_monitor_sub_all(AdapterInfoMap &infos, void *data,
                                           uint32_t size, uint32_t flags);

  static int32_t serialize_monitor_sub_add(uint32_t id, const std::string &name,
                                           void *data, uint32_t size,
                                           uint32_t flags);

  static int32_t serialize_monitor_sub_remove(uint32_t id,
                                              const std::string &name,
                                              void *data, uint32_t size,
                                              uint32_t flags);

  static int32_t serialize_monitor_decl_all(AdapterInfoMap &infos, void *data,
                                            uint32_t size, uint32_t flags);

  static int32_t serialize_monitor_decl_add(uint32_t id,
                                            const std::string &name, void *data,
                                            uint32_t size, uint32_t flags);

  static int32_t serialize_monitor_decl_remove(uint32_t id,
                                               const std::string &name,
                                               void *data, uint32_t size,
                                               uint32_t flags);

  static int32_t serialize_monitor_post(uint32_t from, const std::string &name,
                                        void *data, uint32_t size,
                                        uint32_t flags);

  static int32_t serialize_monitor_call(uint32_t from, const std::string &name,
                                        const std::string &target, int32_t err,
                                        void *data, uint32_t size,
                                        uint32_t flags);

  static int32_t serialize_pong(void *data, uint32_t size, uint32_t flags);
};

class RequestParser {
public:
  static int32_t parse_auth(std::shared_ptr<Caps> &caps, uint32_t &version,
                            std::string &extra, int32_t &pid, uint32_t &flags);

  static int32_t parse_subscribe(std::shared_ptr<Caps> &caps,
                                 std::string &name);

  static int32_t parse_unsubscribe(std::shared_ptr<Caps> &caps,
                                   std::string &name);

  static int32_t parse_declare_method(std::shared_ptr<Caps> &caps,
                                      std::string &name);

  static int32_t parse_remove_method(std::shared_ptr<Caps> &caps,
                                     std::string &name);

  static int32_t parse_post(std::shared_ptr<Caps> &caps, std::string &name,
                            uint32_t &msgtype, std::shared_ptr<Caps> &args);

  static int32_t parse_call(std::shared_ptr<Caps> &caps, std::string &name,
                            std::shared_ptr<Caps> &args, std::string &target,
                            int32_t &id, uint32_t &timeout);

  static int32_t parse_reply(std::shared_ptr<Caps> &caps, int32_t &id,
                             int32_t &code, std::shared_ptr<Caps> &values);
};

class ResponseParser {
public:
  static int32_t parse_auth(std::shared_ptr<Caps> &caps, int32_t &result,
                            uint32_t &version);

  static int32_t parse_post(std::shared_ptr<Caps> &caps, std::string &name,
                            uint32_t &msgtype, std::shared_ptr<Caps> &args,
                            uint64_t &tag, std::string &cliname);

  static int32_t parse_call(std::shared_ptr<Caps> &caps, std::string &name,
                            std::shared_ptr<Caps> &args, int32_t &id,
                            uint64_t &tag, std::string &cliname);

  static int32_t parse_reply(std::shared_ptr<Caps> &caps, int32_t &id,
                             int32_t &rescode, Response &reply, uint64_t &tag);

  static int32_t parse_monitor_list_all(std::shared_ptr<Caps> &caps,
                                        std::vector<MonitorListItem> &infos);

  static int32_t parse_monitor_list_add(std::shared_ptr<Caps> &caps,
                                        MonitorListItem &info);

  static int32_t parse_monitor_list_remove(std::shared_ptr<Caps> &caps,
                                           uint32_t &id);

  static int32_t
  parse_monitor_sub_all(std::shared_ptr<Caps> &caps,
                        std::vector<MonitorSubscriptionItem> &infos);

  static int32_t parse_monitor_sub_add(std::shared_ptr<Caps> &caps,
                                       MonitorSubscriptionItem &info);

  static int32_t parse_monitor_sub_remove(std::shared_ptr<Caps> &caps,
                                          MonitorSubscriptionItem &info);

  static int32_t
  parse_monitor_decl_all(std::shared_ptr<Caps> &caps,
                         std::vector<MonitorDeclarationItem> &infos);

  static int32_t parse_monitor_decl_add(std::shared_ptr<Caps> &caps,
                                        MonitorDeclarationItem &info);

  static int32_t parse_monitor_decl_remove(std::shared_ptr<Caps> &caps,
                                           MonitorDeclarationItem &info);

  static int32_t parse_monitor_post(std::shared_ptr<Caps> &caps,
                                    MonitorPostInfo &info);

  static int32_t parse_monitor_call(std::shared_ptr<Caps> &caps,
                                    MonitorCallInfo &info);
};

bool is_valid_msgtype(uint32_t msgtype);

class TagHelper {
public:
  static uint64_t create(struct sockaddr_in& addr) {
    return (((uint64_t)(0x80000000 | addr.sin_port)) << 32) | addr.sin_addr.s_addr;
  }

  static uint64_t create(uint32_t pid) { return pid; }

  // return: 0  unix
  //         1  tcp
  static uint32_t type(uint64_t tag) {
    return ((tag >> 32) & 0x80000000) ? 1 : 0;
  }

  static pid_t pid(uint64_t tag) {
    return tag & 0xffffffff;
  }

  static const char* ipaddr(uint64_t tag) {
    struct in_addr iaddr;
    iaddr.s_addr = (tag & 0xffffffff);
    return inet_ntoa(iaddr);
  }

  static uint16_t port(uint64_t tag) {
    return (tag >> 32) & 0xffff;
  }

  static void to_string(uint64_t tag, std::string& str) {
    char buf[16];
    if (type(tag) == 0) {
      snprintf(buf, sizeof(buf), "%d", pid(tag));
      str = buf;
    } else {
      str = ipaddr(tag);
      str += ':';
      snprintf(buf, sizeof(buf), "%d", port(tag));
      str += buf;
    }
  }
};

} // namespace internal
} // namespace flora
