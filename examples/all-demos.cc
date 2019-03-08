#include "all-demos.h"
#include "rlog.h"
#include <unistd.h>

#define SERVICE_URI "unix:flora.example.sock"
// #define SERVICE_URI "tcp://0.0.0.0:37702/"

using namespace std;
using namespace flora;

void DemoAllInOne::run() {
  run_start_stop_service();
  run_post_msg();
  run_recv_msg();
  test_errors();
  test_reply_after_close();
  test_invoke_in_callback();
}

void DemoAllInOne::start_service() {
  poll = Poll::new_instance(SERVICE_URI);
  dispatcher = Dispatcher::new_instance();
  poll->start(dispatcher);
  dispatcher->run();
}

void DemoAllInOne::stop_service() {
  dispatcher->close();
  poll->stop();
  poll.reset();
  dispatcher.reset();
}

void DemoAllInOne::run_start_stop_service() {
  KLOGI(TAG, "============run start stop service============");
  start_service();
  stop_service();
}

static void config_agent(Agent &agent, char id) {
  char buf[64];
  snprintf(buf, sizeof(buf), "%s#%c", SERVICE_URI, id);
  agent.config(FLORA_AGENT_CONFIG_URI, buf);
  agent.start();
}

static void create_cagent(flora_agent_t *agent, char id) {
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

static void print_response(const char *prefix, Response &resp) {
  int32_t iv = -1;
  resp.data->read(iv);
  KLOGI(TAG, "%s: response retcode %d, value %d, from %s", prefix,
        resp.ret_code, iv, resp.extra.c_str());
}

static void print_c_response(const char *prefix, flora_call_result &resp) {
  int32_t iv = -1;
  caps_read_integer(resp.data, &iv);
  KLOGI(TAG, "%s: c response retcode %d, value %d, from %s", prefix,
        resp.ret_code, iv, resp.extra);
}

static void exam_call_callback(int32_t rescode, flora_call_result *result,
                               void *arg) {
  if (rescode == FLORA_CLI_SUCCESS) {
    print_c_response("c non-blocking call", *result);
    flora_result_delete(result);
  } else {
    KLOGI(TAG, "c non-blocking call failed: %d", rescode);
  }
}

static void agent_post(Agent &agent, flora_agent_t cagent) {
  shared_ptr<Caps> msg;
  agent.post("0", msg, FLORA_MSGTYPE_INSTANT);
  msg = Caps::new_instance();
  msg->write(1);
  agent.post("1", msg, FLORA_MSGTYPE_PERSIST);

  flora_agent_post(cagent, "0", 0, FLORA_MSGTYPE_INSTANT);
  caps_t cmsg = caps_create();
  caps_write_integer(cmsg, 1);
  flora_agent_post(cagent, "1", cmsg, FLORA_MSGTYPE_INSTANT);
  caps_destroy(cmsg);
}

static void agent_call(Agent &agent, flora_agent_t cagent, const char *target) {
  shared_ptr<Caps> msg = Caps::new_instance();
  Response resp;
  msg->write(101);
  int32_t r = agent.call("2", msg, target, resp);
  if (r != FLORA_CLI_SUCCESS) {
    KLOGI(TAG, "agent call 2 failed: %d", r);
  } else {
    print_response("blocking call", resp);
  }
  r = agent.call("3", msg, target,
                 [](int32_t rcode, Response &resp) {
                   if (rcode == FLORA_CLI_SUCCESS) {
                     print_response("non-blocking call", resp);
                   } else {
                     KLOGI(TAG, "non-blocking call 3 failed: %d", rcode);
                   }
                 },
                 0);
  if (r != FLORA_CLI_SUCCESS) {
    KLOGI(TAG, "agent call 3 failed: %d", r);
  }

  caps_t cmsg = 0;
  flora_call_result cresp;
  cmsg = caps_create();
  caps_write_integer(cmsg, 110);
  r = flora_agent_call(cagent, "2", cmsg, target, &cresp, 0);
  if (r != FLORA_CLI_SUCCESS) {
    KLOGI(TAG, "c agent call 2 failed: %d", r);
  } else {
    print_c_response("c blocking call", cresp);
  }
  r = flora_agent_call_nb(cagent, "3", cmsg, target, exam_call_callback,
                          nullptr, 0);
  if (r != FLORA_CLI_SUCCESS) {
    KLOGI(TAG, "c agent call 3 failed: %d", r);
  }
}

void DemoAllInOne::run_post_msg() {
  KLOGI(TAG, "============run post msg============");
  start_service();
  config_agent(agents[0], 'a');
  create_cagent(cagents, 'a');
  // wait agent connected
  sleep(1);
  agent_post(agents[0], cagents[0]);
  agent_call(agents[0], cagents[0], "a");
  // wait recv responses
  sleep(1);
  delete_cagent(cagents[0]);
  agents[0].close();
  stop_service();
}

static void exam_subscribe_callback(const char *name, caps_t msg, uint32_t type,
                                    void *arg) {
  int32_t iv = -1;
  switch ((intptr_t)arg) {
  case 0:
    KLOGI(TAG, "0: recv msg %s, type %u, content %p", name, type, msg);
    break;
  case 1:
    caps_read_integer(msg, &iv);
    KLOGI(TAG, "1: recv msg %s, type %u, content %d", name, type, iv);
    break;
  }
}

static void exam_declare_method_callback(const char *name, caps_t msg,
                                         flora_call_reply_t reply, void *arg) {
  int32_t iv = -1;
  caps_t data;
  switch ((intptr_t)arg) {
  case 2:
    caps_read_integer(msg, &iv);
    KLOGI(TAG, "2: recv call %s, content %d", name, iv);
    data = caps_create();
    caps_write_integer(data, 2);
    flora_call_reply_write_data(reply, data);
    flora_call_reply_end(reply);
    caps_destroy(data);
    break;
  case 3:
    caps_read_integer(msg, &iv);
    KLOGI(TAG, "3: recv call %s, content %d", name, iv);
    data = caps_create();
    caps_write_integer(data, 3);
    flora_call_reply_write_data(reply, data);
    flora_call_reply_end(reply);
    caps_destroy(data);
    break;
  }
}

static void agent_subscribe(Agent &agent, flora_agent_t cagent) {
  agent.subscribe(
      "0", [](const char *name, shared_ptr<Caps> &msg, uint32_t type) {
        KLOGI(TAG, "recv msg %s, type %u, content %p", name, type, msg.get());
      });
  agent.subscribe(
      "1", [](const char *name, shared_ptr<Caps> &msg, uint32_t type) {
        int32_t iv = -1;
        msg->read(iv);
        KLOGI(TAG, "recv msg %s, type %u, content %d", name, type, iv);
      });

  flora_agent_subscribe(cagent, "0", exam_subscribe_callback, (void *)0);
  flora_agent_subscribe(cagent, "1", exam_subscribe_callback, (void *)1);
}

static void agent_declare_methods(Agent &agent, flora_agent_t cagent) {
  agent.declare_method("2", [](const char *name, shared_ptr<Caps> &msg,
                               shared_ptr<Reply> &reply) {
    int32_t iv = -1;
    msg->read(iv);
    KLOGI(TAG, "recv call %s, content %d", name, iv);
    shared_ptr<Caps> data = Caps::new_instance();
    data->write(2);
    reply->write_data(data);
    reply->end();
  });
  agent.declare_method("3", [](const char *name, shared_ptr<Caps> &msg,
                               shared_ptr<Reply> &reply) {
    int32_t iv = -1;
    msg->read(iv);
    KLOGI(TAG, "recv call %s, content %d", name, iv);
    shared_ptr<Caps> data = Caps::new_instance();
    data->write(3);
    reply->write_data(data);
    reply->end();
  });

  flora_agent_declare_method(cagent, "2", exam_declare_method_callback,
                             (void *)2);
  flora_agent_declare_method(cagent, "3", exam_declare_method_callback,
                             (void *)3);
}

void DemoAllInOne::run_recv_msg() {
  KLOGI(TAG, "============run recv msg============");
  start_service();
  config_agent(agents[0], 'a');
  config_agent(agents[1], 'b');
  create_cagent(cagents, 'a');
  create_cagent(cagents + 1, 'b');
  agent_subscribe(agents[1], cagents[1]);
  agent_declare_methods(agents[1], cagents[1]);
  // wait agent connected and subscribe
  sleep(1);
  agent_post(agents[0], cagents[0]);
  agent_call(agents[0], cagents[0], "b");
  // wait recv responses
  sleep(1);
  delete_cagent(cagents[0]);
  delete_cagent(cagents[1]);
  agents[0].close();
  agents[1].close();
  stop_service();
}

void DemoAllInOne::test_errors() {
  KLOGI(TAG, "============test errors============");
  start_service();
  config_agent(agents[0], 'a');
  config_agent(agents[1], 'b');
  shared_ptr<Caps> msg;
  int32_t r = agents[0].post("foo", msg, 2);
  KLOGI(TAG, "post msg with wrong type, return %d, except %d", r,
        FLORA_CLI_EINVAL);

  Response resp;
  r = agents[0].call("foo", msg, "b", resp, 0);
  KLOGI(TAG, "call method that not exists, return %d, except %d", r,
        FLORA_CLI_ENEXISTS);

  r = agents[0].call(
      "foo", msg, "b",
      [](int32_t rescode, Response &resp) {
        KLOGI(TAG, "call method that not exists, rescode %d, except %d",
              rescode, FLORA_CLI_ENEXISTS);
      },
      0);
  KLOGI(TAG, "call method return %d, except %d", r, FLORA_CLI_SUCCESS);

  agents[1].declare_method("foo", [](const char *name, shared_ptr<Caps> &msg,
                                     shared_ptr<Reply> &reply) {
    KLOGI(TAG, "method %s invoked: sleep 1 second", name);
    sleep(1);
    reply->end();
  });
  // wait declare method finish
  usleep(200000);
  r = agents[0].call("foo", msg, "b", resp, 1500);
  KLOGI(TAG, "call method foo: rescode %d, except %d", r, FLORA_CLI_SUCCESS);
  agents[0].call("foo", msg, "b",
                 [](int32_t rescode, Response &resp) {
                   KLOGI(TAG, "call method foo: rescode %d, except %d", rescode,
                         FLORA_CLI_ETIMEOUT);
                 },
                 1);
  r = agents[0].call("foo", msg, "b", resp, 0);
  KLOGI(TAG, "call method foo: rescode %d, except %d", r, FLORA_CLI_ETIMEOUT);

  sleep(1);
  stop_service();
}

void DemoAllInOne::test_reply_after_close() {
  KLOGI(TAG, "============test reply after close============");
  start_service();

  const char *targetName = "reply-after-close";
  Agent agent;
  char buf[64];
  shared_ptr<Reply> outreply;
  snprintf(buf, sizeof(buf), "%s#%s", SERVICE_URI, targetName);
  agent.config(FLORA_AGENT_CONFIG_URI, buf);
  agent.declare_method("foo",
                       [&outreply](const char *name, shared_ptr<Caps> &args,
                                   shared_ptr<Reply> &reply) {
                         KLOGI(TAG, "remote method %s invoked", name);
                         outreply = reply;
                       });
  agent.start();
  // wait declare method finish
  usleep(200000);

  Response resp;
  shared_ptr<Caps> empty_arg;
  int32_t r = agent.call("foo", empty_arg, targetName, resp, 100);
  if (r != FLORA_CLI_ETIMEOUT) {
    KLOGE(TAG, "agent call should timeout, but %d", r);
    agent.close();
    stop_service();
    return;
  }

  r = agent.call("foo", empty_arg, targetName,
                 [](int32_t code, Response &resp) {
                   KLOGI(TAG, "remote method foo return: %d, %d", code,
                         resp.ret_code);
                 },
                 1000);
  if (r != FLORA_CLI_SUCCESS) {
    KLOGE(TAG, "agent call failed: %d", r);
    agent.close();
    stop_service();
    return;
  }
  usleep(200000);
  outreply.reset();

  Agent otherAgent;
  otherAgent.config(FLORA_AGENT_CONFIG_URI, SERVICE_URI);
  otherAgent.start();
  usleep(200000);
  r = otherAgent.call("foo", empty_arg, targetName,
                      [](int32_t code, Response &resp) {
                        KLOGI(TAG, "remote method foo return: %d, %d", code,
                              resp.ret_code);
                      },
                      500);
  if (r != FLORA_CLI_SUCCESS) {
    KLOGE(TAG, "agent call failed: %d", r);
    agent.close();
    stop_service();
    return;
  }
  usleep(200000);
  agent.close();
  outreply.reset();

  sleep(1);
  stop_service();
}

void DemoAllInOne::test_invoke_in_callback() {
  Agent agent;
  char buf[64];
  const char *targetName = "invoke-in-callback";

  start_service();

  snprintf(buf, sizeof(buf), "%s#%s", SERVICE_URI, targetName);
  agent.config(FLORA_AGENT_CONFIG_URI, buf);
  agent.subscribe("foo", [&agent](const char *name, shared_ptr<Caps> &,
                                  uint32_t) {
    shared_ptr<Caps> empty;
    Response resp;
    int32_t r = agent.call("not-exists", empty, "blah", resp);
    KLOGI(TAG, "invoke 'call' in callback function, return %d, excepted %d", r,
          FLORA_CLI_EDEADLOCK);
    r = agent.call(
        "not-exists", empty, "blah", [&agent](int32_t code, Response &) {
          KLOGI(TAG, "invoke async 'call' in callback, return %d, excepted %d",
                code, FLORA_CLI_ENEXISTS);
          agent.close();
        });
  });
  agent.start();
  shared_ptr<Caps> empty;
  agent.post("foo", empty);
  sleep(5);

  stop_service();
}
