#include "flora-agent.h"
#include "cli.h"
#include "rlog.h"
#include <string.h>
#include <thread>

using namespace std;
using namespace std::chrono;

namespace flora {

Agent::~Agent() { close(); }

void Agent::config(uint32_t key, ...) {
  va_list ap;
  va_start(ap, key);
  config(key, ap);
  va_end(ap);
}

void Agent::config(uint32_t key, va_list ap) {
  switch (key) {
  case FLORA_AGENT_CONFIG_URI:
    options.uri = va_arg(ap, const char *);
    break;
  case FLORA_AGENT_CONFIG_BUFSIZE:
    options.bufsize = va_arg(ap, uint32_t);
    break;
  case FLORA_AGENT_CONFIG_RECONN_INTERVAL:
    options.reconn_interval = milliseconds(va_arg(ap, uint32_t));
    break;
  case FLORA_AGENT_CONFIG_MONITOR:
    options.flags = va_arg(ap, uint32_t);
    if (options.flags & FLORA_CLI_FLAG_MONITOR) {
      options.mon_callback = va_arg(ap, MonitorCallback *);
    }
    break;
  case FLORA_AGENT_CONFIG_KEEPALIVE:
    options.beep_interval = va_arg(ap, uint32_t);
    options.noresp_timeout = va_arg(ap, uint32_t);
    break;
  }
}

void Agent::subscribe(const char *name, PostHandler &&cb) {
  subscribe(name, cb);
}

void Agent::subscribe(const char *name, PostHandler &cb) {
  shared_ptr<Client> cli;
  conn_mutex.lock();
  auto r = post_handlers.insert(make_pair(name, cb));
  cli = flora_cli;
  conn_mutex.unlock();
  if (r.second && cli.get())
    cli->subscribe(name);
}

void Agent::unsubscribe(const char *name) {
  shared_ptr<Client> cli;
  PostHandlerMap::iterator it;

  conn_mutex.lock();
  it = post_handlers.find(name);
  if (it != post_handlers.end()) {
    post_handlers.erase(it);
    cli = flora_cli;
  }
  conn_mutex.unlock();
  if (cli.get())
    cli->unsubscribe(name);
}

void Agent::declare_method(const char *name, CallHandler &&cb) {
  declare_method(name, cb);
}

void Agent::declare_method(const char *name, CallHandler &cb) {
  shared_ptr<Client> cli;
  conn_mutex.lock();
  auto r = call_handlers.insert(make_pair(name, cb));
  cli = flora_cli;
  conn_mutex.unlock();
  if (r.second && cli.get())
    cli->declare_method(name);
}

void Agent::remove_method(const char *name) {
  shared_ptr<Client> cli;
  CallHandlerMap::iterator it;

  conn_mutex.lock();
  it = call_handlers.find(name);
  if (it != call_handlers.end()) {
    call_handlers.erase(it);
    cli = flora_cli;
  }
  conn_mutex.unlock();
  if (cli.get())
    cli->remove_method(name);
}

void Agent::start(bool block) {
  if (block) {
    working = true;
    run();
  } else {
    unique_lock<mutex> locker(conn_mutex);
    run_thread = thread([this]() {
      working = true;
      this->run();
    });
    // wait run thread execute and connect service
    start_cond.wait(locker);
  }
}

void Agent::run() {
  unique_lock<mutex> locker(conn_mutex);
  shared_ptr<Client> cli;
  flora::ClientOptions cliopts;

  cliopts.bufsize = options.bufsize;
  cliopts.flags = options.flags;
  cliopts.beep_interval = options.beep_interval;
  cliopts.noresp_timeout = options.noresp_timeout;
  while (working) {
    int32_t r = Client::connect(options.uri.c_str(), this, options.mon_callback,
                                &cliopts, cli);
    if (r != FLORA_CLI_SUCCESS) {
      KLOGI(TAG,
            "connect to flora service %s failed, retry after %u milliseconds",
            options.uri.c_str(), options.reconn_interval.count());
      start_cond.notify_one();
      conn_cond.wait_for(locker, options.reconn_interval);
    } else {
      KLOGI(TAG, "flora service %s connected", options.uri.c_str());
      init_cli(cli);
      flora_cli = cli;
      start_cond.notify_one();
      conn_cond.wait(locker);
    }
  }
}

void Agent::init_cli(shared_ptr<Client> &cli) {
  PostHandlerMap::iterator pit;
  CallHandlerMap::iterator cit;

  for (pit = post_handlers.begin(); pit != post_handlers.end(); ++pit) {
    cli->subscribe((*pit).first.c_str());
  }
  for (cit = call_handlers.begin(); cit != call_handlers.end(); ++cit) {
    cli->declare_method((*cit).first.c_str());
  }
}

void Agent::close() {
  unique_lock<mutex> locker(conn_mutex);
  if (working) {
    working = false;
    conn_cond.notify_one();
    shared_ptr<flora::internal::Client> cli =
        static_pointer_cast<flora::internal::Client>(flora_cli);
    flora_cli.reset();
    post_handlers.clear();
    call_handlers.clear();
    locker.unlock();
    if (cli != nullptr && cli->close(false) == FLORA_CLI_EDEADLOCK) {
      thread tmp([cli]() { cli->close(false); });
      tmp.detach();
    }
    if (run_thread.joinable())
      run_thread.join();
  }
}

int32_t Agent::post(const char *name, shared_ptr<Caps> &msg, uint32_t msgtype) {
  shared_ptr<Client> cli;

  conn_mutex.lock();
  cli = flora_cli;
  conn_mutex.unlock();

  if (cli.get() == nullptr) {
    return FLORA_CLI_ECONN;
  }
  int32_t r = cli->post(name, msg, msgtype);
  if (r == FLORA_CLI_ECONN) {
    destroy_client();
  }
  return r;
}

int32_t Agent::call(const char *name, shared_ptr<Caps> &msg, const char *target,
                    Response &response, uint32_t timeout) {
  shared_ptr<Client> cli;

  conn_mutex.lock();
  cli = flora_cli;
  conn_mutex.unlock();

  if (cli.get() == nullptr) {
    return FLORA_CLI_ECONN;
  }
  int32_t r = cli->call(name, msg, target, response, timeout);
  if (r == FLORA_CLI_ECONN) {
    destroy_client();
  }
  return r;
}

int32_t Agent::call(const char *name, shared_ptr<Caps> &msg, const char *target,
                    function<void(int32_t, Response &)> &&cb,
                    uint32_t timeout) {
  return call(name, msg, target, cb, timeout);
}

int32_t Agent::call(const char *name, shared_ptr<Caps> &msg, const char *target,
                    function<void(int32_t, Response &)> &cb, uint32_t timeout) {
  shared_ptr<Client> cli;

  conn_mutex.lock();
  cli = flora_cli;
  conn_mutex.unlock();

  if (cli.get() == nullptr) {
    return FLORA_CLI_ECONN;
  }
  int32_t r = cli->call(name, msg, target, cb, timeout);
  if (r == FLORA_CLI_ECONN) {
    destroy_client();
  }
  return r;
}

void Agent::destroy_client() {
  conn_mutex.lock();
  conn_cond.notify_one();
  conn_mutex.unlock();
}

void Agent::recv_post(const char *name, uint32_t msgtype,
                      shared_ptr<Caps> &msg) {
  unique_lock<mutex> locker(conn_mutex);
  PostHandlerMap::iterator it = post_handlers.find(name);
  if (it != post_handlers.end()) {
    auto cb = it->second;
    locker.unlock();
    cb(name, msg, msgtype);
  }
}

void Agent::recv_call(const char *name, shared_ptr<Caps> &msg,
                      shared_ptr<Reply> &reply) {
  unique_lock<mutex> locker(conn_mutex);
  CallHandlerMap::iterator it = call_handlers.find(name);
  if (it != call_handlers.end()) {
    auto cb = it->second;
    locker.unlock();
    cb(name, msg, reply);
  }
}

void Agent::disconnected() { destroy_client(); }

} // namespace flora

