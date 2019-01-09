#include "cli.h"
#include "defs.h"
#include "rlog.h"
#include "ser-helper.h"
#include "sock-conn.h"
#include "uri.h"
#include <assert.h>
#include <string.h>
#include <sys/mman.h>

using namespace std;
using namespace std::chrono;
using rokid::Uri;

#define TAG "flora.Client"

int32_t flora::Client::connect(const char *uri, flora::ClientCallback *cb,
                               uint32_t msg_buf_size,
                               shared_ptr<flora::Client> &result) {
  uint32_t bufsize =
      msg_buf_size > DEFAULT_MSG_BUF_SIZE ? msg_buf_size : DEFAULT_MSG_BUF_SIZE;
  shared_ptr<flora::internal::Client> cli =
      make_shared<flora::internal::Client>(bufsize);
  int32_t r = cli->connect(uri, cb);
  if (r != FLORA_CLI_SUCCESS)
    return r;
  cli->set_weak_ptr(cli);
  result = static_pointer_cast<flora::Client>(cli);
  return r;
}

namespace flora {
namespace internal {

Client::Client(uint32_t bufsize) : buf_size(bufsize) {
  sbuffer = (int8_t *)mmap(NULL, buf_size * 2, PROT_READ | PROT_WRITE,
                           MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
  rbuffer = sbuffer + buf_size;
}

Client::~Client() noexcept {
  close(false);

  munmap(sbuffer, buf_size * 2);
#ifdef FLORA_DEBUG
  KLOGI(TAG,
        "client %s: post %u times, post %u bytes, "
        "send %u times, send %u bytes",
        auth_extra.c_str(), post_times, post_bytes, send_times, send_bytes);
#endif
}

int32_t Client::connect(const char *uri, ClientCallback *cb) {
  if (uri == nullptr)
    return FLORA_CLI_EINVAL;

  Uri urip;
  if (!urip.parse(uri)) {
    KLOGE(TAG, "invalid uri %s", uri);
    return FLORA_CLI_EINVAL;
  }
  KLOGI(TAG, "uri parse: path = %s", urip.path.c_str());
  KLOGI(TAG, "uri parse: fragment = %s", urip.fragment.c_str());

  SocketConn *conn = new SocketConn();
  if (urip.scheme == "unix") {
    if (!conn->connect(urip.path)) {
      delete conn;
      return FLORA_CLI_ECONN;
    }
    connection.reset(conn);
    serialize_flags = 0;
  } else if (urip.scheme == "tcp") {
    if (!conn->connect(urip.host, urip.port)) {
      delete conn;
      return FLORA_CLI_ECONN;
    }
    connection.reset(conn);
    serialize_flags = CAPS_FLAG_NET_BYTEORDER;
  } else {
    KLOGE(TAG, "unsupported uri scheme %s", urip.scheme.c_str());
    delete conn;
    return FLORA_CLI_EINVAL;
  }
  int32_t r = auth(urip.fragment);
  if (r != FLORA_CLI_SUCCESS) {
    close(false);
    return r;
  }
  callback = cb;
  auth_extra = urip.fragment;

  recv_thread = thread([this]() { this->recv_loop(); });
  return FLORA_CLI_SUCCESS;
}

int32_t Client::auth(const string &extra) {
  int32_t c = RequestSerializer::serialize_auth(
      FLORA_VERSION, extra.c_str(), sbuffer, buf_size, serialize_flags);
  if (c <= 0)
    return FLORA_CLI_EAUTH;
  if (!connection->send(sbuffer, c))
    return FLORA_CLI_EAUTH;
#ifdef FLORA_DEBUG
  ++send_times;
  send_bytes += c;
#endif
  c = connection->recv(rbuffer, buf_size);
  if (c <= 0)
    return FLORA_CLI_EAUTH;
  int32_t auth_res;
  if (ResponseParser::parse_auth(rbuffer, c, auth_res) != 0)
    return FLORA_CLI_EAUTH;
  return auth_res;
}

void Client::recv_loop() {
  int32_t err;
  while (true) {
    if (rbuf_off == buf_size) {
      KLOGW(TAG, "recv buffer not enough, %u bytes", buf_size);
      err = FLORA_CLI_EINSUFF_BUF;
      break;
    }
    int32_t c = connection->recv(rbuffer + rbuf_off, buf_size - rbuf_off);
    if (c <= 0) {
      err = FLORA_CLI_ECONN;
      break;
    }
    if (!handle_received(rbuf_off + c)) {
      err = FLORA_CLI_ECONN;
      break;
    }
  }
  iclose(true, err);
}

bool Client::handle_received(int32_t size) {
  shared_ptr<Caps> resp;
  uint32_t off = 0;
  uint32_t version;
  uint32_t length;
  int32_t cmd;

  while (off < size) {
    if (size - off < 8)
      break;
    if (Caps::binary_info(rbuffer + off, &version, &length) != CAPS_SUCCESS)
      return false;
    if (size - off < length)
      break;
    if (Caps::parse(rbuffer + off, length, resp, false) != CAPS_SUCCESS)
      return false;
    off += length;

    if (resp->read(cmd) != CAPS_SUCCESS) {
      return false;
    }
    switch (cmd) {
    case CMD_POST_RESP: {
      uint32_t msgtype;
      string name;
      shared_ptr<Caps> args;

      if (ResponseParser::parse_post(resp, name, msgtype, args) != 0) {
        return false;
      }
      if (callback) {
        callback->recv_post(name.c_str(), msgtype, args);
      }
      break;
    }
    case CMD_CALL_RESP: {
      string name;
      int32_t msgid;
      shared_ptr<Caps> args;
      if (ResponseParser::parse_call(resp, name, args, msgid) != 0) {
        return false;
      }
      if (callback) {
        shared_ptr<Reply> reply =
            make_shared<ReplyImpl>(this_weak_ptr.lock(), msgid);
        callback->recv_call(name.c_str(), args, reply);
      }
      break;
    }
    case CMD_REPLY_RESP: {
      string name;
      int32_t msgid;
      int32_t rescode;
      PendingRequestList::iterator it;
      Response response;

      if (ResponseParser::parse_reply(resp, msgid, rescode, response) != 0) {
        KLOGW(TAG, "parse reply failed");
        return false;
      }

      req_mutex.lock();
      for (it = pending_requests.begin(); it != pending_requests.end(); ++it) {
        if ((*it).id == msgid) {
          if ((*it).result) {
            (*it).id = 0;
            (*it).rcode = rescode;
            if (rescode == FLORA_CLI_SUCCESS) {
              (*it).result->ret_code = response.ret_code;
              (*it).result->data = response.data;
              (*it).result->extra = response.extra;
            }
            req_reply_cond.notify_all();
          } else {
            if (rescode != FLORA_CLI_SUCCESS) {
              response.ret_code = 0;
            }
            req_mutex.unlock();
            (*it).callback(rescode, response);
            req_mutex.lock();
            pending_requests.erase(it);
          }
          break;
        }
      }
      req_mutex.unlock();
      break;
    }
    default:
      KLOGE(TAG, "client received invalid command %d", cmd);
      return false;
    }
  }
  if (off > 0 && size - off > 0) {
    memmove(rbuffer, rbuffer + off, size - off);
  }
  rbuf_off = size - off;
  return true;
}

void Client::iclose(bool passive, int32_t err) {
  if (connection == nullptr || connection->closed())
    return;
  connection->close();

  req_mutex.lock();
  close_reason = err;
  req_reply_cond.notify_all();
  PendingRequestList::iterator it = pending_requests.begin();
  PendingRequestList::iterator rmit;
  while (it != pending_requests.end()) {
    Response resp;
    if ((*it).result == nullptr) {
      (*it).callback(err, resp);
      rmit = it;
      ++it;
      pending_requests.erase(rmit);
      continue;
    }
    ++it;
  }
  req_mutex.unlock();

  if (passive && callback) {
    callback->disconnected();
  }
}

void Client::close(bool passive) {
  iclose(passive, FLORA_CLI_ECLOSED);
  if (recv_thread.joinable())
    recv_thread.join();
}

void Client::send_reply(int32_t callid, int32_t code,
                        std::shared_ptr<Caps> &data) {
  int32_t c = RequestSerializer::serialize_reply(callid, code, data, sbuffer,
                                                 buf_size, serialize_flags);
  if (c <= 0)
    return;
  connection->send(sbuffer, c);
}

int32_t Client::subscribe(const char *name) {
  if (name == nullptr)
    return FLORA_CLI_EINVAL;
  int32_t c = RequestSerializer::serialize_subscribe(name, sbuffer, buf_size,
                                                     serialize_flags);
  if (c <= 0)
    return FLORA_CLI_EINVAL;
  if (!connection->send(sbuffer, c)) {
    return FLORA_CLI_ECONN;
  }
#ifdef FLORA_DEBUG
  ++send_times;
  send_bytes += c;
#endif
  return FLORA_CLI_SUCCESS;
}

int32_t Client::unsubscribe(const char *name) {
  if (name == nullptr)
    return FLORA_CLI_EINVAL;
  int32_t c = RequestSerializer::serialize_unsubscribe(name, sbuffer, buf_size,
                                                       serialize_flags);
  if (c <= 0)
    return FLORA_CLI_EINVAL;
  if (!connection->send(sbuffer, c)) {
    return FLORA_CLI_ECONN;
  }
#ifdef FLORA_DEBUG
  ++send_times;
  send_bytes += c;
#endif
  return FLORA_CLI_SUCCESS;
}

int32_t Client::declare_method(const char *name) {
  if (name == nullptr)
    return FLORA_CLI_EINVAL;
  int32_t c = RequestSerializer::serialize_declare_method(
      name, sbuffer, buf_size, serialize_flags);
  if (c <= 0)
    return FLORA_CLI_EINVAL;
  if (!connection->send(sbuffer, c)) {
    return FLORA_CLI_ECONN;
  }
#ifdef FLORA_DEBUG
  ++send_times;
  send_bytes += c;
#endif
  return FLORA_CLI_SUCCESS;
}

int32_t Client::remove_method(const char *name) {
  if (name == nullptr)
    return FLORA_CLI_EINVAL;
  int32_t c = RequestSerializer::serialize_remove_method(
      name, sbuffer, buf_size, serialize_flags);
  if (c <= 0)
    return FLORA_CLI_EINVAL;
  if (!connection->send(sbuffer, c)) {
    return FLORA_CLI_ECONN;
  }
#ifdef FLORA_DEBUG
  ++send_times;
  send_bytes += c;
#endif
  return FLORA_CLI_SUCCESS;
}

int32_t Client::post(const char *name, shared_ptr<Caps> &msg,
                     uint32_t msgtype) {
  if (name == nullptr || !is_valid_msgtype(msgtype))
    return FLORA_CLI_EINVAL;
  int32_t c = RequestSerializer::serialize_post(name, msgtype, msg, sbuffer,
                                                buf_size, serialize_flags);
  if (c <= 0)
    return FLORA_CLI_EINVAL;
  if (!connection->send(sbuffer, c)) {
    return FLORA_CLI_ECONN;
  }
#ifdef FLORA_DEBUG
  ++post_times;
  post_bytes += c;
  ++send_times;
  send_bytes += c;
#endif
  return FLORA_CLI_SUCCESS;
}

int32_t Client::call(const char *name, shared_ptr<Caps> &msg,
                     const char *target, Response &reply, uint32_t timeout) {
  if (name == nullptr)
    return FLORA_CLI_EINVAL;
  int32_t c = RequestSerializer::serialize_call(
      name, msg, target, ++reqseq, timeout, sbuffer, buf_size, serialize_flags);
  if (c <= 0)
    return FLORA_CLI_EINVAL;

  unique_lock<mutex> locker(req_mutex);
  PendingRequestList::iterator it =
      pending_requests.emplace(pending_requests.end());
  (*it).id = reqseq;
  (*it).result = &reply;
  locker.unlock();

  if (!connection->send(sbuffer, c)) {
    return FLORA_CLI_ECONN;
  }
#ifdef FLORA_DEBUG
  ++req_times;
  req_bytes += c;
  ++send_times;
  send_bytes += c;
#endif
  int32_t retcode;
  locker.lock();
  while (true) {
    // received reply
    if ((*it).id == 0) {
      retcode = (*it).rcode;
      break;
    }

    if (connection.get() == nullptr) {
      retcode = close_reason;
      goto exit;
    }

    req_reply_cond.wait(locker);
  }

exit:
  pending_requests.erase(it);
  return retcode;
}

int32_t Client::call(const char *name, shared_ptr<Caps> &msg,
                     const char *target,
                     function<void(int32_t, Response &)> &cb,
                     uint32_t timeout) {
  if (name == nullptr)
    return FLORA_CLI_EINVAL;
  int32_t c = RequestSerializer::serialize_call(
      name, msg, target, ++reqseq, timeout, sbuffer, buf_size, serialize_flags);
  if (c <= 0)
    return FLORA_CLI_EINVAL;

  req_mutex.lock();
  PendingRequestList::iterator it =
      pending_requests.emplace(pending_requests.end());
  (*it).id = reqseq;
  (*it).result = nullptr;
  (*it).callback = cb;
  req_mutex.unlock();

  if (!connection->send(sbuffer, c)) {
    return FLORA_CLI_ECONN;
  }
#ifdef FLORA_DEBUG
  ++req_times;
  req_bytes += c;
  ++send_times;
  send_bytes += c;
#endif
  return FLORA_CLI_SUCCESS;
}

int32_t Client::call(const char *name, shared_ptr<Caps> &msg,
                     const char *target,
                     function<void(int32_t, Response &)> &&cb,
                     uint32_t timeout) {
  return call(name, msg, target, cb, timeout);
}

ReplyImpl::ReplyImpl(shared_ptr<Client> &&c, int32_t id)
    : client(c), callid(id) {}

ReplyImpl::~ReplyImpl() noexcept { end(); }

void ReplyImpl::write_code(int32_t code) { ret_code = code; }

void ReplyImpl::write_data(shared_ptr<Caps> &data) { this->data = data; }

void ReplyImpl::end() {
  if (client != nullptr) {
    client->send_reply(callid, ret_code, data);
    client.reset();
  }
}

} // namespace internal
} // namespace flora

using flora::Reply;
using flora::Response;
using flora::internal::Client;

class WrapClientCallback : public flora::ClientCallback {
public:
  void recv_post(const char *name, uint32_t msgtype, shared_ptr<Caps> &msg);

