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
  agent.config(FLORA_AGENT_CONFIG_URI, buf);
  agent.start();
}

static void create_cagent(flora_agent_t* agent, char id) {
  char buf[64];
  snprintf(buf, sizeof(buf), "%s#c-%c", SERVICE_URI, id);
  *agent = flora_agent_create();
  flora_agent_config(*agent, FLORA_AGENT_CONFIG_URI, buf);
  flora_agent_start(*agent, 0);
}

static void delete_cagent(flora_agent_t agent) {
  flora_agent_close(agent);
  flora_agent_delete(agent);
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

static void print_c_responses(const char* prefix, flora_get_result* results, uint32_t size) {
  KLOGI(TAG, "%s: response num %d", prefix, size);
  uint32_t i;
  int32_t iv;
  for (i = 0; i < size; ++i) {
    iv = -1;
    caps_read_integer(results[i].data, &iv);
    KLOGI(TAG, "%d: ret %d, value %d, from %s", i, results[i].ret_code,
        iv, results[i].extra);
  }
}

static void exam_get_callback(flora_get_result* results, uint32_t size, void* arg) {
  print_c_responses("callback get", results, size);
  flora_result_delete(results, size);
}

static void agent_post(Agent& agent, flora_agent_t cagent) {
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

  flora_agent_post(cagent, "0", 0, FLORA_MSGTYPE_INSTANT);
  caps_t cmsg = caps_create();
  caps_write_integer(cmsg, 1);
  flora_agent_post(cagent, "1", cmsg, FLORA_MSGTYPE_INSTANT);
  flora_get_result* results;
  uint32_t result_size;
  int32_t r = flora_agent_get(cagent, "2", cmsg, &results, &result_size, 0);
  if (r == FLORA_CLI_SUCCESS) {
    print_c_responses("blocking get", results, result_size);
    flora_result_delete(results, result_size);
  }
  flora_agent_get_nb(cagent, "3", cmsg, exam_get_callback, nullptr);
  caps_destroy(cmsg);
}

void DemoAllInOne::run_post_msg() {
  start_service();
  config_agent(agents[0], 'a');
  create_cagent(cagents, 'a');
  // wait agent connected
  sleep(1);
  agent_post(agents[0], cagents[0]);
  // wait recv responses
  sleep(1);
  delete_cagent(cagents[0]);
  agents[0].close();
  stop_service();
}

static void exam_subscribe_callback(caps_t msg, uint32_t type, flora_reply_t* reply, void* arg) {
  int32_t iv = -1;

  switch ((intptr_t)arg) {
    case 0:
      KLOGI(TAG, "recv msg 0, type %u, content %p", type, msg);
      break;
    case 1:
      caps_read_integer(msg, &iv);
      KLOGI(TAG, "recv msg 1, type %u, content %d", type, iv);
      break;
    case 2:
      caps_read_integer(msg, &iv);
      KLOGI(TAG, "recv msg 2, type %u, content %d", type, iv);
      reply->ret_code = FLORA_CLI_SUCCESS;
      reply->data = caps_create();
      caps_write_integer(reply->data, 2);
      break;
    case 3:
      caps_read_integer(msg, &iv);
      KLOGI(TAG, "recv msg 3, type %u, content %d", type, iv);
      reply->ret_code = FLORA_CLI_SUCCESS;
      reply->data = caps_create();
      caps_write_integer(reply->data, 3);
      break;
  }
}

static void agent_subscribe(Agent& agent, flora_agent_t cagent) {
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

  flora_agent_subscribe(cagent, "0", exam_subscribe_callback, (void*)0);
  flora_agent_subscribe(cagent, "1", exam_subscribe_callback, (void*)1);
  flora_agent_subscribe(cagent, "2", exam_subscribe_callback, (void*)2);
  flora_agent_subscribe(cagent, "3", exam_subscribe_callback, (void*)3);
}

void DemoAllInOne::run_recv_msg() {
  start_service();
  config_agent(agents[0], 'a');
  config_agent(agents[1], 'b');
  create_cagent(cagents, 'a');
  create_cagent(cagents + 1, 'b');
  agent_subscribe(agents[1], cagents[1]);
  // wait agent connected and subscribe
  sleep(1);
  agent_post(agents[0], cagents[0]);
  // wait recv responses
  sleep(1);
  delete_cagent(cagents[0]);
  delete_cagent(cagents[1]);
  agents[0].close();
  agents[1].close();
  stop_service();
}
