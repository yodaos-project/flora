#pragma once

#include "caps.h"
#include "defs.h"
#include "disp.h"
#include "flora-cli.h"
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
  static int32_t serialize_auth(int32_t result, void *data, uint32_t size,
                                uint32_t flags);

  static int32_t serialize_post(const char *name, uint32_t msgtype,
                                std::shared_ptr<Caps> &args, void *data,
                                uint32_t size, uint32_t flags);

  static int32_t serialize_call(const char *name, std::shared_ptr<Caps> &args,
                                int32_t id, void *data, uint32_t size,
                                uint32_t flags);

  static int32_t serialize_reply(int32_t id, int32_t rescode, Response *reply,
                                 void *data, uint32_t size, uint32_t flags);

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
  static int32_t parse_auth(std::shared_ptr<Caps> &caps, int32_t &result);

  static int32_t parse_post(std::shared_ptr<Caps> &caps, std::string &name,
                            uint32_t &msgtype, std::shared_ptr<Caps> &args);

  static int32_t parse_call(std::shared_ptr<Caps> &caps, std::string &name,
                            std::shared_ptr<Caps> &args, int32_t &id);

  static int32_t parse_reply(std::shared_ptr<Caps> &caps, int32_t &id,
                             int32_t &rescode, Response &reply);

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

} // namespace internal
} // namespace flora
