#include <stdarg.h>
#include <thread>
#include "flora-agent.h"
#include "rlog.h"

#define TAG "flora.agent"

using namespace std;
using namespace std::chrono;

namespace flora {

void Agent::config(AgentConfigKey key, ...) {
  va_list ap;
  va_start(ap, key);
  switch (key) {
    case AgentConfigKey::URI:
      uri = va_arg(ap, const char*);
      break;
    case AgentConfigKey::BUFSIZE:
      bufsize = va_arg(ap, uint32_t);
      break;
    case AgentConfigKey::RECONN_INTERVAL:
      reconn_interval = milliseconds(va_arg(ap, uint32_t));
      break;
  }
  va_end(ap);
}

void Agent::subscribe(const char* name,
    function<void(shared_ptr<Caps>&, uint32_t, Reply*)>&& cb) {
  shared_ptr<Client> cli;
  conn_mutex.lock();
  auto r = msg_handlers.insert(make_pair(name, cb));
  cli = flora_cli;
  conn_mutex.unlock();
  if (r.second && cli.get())
    cli->subscribe(name);
}

void Agent::unsubscribe(const char* name) {
  shared_ptr<Client> cli;
  MsgHandlerMap::iterator it;

  conn_mutex.lock();
  it = msg_handlers.find(name);
  if (it != msg_handlers.end()) {
    msg_handlers.erase(it);
    cli = flora_cli;
  }
  conn_mutex.unlock();
  if (cli.get())
    cli->unsubscribe(name);
}

void Agent::start(bool block) {
  if (block) {
    working = true;
    run();
  } else {
    thread run_thread([this]() {
        working = true;
        this->run();
    });
    run_thread.detach();
  }
}

void Agent::run() {
  unique_lock<mutex> locker(conn_mutex);
  shared_ptr<Client> cli;
  int32_t r;

	while (working) {
		r = Client::connect(uri.c_str(), this, bufsize, cli);
		if (r != FLORA_CLI_SUCCESS) {
			KLOGI(TAG, "connect to flora service %s failed, retry after %u milliseconds",
					uri.c_str(), reconn_interval.count());
			conn_cond.wait_for(locker, reconn_interval);
		} else {
			KLOGI(TAG, "flora service %s connected", uri.c_str());
			subscribe_msgs(cli);
      flora_cli = cli;
			conn_cond.wait(locker);
		}
	}
}

void Agent::subscribe_msgs(shared_ptr<Client>& cli) {
  MsgHandlerMap::iterator it;

	for (it = msg_handlers.begin(); it != msg_handlers.end(); ++it) {
		cli->subscribe((*it).first.c_str());
	}
}

void Agent::close() {
  lock_guard<mutex> locker(conn_mutex);
  working = false;
  conn_cond.notify_one();
  flora_cli.reset();
}

int32_t Agent::post(const char* name, shared_ptr<Caps>& msg, uint32_t msgtype) {
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

int32_t Agent::get(const char* name, shared_ptr<Caps>& msg,
    ResponseArray& responses, uint32_t timeout) {
  shared_ptr<Client> cli;

  conn_mutex.lock();
  cli = flora_cli;
  conn_mutex.unlock();

  if (cli.get() == nullptr) {
    return FLORA_CLI_ECONN;
  }
  int32_t r = cli->get(name, msg, responses, timeout);
  if (r == FLORA_CLI_ECONN) {
    destroy_client();
  }
  return r;
}

int32_t Agent::get(const char* name, shared_ptr<Caps>& msg,
    function<void(ResponseArray&)>&& cb, uint32_t timeout) {
  return get(name, msg, cb, timeout);
}

int32_t Agent::get(const char* name, shared_ptr<Caps>& msg,
    function<void(ResponseArray&)>& cb, uint32_t timeout) {
  shared_ptr<Client> cli;

  conn_mutex.lock();
  cli = flora_cli;
  conn_mutex.unlock();

  if (cli.get() == nullptr) {
    return FLORA_CLI_ECONN;
  }
  int32_t r = cli->get(name, msg, cb, timeout);
  if (r == FLORA_CLI_ECONN) {
    destroy_client();
  }
  return r;
}

void Agent::destroy_client() {
  conn_mutex.lock();
  flora_cli.reset();
  conn_cond.notify_one();
  conn_mutex.unlock();
}

void Agent::recv_post(const char* name, uint32_t msgtype,
    shared_ptr<Caps>& msg) {
  conn_mutex.lock();
  MsgHandlerMap::iterator it = msg_handlers.find(name);
  auto cb = it->second;
  conn_mutex.unlock();
  cb(msg, msgtype, nullptr);
}

void Agent::recv_get(const char* name, shared_ptr<Caps>& msg, Reply& reply) {
  conn_mutex.lock();
  MsgHandlerMap::iterator it = msg_handlers.find(name);
  auto cb = it->second;
  conn_mutex.unlock();
  cb(msg, FLORA_MSGTYPE_REQUEST, &reply);
}

void Agent::disconnected() {
  thread tmp([this]() { this->destroy_client(); });
  tmp.detach();
}

} // namespace flora
