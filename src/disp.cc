#include "disp.h"
#include "caps.h"
#include "flora-cli.h"
#include "flora-svc.h"
#include "rlog.h"
#include "ser-helper.h"
#include <sys/mman.h>

using namespace std;

#define TAG "flora.Dispatcher"

namespace flora {
namespace internal {

bool (Dispatcher::*(Dispatcher::msg_handlers[MSG_HANDLER_COUNT]))(
    shared_ptr<Caps> &, std::shared_ptr<Adapter> &) = {
    &Dispatcher::handle_auth_req, &Dispatcher::handle_subscribe_req,
    &Dispatcher::handle_unsubscribe_req, &Dispatcher::handle_post_req,
    &Dispatcher::handle_reply_req};

Dispatcher::Dispatcher(uint32_t bufsize) : reply_mgr(bufsize) {
  buf_size = bufsize > DEFAULT_MSG_BUF_SIZE ? bufsize : DEFAULT_MSG_BUF_SIZE;
  buffer = (int8_t *)mmap(NULL, buf_size, PROT_READ | PROT_WRITE,
                          MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
}

Dispatcher::~Dispatcher() noexcept {
  subscriptions.clear();
  munmap(buffer, buf_size);
}

bool Dispatcher::put(Frame &frame, shared_ptr<Adapter> &sender) {
  shared_ptr<Caps> msg_caps;

  if (Caps::parse(frame.data, frame.size, msg_caps, false) != CAPS_SUCCESS) {
    KLOGE(TAG, "msg caps parse failed");
    return false;
  }

  int32_t cmd;
  if (msg_caps->read(cmd) != CAPS_SUCCESS) {
    KLOGE(TAG, "read msg cmd failed");
    return false;
  }
  if (cmd < 0 || cmd >= MSG_HANDLER_COUNT) {
    KLOGE(TAG, "msg cmd invalid: %d", cmd);
    return false;
  }
  return (this->*(msg_handlers[cmd]))(msg_caps, sender);
}

bool Dispatcher::handle_auth_req(shared_ptr<Caps> &msg_caps,
                                 shared_ptr<Adapter> &sender) {
  uint32_t version;
  string extra;

  if (RequestParser::parse_auth(msg_caps, version, extra) != 0)
    return false;
  sender->auth_extra = extra;
  int32_t result = FLORA_CLI_SUCCESS;
  if (version < FLORA_VERSION) {
    result = FLORA_CLI_EAUTH;
    KLOGE(TAG, "version %u not supported, protocol version is %d", version,
          FLORA_VERSION);
  }
  int32_t c = ResponseSerializer::serialize_auth(result, buffer, buf_size,
                                                 sender->serialize_flags);
  if (c < 0)
    return false;
  sender->write(buffer, c);
  return true;
}

bool Dispatcher::handle_subscribe_req(shared_ptr<Caps> &msg_caps,
                                      shared_ptr<Adapter> &sender) {
  string name;

  if (RequestParser::parse_subscribe(msg_caps, name) != 0)
    return false;
  if (name.length() == 0)
    return false;
  AdapterList &adapters = subscriptions[name];
  AdapterList::iterator it;
  for (it = adapters.begin(); it != adapters.end(); ++it) {
    if ((*it).get() == sender.get())
      return true;
  }
  adapters.push_back(sender);
  KLOGI(TAG, "client %s subscribe msg %s", sender->auth_extra.c_str(),
        name.c_str());

  // post persist messge to client
  PersistMsgMap::iterator pit = persist_msgs.find(name);
  if (pit != persist_msgs.end()) {
    int32_t c = ResponseSerializer::serialize_post(
        name.c_str(), FLORA_MSGTYPE_PERSIST, pit->second.data, 0, buffer,
        buf_size, sender->serialize_flags);
    if (c > 0) {
      KLOGI(TAG, "client %s subscribe msg %s, post persist msg to client",
            sender->auth_extra.c_str(), name.c_str());
      sender->write(buffer, c);
    }
  }
  return true;
}

bool Dispatcher::handle_unsubscribe_req(shared_ptr<Caps> &msg_caps,
                                        shared_ptr<Adapter> &sender) {
  uint32_t msgtype;
  string name;

  if (RequestParser::parse_unsubscribe(msg_caps, name) != 0)
    return false;
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

bool Dispatcher::handle_post_req(shared_ptr<Caps> &msg_caps,
                                 shared_ptr<Adapter> &sender) {
  uint32_t msgtype;
  uint32_t timeout;
  string name;
  shared_ptr<Caps> args;
  int32_t cliid;

  if (RequestParser::parse_post(msg_caps, name, msgtype, args, cliid,
                                timeout) != 0)
    return false;
  if (!is_valid_msgtype(msgtype))
    return false;
  KLOGI(TAG, "recv msg(%u) %s from %s", msgtype, name.c_str(),
        sender->auth_extra.c_str());
  if (name.length() == 0)
    return false;

  SubscriptionMap::iterator sit;
  int32_t svrid = 0;
  bool has_subscription = true;

  sit = subscriptions.find(name);
  if (sit == subscriptions.end() || sit->second.empty())
    has_subscription = false;
  if (msgtype == FLORA_MSGTYPE_REQUEST)
    svrid = ++reqseq;
  if (has_subscription) {
    int32_t c = ResponseSerializer::serialize_post(name.c_str(), msgtype, args,
                                                   svrid, buffer, buf_size,
                                                   sender->serialize_flags);
    if (c < 0)
      return false;
    AdapterList::iterator ait;
    ait = sit->second.begin();
    while (ait != sit->second.end()) {
      if ((*ait)->closed()) {
        AdapterList::iterator dit = ait;
        ++ait;
        sit->second.erase(dit);
        continue;
      }
      KLOGI(TAG, "dispatch msg(%u) %s from %s to %s", msgtype, name.c_str(),
            sender->auth_extra.c_str(), (*ait)->auth_extra.c_str());
      (*ait)->write(buffer, c);
      ++ait;
    }
  }

  if (msgtype == FLORA_MSGTYPE_PERSIST) {
    PersistMsg &pmsg = persist_msgs[name];
    pmsg.data = args;
  } else if (msgtype == FLORA_MSGTYPE_REQUEST) {
    KLOGI(TAG, "reply manager add request: sender %s, name %s, timeout %u",
          sender->auth_extra.c_str(), name.c_str(), timeout);
    static AdapterList empty_adapter_list;
    if (has_subscription)
      reply_mgr.add_req(sender, name.c_str(), cliid, svrid, timeout,
                        sit->second);
    else
      reply_mgr.add_req(sender, name.c_str(), cliid, svrid, timeout,
                        empty_adapter_list);
  }
  return true;
}

bool Dispatcher::handle_reply_req(shared_ptr<Caps> &msg_caps,
                                  shared_ptr<Adapter> &sender) {
  string name;
  shared_ptr<Caps> args;
  int32_t id;
  int32_t retcode;

  if (RequestParser::parse_reply(msg_caps, name, args, id, retcode) != 0)
    return false;
  KLOGI(TAG, "handle_reply_req, name %s, retcode %d", name.c_str(), retcode);
  if (name.length() == 0)
    return false;
  reply_mgr.put_reply(sender, name.c_str(), id, retcode, args);
  return true;
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