  void recv_call(const char *name, shared_ptr<Caps> &msg,
                 shared_ptr<Reply> &reply);

  void disconnected();

  flora_cli_callback_t cb_funcs;
  void *arg;
};

void WrapClientCallback::recv_post(const char *name, uint32_t msgtype,
                                   shared_ptr<Caps> &msg) {
  if (cb_funcs.recv_post) {
    caps_t cmsg = Caps::convert(msg);
    cb_funcs.recv_post(name, msgtype, cmsg, arg);
    caps_destroy(cmsg);
  }
}

void WrapClientCallback::recv_call(const char *name, shared_ptr<Caps> &msg,
                                   shared_ptr<Reply> &reply) {
  if (cb_funcs.recv_call) {
    caps_t cmsg = Caps::convert(msg);
    CReply *creply = new CReply();
    creply->cxxreply = reply;
    cb_funcs.recv_call(name, cmsg, arg,
                       reinterpret_cast<flora_call_reply_t>(creply));
  }
}

void WrapClientCallback::disconnected() {
  if (cb_funcs.disconnected) {
    cb_funcs.disconnected(arg);
  }
}

typedef struct {
  shared_ptr<Client> cxxclient;
} CClient;

int32_t flora_cli_connect(const char *uri, flora_cli_callback_t *cb, void *arg,
                          uint32_t msg_buf_size, flora_cli_t *result) {
  if (result == nullptr)
    return FLORA_CLI_EINVAL;

  uint32_t bufsize =
      msg_buf_size > DEFAULT_MSG_BUF_SIZE ? msg_buf_size : DEFAULT_MSG_BUF_SIZE;
  shared_ptr<Client> cli = make_shared<Client>(bufsize);
  int32_t r;

  WrapClientCallback *wcb = nullptr;
  if (cb) {
    wcb = new WrapClientCallback();
    wcb->cb_funcs = *cb;
    wcb->arg = arg;
  }

  r = cli->connect(uri, wcb);
  if (r != FLORA_CLI_SUCCESS) {
    return r;
  }
  cli->set_weak_ptr(cli);
  CClient *ccli = new CClient();
  ccli->cxxclient = cli;
  *result = reinterpret_cast<flora_cli_t>(ccli);
  return FLORA_CLI_SUCCESS;
}

void flora_cli_delete(flora_cli_t handle) {
  if (handle) {
    CClient *cli = reinterpret_cast<CClient *>(handle);
    cli->cxxclient->close(false);
    if (cli->cxxclient->callback)
      delete cli->cxxclient->callback;
    delete cli;
  }
}

int32_t flora_cli_subscribe(flora_cli_t handle, const char *name) {
  if (handle == 0)
    return FLORA_CLI_EINVAL;
  return reinterpret_cast<CClient *>(handle)->cxxclient->subscribe(name);
}

int32_t flora_cli_unsubscribe(flora_cli_t handle, const char *name) {
  if (handle == 0)
    return FLORA_CLI_EINVAL;
  return reinterpret_cast<CClient *>(handle)->cxxclient->unsubscribe(name);
}

int32_t flora_cli_declare_method(flora_cli_t handle, const char *name) {
  if (handle == 0)
    return FLORA_CLI_EINVAL;
  return reinterpret_cast<CClient *>(handle)->cxxclient->declare_method(name);
}

int32_t flora_cli_remove_method(flora_cli_t handle, const char *name) {
  if (handle == 0)
    return FLORA_CLI_EINVAL;
  return reinterpret_cast<CClient *>(handle)->cxxclient->remove_method(name);
}

int32_t flora_cli_post(flora_cli_t handle, const char *name, caps_t msg,
                       uint32_t msgtype) {
  if (handle == 0)
    return FLORA_CLI_EINVAL;
  shared_ptr<Caps> pmsg = Caps::convert(msg);
  return reinterpret_cast<CClient *>(handle)->cxxclient->post(name, pmsg,
                                                              msgtype);
}

void cxxresp_to_cresp(Response &resp, flora_call_result &result) {
  result.ret_code = resp.ret_code;
  result.data = Caps::convert(resp.data);
  string *extra = &resp.extra;
  if (extra->length()) {
    result.extra = new char[extra->length() + 1];
    strcpy(result.extra, extra->c_str());
  } else
    result.extra = nullptr;
}

int32_t flora_cli_call(flora_cli_t handle, const char *name, caps_t msg,
                       const char *target, flora_call_result *result,
                       uint32_t timeout) {
  if (handle == 0)
    return FLORA_CLI_EINVAL;
  Response resp;
  shared_ptr<Caps> pmsg = Caps::convert(msg);
  int32_t r = reinterpret_cast<CClient *>(handle)->cxxclient->call(
      name, pmsg, target, resp, timeout);
  if (r != FLORA_CLI_SUCCESS)
    return r;
  if (result == nullptr)
    return FLORA_CLI_SUCCESS;
  cxxresp_to_cresp(resp, *result);
  return FLORA_CLI_SUCCESS;
}

void flora_result_delete(flora_call_result *result) {
  if (result == nullptr)
    return;
  if (result->extra) {
    delete[] result->extra;
    result->extra = nullptr;
  }
  if (result->data) {
    caps_destroy(result->data);
    result->data = 0;
  }
}

int32_t flora_cli_call_nb(flora_cli_t handle, const char *name, caps_t msg,
                          const char *target, flora_call_callback_t cb,
                          void *arg, uint32_t timeout) {
  if (handle == 0)
    return FLORA_CLI_EINVAL;
  shared_ptr<Caps> pmsg = Caps::convert(msg);
  int32_t r = reinterpret_cast<CClient *>(handle)->cxxclient->call(
      name, pmsg, target,
      [cb, arg](int32_t rcode, Response &resp) {
        if (cb) {
          flora_call_result result;
          if (rcode == FLORA_CLI_SUCCESS) {
            cxxresp_to_cresp(resp, result);
            cb(rcode, &result, arg);
            flora_result_delete(&result);
          } else
            cb(rcode, nullptr, arg);
        }
      },
      timeout);
  return r;
}

void flora_call_reply_write_code(flora_call_reply_t reply, int32_t code) {
  reinterpret_cast<CReply *>(reply)->cxxreply->write_code(code);
}

void flora_call_reply_write_data(flora_call_reply_t reply, caps_t data) {
  shared_ptr<Caps> cxxdata = Caps::convert(data);
  reinterpret_cast<CReply *>(reply)->cxxreply->write_data(cxxdata);
}

void flora_call_reply_end(flora_call_reply_t reply) {
  CReply *creply = reinterpret_cast<CReply *>(reply);
  creply->cxxreply->end();
  delete creply;
}
