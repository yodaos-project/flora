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

void Agent::subscribe(const char* name, uint32_t msgtype,
    function<void(shared_ptr<Caps>&)>& cb) {
  MsgId mid;
  mid.name = name;
  mid.type = msgtype;
  msg_handlers.insert(make_pair(mid, cb));
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
		cli->subscribe((*it).first.name.c_str(), (*it).first.type);
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

void Agent::destroy_client() {
  conn_mutex.lock();
  flora_cli.reset();
  conn_cond.notify_one();
  conn_mutex.unlock();
}

void Agent::recv_post(const char* name, uint32_t msgtype,
    shared_ptr<Caps>& msg) {
  MsgId key;
  key.name = name;
  key.type = msgtype;
  MsgHandlerMap::iterator it = msg_handlers.find(key);
  it->second(msg);
}

void Agent::disconnected() {
  thread tmp([this]() { this->destroy_client(); });
  tmp.detach();
}

} // namespace flora
