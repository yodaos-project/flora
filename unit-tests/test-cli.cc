#include "test-cli.h"
#include "rlog.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_POST_COUNT 256
#define MAX_CALL_COUNT 256
#define TAG "unit-test.TestClient"

FloraMsg TestClient::flora_msgs[FLORA_MSG_COUNT];
flora_cli_callback_t TestClient::flora_callback;

void TestClient::recv_post_s(const char *name, uint32_t msgtype, caps_t msg,
                             void *arg) {
  TestClient *tc = reinterpret_cast<TestClient *>(arg);
  tc->c_recv_post(name, msgtype, msg);
}

void TestClient::recv_call_s(const char *name, caps_t msg, void *arg,
                             flora_call_reply_t reply) {
  TestClient *tc = reinterpret_cast<TestClient *>(arg);
  tc->c_recv_call(name, msg, reply);
}

static void conn_disconnected(void *arg) {}

void TestClient::static_init(bool capi) {
  int32_t i;
  for (i = 0; i < FLORA_MSG_COUNT; ++i) {
    snprintf(flora_msgs[i].name, sizeof(flora_msgs[i].name), "foo%02d", i);
    if (i % 2 == 0) {
      if (capi) {
        flora_msgs[i].c_args = caps_create();
        caps_write_integer(flora_msgs[i].c_args, i);
      } else {
        flora_msgs[i].args = Caps::new_instance();
        flora_msgs[i].args->write(i);
      }
    } else {
      flora_msgs[i].args = 0;
    }
  }

  if (capi) {
    flora_callback.recv_post = TestClient::recv_post_s;
    flora_callback.disconnected = conn_disconnected;
    flora_callback.recv_call = TestClient::recv_call_s;
  }
}

bool TestClient::init(const char *uri, bool capi) {
  use_c_api = capi;
  if (capi)
    return flora_cli_connect(uri, &flora_callback, this, 0, &c_flora_cli) ==
           FLORA_CLI_SUCCESS;
  return Client::connect(uri, this, 0, flora_cli) == FLORA_CLI_SUCCESS;
}

void TestClient::do_subscribe() {
  int32_t i;
  int32_t r;

  memset(recv_post_counter, 0, sizeof(recv_post_counter));
  memset(recv_call_counter, 0, sizeof(recv_call_counter));
  memset(subscribe_flags, 0, sizeof(subscribe_flags));
  memset(declare_method_flags, 0, sizeof(declare_method_flags));
  for (i = 0; i < FLORA_MSG_COUNT; ++i) {
    bool dosub = (rand() % 5) <= 2;
    if (dosub) {
      if (use_c_api) {
        r = flora_cli_subscribe(c_flora_cli, flora_msgs[i].name);
      } else {
        r = flora_cli->subscribe(flora_msgs[i].name);
      }

      if (r != FLORA_CLI_SUCCESS)
        KLOGE(TAG, "client subscribe failed");
      else
        subscribe_flags[i] = 1;
    }

    bool dodecl = (rand() % 5) <= 2;
    if (dodecl) {
      if (use_c_api) {
        r = flora_cli_declare_method(c_flora_cli, flora_msgs[i].name);
      } else {
        r = flora_cli->declare_method(flora_msgs[i].name);
      }

      if (r != FLORA_CLI_SUCCESS)
        KLOGE(TAG, "client declare method failed");
      else
        declare_method_flags[i] = 1;
    }
  }
}

void TestClient::do_post() {
  int32_t c = rand() % MAX_POST_COUNT;
  int32_t i;
  int32_t r;

  memset(post_counter, 0, sizeof(post_counter));
  for (i = 0; i < c; ++i) {
    int32_t idx = rand() % FLORA_MSG_COUNT;
    if (use_c_api) {
      r = flora_cli_post(c_flora_cli, flora_msgs[idx].name,
                         flora_msgs[idx].c_args, FLORA_MSGTYPE_INSTANT);
    } else {
      r = flora_cli->post(flora_msgs[idx].name, flora_msgs[idx].args,
                          FLORA_MSGTYPE_INSTANT);
    }
    if (r != FLORA_CLI_SUCCESS)
      KLOGE(TAG, "client post failed: %d", r);
    else
      ++post_counter[idx];
  }
}

