#include <unistd.h>
#include "all-demos.h"
#include "rlog.h"

#define SERVICE_URI "unix:flora.example.sock"

using namespace std;
using namespace flora;

void DemoAllInOne::run() {
  run_start_stop_service();
  run_post_msg();
  run_recv_msg();
}

void DemoAllInOne::start_service() {
  poll = Poll::new_instance(SERVICE_URI);
  dispatcher = Dispatcher::new_instance();
  poll->start(dispatcher);
}

void DemoAllInOne::stop_service() {
  poll->stop();
}

void DemoAllInOne::run_start_stop_service() {
  start_service();
  stop_service();
}

static void config_agent(Agent& agent, char id) {
  char buf[64];
  snprintf(buf, sizeof(buf), "%s#%c", SERVICE_URI, id);
  agent.config(AgentConfigKey::URI, buf);
  agent.start();
}

static void print_responses(const char* prefix, ResponseArray& resps) {
  KLOGI(TAG, "%s: response num %d", prefix, resps.size());
  size_t i;
  int32_t iv;
  for (i = 0; i < resps.size(); ++i) {
    iv = -1;
    resps[i].data->read(iv);
    KLOGI(TAG, "%d: ret %d, value %d, from %s", i, resps[i].ret_code,
        iv, resps[i].extra.c_str());
  }
}

static void agent_post(Agent& agent) {
  shared_ptr<Caps> msg;
  ResponseArray resps;
  agent.post("0", msg, FLORA_MSGTYPE_INSTANT);
  msg = Caps::new_instance();
  msg->write(1);
  agent.post("1", msg, FLORA_MSGTYPE_PERSIST);
  agent.get("2", msg, resps);
  print_responses("blocking get", resps);
  agent.get("3", msg, [](ResponseArray& resps) {
      print_responses("callback get", resps);
    });
}

void DemoAllInOne::run_post_msg() {
  start_service();
  config_agent(agents[0], 'a');
  // wait agent connected
  sleep(1);
  agent_post(agents[0]);
  // wait recv responses
  sleep(1);
  agents[0].close();
  stop_service();
}

static void agent_subscribe(Agent& agent) {
  agent.subscribe("0", [](shared_ptr<Caps>& msg, uint32_t type, Reply*) {
      KLOGI(TAG, "recv msg 0, type %u, content %p", type, msg.get());
    });
  agent.subscribe("1", [](shared_ptr<Caps>& msg, uint32_t type, Reply*) {
      int32_t iv = -1;
      msg->read(iv);
      KLOGI(TAG, "recv msg 1, type %u, content %d", type, iv);
    });
  agent.subscribe("2", [](shared_ptr<Caps>& msg, uint32_t type, Reply* reply) {
      int32_t iv = -1;
      msg->read(iv);
      KLOGI(TAG, "recv msg 2, type %u, content %d", type, iv);
      reply->ret_code = FLORA_CLI_SUCCESS;
      reply->data = Caps::new_instance();
      reply->data->write(2);
    });
  agent.subscribe("3", [](shared_ptr<Caps>& msg, uint32_t type, Reply* reply) {
      int32_t iv = -1;
      msg->read(iv);
      KLOGI(TAG, "recv msg 3, type %u, content %d", type, iv);
      reply->ret_code = FLORA_CLI_SUCCESS;
      reply->data = Caps::new_instance();
      reply->data->write(3);
    });
}

void DemoAllInOne::run_recv_msg() {
  start_service();
  config_agent(agents[0], 'a');
  config_agent(agents[1], 'b');
  agent_subscribe(agents[1]);
  // wait agent connected and subscribe
  sleep(1);
  agent_post(agents[0]);
  // wait recv responses
  sleep(1);
  agents[0].close();
  agents[1].close();
  stop_service();
}