using namespace flora;
flora_agent_t flora_agent_create() {
  Agent *agent = new Agent();
  return reinterpret_cast<flora_agent_t>(agent);
}

void flora_agent_config(flora_agent_t agent, uint32_t key, ...) {
  Agent *cxxagent = reinterpret_cast<Agent *>(agent);
  va_list ap;
  va_start(ap, key);
  cxxagent->config(key, ap);
  va_end(ap);
}

void flora_agent_subscribe(flora_agent_t agent, const char *name,
                           flora_agent_subscribe_callback_t cb, void *arg) {
  Agent *cxxagent = reinterpret_cast<Agent *>(agent);
  cxxagent->subscribe(
      name, [cb, arg](const char *name, shared_ptr<Caps> &msg, uint32_t type) {
        caps_t cmsg = Caps::convert(msg);
        cb(name, cmsg, type, arg);
        caps_destroy(cmsg);
      });
}

void flora_agent_unsubscribe(flora_agent_t agent, const char *name) {
  Agent *cxxagent = reinterpret_cast<Agent *>(agent);
  cxxagent->unsubscribe(name);
}

void flora_agent_declare_method(flora_agent_t agent, const char *name,
                                flora_agent_declare_method_callback_t cb,
                                void *arg) {
  Agent *cxxagent = reinterpret_cast<Agent *>(agent);
  cxxagent->declare_method(name, [cb, arg](const char *name,
                                           shared_ptr<Caps> &msg,
                                           shared_ptr<Reply> &reply) {
    caps_t cmsg = Caps::convert(msg);
    CReply *creply = new CReply();
    creply->cxxreply = reply;
    cb(name, cmsg, reinterpret_cast<intptr_t>(creply), arg);
  });
}

