#pragma once

#include "caps.h"
#include "defs.h"
#include "flora-svc.h"
#include "reply-mgr.h"
#include <map>
#include <memory>
#include <mutex>
#include <string>

namespace flora {
namespace internal {

typedef std::map<std::string, AdapterList> SubscriptionMap;
typedef struct {
  std::shared_ptr<Caps> data;
} PersistMsg;
typedef std::map<std::string, PersistMsg> PersistMsgMap;

class Dispatcher : public flora::Dispatcher {
public:
  explicit Dispatcher(uint32_t bufsize);

  ~Dispatcher() noexcept;

  bool put(Frame &frame, std::shared_ptr<Adapter> &sender);

  inline uint32_t max_msg_size() const { return buf_size; }

private:
  bool handle_auth_req(std::shared_ptr<Caps> &msg_caps,
                       std::shared_ptr<Adapter> &sender);

  bool handle_subscribe_req(std::shared_ptr<Caps> &msg_caps,
                            std::shared_ptr<Adapter> &sender);

  bool handle_unsubscribe_req(std::shared_ptr<Caps> &msg_caps,
                              std::shared_ptr<Adapter> &sender);

  bool handle_post_req(std::shared_ptr<Caps> &msg_caps,
                       std::shared_ptr<Adapter> &sender);

  bool handle_reply_req(std::shared_ptr<Caps> &msg_caps,
                        std::shared_ptr<Adapter> &sender);

private:
  SubscriptionMap subscriptions;
  PersistMsgMap persist_msgs;
  int8_t *buffer;
  uint32_t buf_size;
  // Adapter类没有线程安全特性
  // 现有两个线程可能调用Adapter::write
  std::mutex write_adapter_mutex;
  ReplyManager reply_mgr;
  int32_t reqseq = 0;

  static bool (Dispatcher::*msg_handlers[MSG_HANDLER_COUNT])(
      std::shared_ptr<Caps> &, std::shared_ptr<Adapter> &);
};

} // namespace internal
} // namespace flora
