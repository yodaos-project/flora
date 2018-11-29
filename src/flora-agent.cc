#include <string.h>
#include <thread>
#include "flora-agent.h"
#include "rlog.h"

#define TAG "flora.agent"

using namespace std;
using namespace std::chrono;

namespace flora {

Agent::~Agent() {
  close();
}

void Agent::config(uint32_t key, ...) {
  va_list ap;
  va_start(ap, key);
  config(key, ap);
  va_end(ap);
}

void Agent::config(uint32_t key, va_list ap) {
  switch (key) {
    case FLORA_AGENT_CONFIG_URI:
      uri = va_arg(ap, const char*);
      break;
    case FLORA_AGENT_CONFIG_BUFSIZE:
      bufsize = va_arg(ap, uint32_t);
      break;
    case FLORA_AGENT_CONFIG_RECONN_INTERVAL:
      reconn_interval = milliseconds(va_arg(ap, uint32_t));
      break;
  }
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
    unique_lock<mutex> locker(conn_mutex);
    run_thread = thread([this]() {
        working = true;
        this->run();
    });
    // wait run thread execute
    conn_cond.wait(locker);
  }
}

void Agent::run() {
  unique_lock<mutex> locker(conn_mutex);
  shared_ptr<Client> cli;
  int32_t r;

  // notify 'Agent.start'
  conn_cond.notify_one();

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
  unique_lock<mutex> locker(conn_mutex);
  if (working) {
    working = false;
    conn_cond.notify_one();
    flora_cli.reset();
    locker.unlock();
    if (run_thread.joinable())
      run_thread.join();
  }
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
    function<void(ResponseArray&)>&& cb) {
  return get(name, msg, cb);
}

int32_t Agent::get(const char* name, shared_ptr<Caps>& msg,
    function<void(ResponseArray&)>& cb) {
  shared_ptr<Client> cli;

  conn_mutex.lock();
  cli = flora_cli;
  conn_mutex.unlock();

  if (cli.get() == nullptr) {
    return FLORA_CLI_ECONN;
  }
  int32_t r = cli->get(name, msg, cb);
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

using namespace flora;
flora_agent_t flora_agent_create() {
  Agent* agent = new Agent();
  return reinterpret_cast<flora_agent_t>(agent);
}

void flora_agent_config(flora_agent_t agent, uint32_t key, ...) {
  Agent* cxxagent = reinterpret_cast<Agent*>(agent);
  va_list ap;
  va_start(ap, key);
  cxxagent->config(key, ap);
  va_end(ap);
}

void flora_agent_subscribe(flora_agent_t agent, const char* name,
    flora_agent_subscribe_callback_t cb, void* arg) {
  Agent* cxxagent = reinterpret_cast<Agent*>(agent);
  cxxagent->subscribe(name, [cb, arg](shared_ptr<Caps>& msg, uint32_t type, Reply* reply) {
      caps_t cmsg = Caps::convert(msg);
      flora_reply_t creply;
      memset(&creply, 0, sizeof(creply));
      cb(cmsg, type, &creply, arg);
      caps_destroy(cmsg);
      if (type == FLORA_MSGTYPE_REQUEST) {
        reply->ret_code = creply.ret_code;
        reply->data = Caps::convert(creply.data);
      }
      caps_destroy(creply.data);
  });
}

void flora_agent_unsubscribe(flora_agent_t agent, const char* name) {
  Agent* cxxagent = reinterpret_cast<Agent*>(agent);
  cxxagent->unsubscribe(name);
}

void flora_agent_start(flora_agent_t agent, int32_t block) {
  Agent* cxxagent = reinterpret_cast<Agent*>(agent);
  cxxagent->start(block);
}

void flora_agent_close(flora_agent_t agent) {
  Agent* cxxagent = reinterpret_cast<Agent*>(agent);
  cxxagent->close();
}

int32_t flora_agent_post(flora_agent_t agent, const char* name,
    caps_t msg, uint32_t msgtype) {
  Agent* cxxagent = reinterpret_cast<Agent*>(agent);
  shared_ptr<Caps> cxxmsg = Caps::convert(msg);
  return cxxagent->post(name, cxxmsg, msgtype);
}

void cxxreplys_to_creplys(ResponseArray& replys, flora_get_result* results);
int32_t flora_agent_get(flora_agent_t agent, const char* name, caps_t msg,
    flora_get_result** results, uint32_t* res_size, uint32_t timeout) {
  Agent* cxxagent = reinterpret_cast<Agent*>(agent);
  shared_ptr<Caps> cxxmsg = Caps::convert(msg);
  ResponseArray resps;
  int32_t r = cxxagent->get(name, cxxmsg, resps, timeout);
  if (r != FLORA_CLI_SUCCESS)
    return r;
  if (resps.empty()) {
    *res_size = 0;
    *results = nullptr;
  }
  *results = new flora_get_result[resps.size()];
  cxxreplys_to_creplys(resps, *results);
  *res_size = resps.size();
  return FLORA_CLI_SUCCESS;
}

int32_t flora_agent_get_nb(flora_agent_t agent, const char* name, caps_t msg,
    flora_agent_get_callback_t cb, void* arg) {
  Agent* cxxagent = reinterpret_cast<Agent*>(agent);
  shared_ptr<Caps> cxxmsg = Caps::convert(msg);
  return cxxagent->get(name, cxxmsg, [cb, arg](ResponseArray& resps) {
    flora_get_result* results;
    if (resps.empty()) {
      cb(nullptr, 0, arg);
    } else {
      results = new flora_get_result[resps.size()];
      cxxreplys_to_creplys(resps, results);
      cb(results, resps.size(), arg);
    }
  });
}

void flora_agent_delete(flora_agent_t agent) {
  delete reinterpret_cast<Agent*>(agent);
}