void flora_agent_start(flora_agent_t agent, int32_t block) {
  Agent *cxxagent = reinterpret_cast<Agent *>(agent);
  cxxagent->start(block);
}

void flora_agent_close(flora_agent_t agent) {
  Agent *cxxagent = reinterpret_cast<Agent *>(agent);
  cxxagent->close();
}

int32_t flora_agent_post(flora_agent_t agent, const char *name, caps_t msg,
                         uint32_t msgtype) {
  Agent *cxxagent = reinterpret_cast<Agent *>(agent);
  shared_ptr<Caps> cxxmsg = Caps::convert(msg);
  return cxxagent->post(name, cxxmsg, msgtype);
}

void cxxresp_to_cresp(Response &resp, flora_call_result &result);
int32_t flora_agent_call(flora_agent_t agent, const char *name, caps_t msg,
                         const char *target, flora_call_result *result,
                         uint32_t timeout) {
  Agent *cxxagent = reinterpret_cast<Agent *>(agent);
  shared_ptr<Caps> cxxmsg = Caps::convert(msg);
  Response resp;
  int32_t r = cxxagent->call(name, cxxmsg, target, resp, timeout);
  if (r != FLORA_CLI_SUCCESS)
    return r;
  if (result)
    cxxresp_to_cresp(resp, *result);
  return FLORA_CLI_SUCCESS;
}

int32_t flora_agent_call_nb(flora_agent_t agent, const char *name, caps_t msg,
                            const char *target, flora_call_callback_t cb,
                            void *arg, uint32_t timeout) {
  Agent *cxxagent = reinterpret_cast<Agent *>(agent);
  shared_ptr<Caps> cxxmsg = Caps::convert(msg);
  return cxxagent->call(name, cxxmsg, target,
                        [cb, arg](int32_t rescode, Response &resp) {
                          flora_call_result result;
                          if (rescode == FLORA_CLI_SUCCESS) {
                            cxxresp_to_cresp(resp, result);
                            cb(rescode, &result, arg);
                          } else {
                            cb(rescode, nullptr, arg);
                          }
                        },
                        timeout);
}

void flora_agent_delete(flora_agent_t agent) {
  delete reinterpret_cast<Agent *>(agent);
}