void TestClient::do_call(int32_t clinum) {
  int32_t i;
  int32_t r;
  int32_t c = rand() % MAX_CALL_COUNT;
  char clid[8];

  memset(call_counter, 0, sizeof(call_counter));
  for (i = 0; i < c; ++i) {
    int32_t idx = rand() % FLORA_MSG_COUNT;
    int32_t idn = rand() % clinum;
    snprintf(clid, sizeof(clid), "%03d", idn);
    if (use_c_api) {
      flora_call_result result;
      r = flora_cli_call(c_flora_cli, flora_msgs[idx].name,
                         flora_msgs[idx].c_args, clid, &result, 0);
    } else {
      Response resp;
      r = flora_cli->call(flora_msgs[idx].name, flora_msgs[idx].args, clid,
                          resp, 0);
    }
    if (r == FLORA_CLI_SUCCESS)
      ++call_counter[idx];
    else if (r != FLORA_CLI_ENEXISTS)
      KLOGE(TAG, "client call failed: %d", r);
  }
}

void TestClient::reset() {
  int32_t i;
  int32_t r;

  for (i = 0; i < FLORA_MSG_COUNT; ++i) {
    if (use_c_api) {
      r = flora_cli_unsubscribe(c_flora_cli, flora_msgs[i].name);
    } else {
      r = flora_cli->unsubscribe(flora_msgs[i].name);
    }
    if (r != FLORA_CLI_SUCCESS)
      KLOGE(TAG, "client unsubscribe failed");
  }
  memset(subscribe_flags, 0, sizeof(subscribe_flags));
  memset(post_counter, 0, sizeof(post_counter));
}

void TestClient::close() {
  if (use_c_api) {
    flora_cli_delete(c_flora_cli);
    c_flora_cli = 0;
  } else
    flora_cli.reset();
}

void TestClient::recv_post(const char *name, uint32_t msgtype,
                           shared_ptr<Caps> &args) {
  int32_t i;
  int32_t v;

  for (i = 0; i < FLORA_MSG_COUNT; ++i) {
    if (strcmp(name, flora_msgs[i].name) == 0) {
      ++recv_post_counter[i];
      if (flora_msgs[i].args.get() == nullptr && args.get() == nullptr)
        break;
      if ((flora_msgs[i].args.get() == nullptr && args.get()) ||
          (flora_msgs[i].args.get() && args.get() == nullptr) ||
          args->read(v) != CAPS_SUCCESS || v != i) {
        KLOGE(TAG, "received incorrect msg args, msgtype = %u", msgtype);
        break;
      }
      break;
    }
  }
}

void TestClient::recv_call(const char *name, shared_ptr<Caps> &args,
                           shared_ptr<Reply> &reply) {
  int32_t i;
  int32_t v;

  for (i = 0; i < FLORA_MSG_COUNT; ++i) {
    if (strcmp(name, flora_msgs[i].name) == 0) {
      ++recv_call_counter[i];
      if (flora_msgs[i].args.get() == nullptr && args.get() == nullptr)
        break;
      if ((flora_msgs[i].args.get() == nullptr && args.get()) ||
          (flora_msgs[i].args.get() && args.get() == nullptr) ||
          args->read(v) != CAPS_SUCCESS || v != i) {
        KLOGE(TAG, "received incorrect method args");
        break;
      }
      break;
    }
  }
  reply->write_data(args);
  reply->end();
}

void TestClient::c_recv_post(const char *name, uint32_t msgtype, caps_t args) {
  int32_t i;
  int32_t v;

  for (i = 0; i < FLORA_MSG_COUNT; ++i) {
    if (strcmp(name, flora_msgs[i].name) == 0) {
      ++recv_post_counter[i];
      if (flora_msgs[i].c_args == 0 && args == 0)
        break;
      if ((flora_msgs[i].c_args == 0 && args != 0) ||
          (flora_msgs[i].c_args != 0 && args == 0) ||
          caps_read_integer(args, &v) != CAPS_SUCCESS || v != i) {
        KLOGE(TAG, "received incorrect msg args, msgtype = %u", msgtype);
        break;
      }
      break;
    }
  }
}

void TestClient::c_recv_call(const char *name, caps_t args,
                             flora_call_reply_t reply) {
  int32_t i;
  int32_t v;

  for (i = 0; i < FLORA_MSG_COUNT; ++i) {
    if (strcmp(name, flora_msgs[i].name) == 0) {
      ++recv_call_counter[i];
      if (flora_msgs[i].c_args == 0 && args == 0)
        break;
      if ((flora_msgs[i].c_args == 0 && args != 0) ||
          (flora_msgs[i].c_args != 0 && args == 0) ||
          caps_read_integer(args, &v) != CAPS_SUCCESS || v != i) {
        KLOGE(TAG, "received incorrect method args");
        break;
      }
      break;
    }
  }
  caps_t data = caps_create();
  caps_write_integer(data, v);
  flora_call_reply_write_data(reply, data);
  flora_call_reply_end(reply);
}
