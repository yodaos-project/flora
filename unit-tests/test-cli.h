#pragma once

#include "caps.h"
#include "flora-cli.h"

#define FLORA_MSG_COUNT 32

using namespace std;
using namespace flora;

typedef struct {
  char name[8];
  std::shared_ptr<Caps> args;
  caps_t c_args;
} FloraMsg;

class TestClient : public ClientCallback {
public:
  bool init(const char *uri, bool capi);

  void do_subscribe();

  void do_post();

  void do_call(int32_t clinum);

  void reset();

  void close();

  void recv_post(const char *name, uint32_t msgtype, shared_ptr<Caps> &msg);

  void recv_call(const char *name, shared_ptr<Caps> &msg,
                 shared_ptr<Reply> &reply);

private:
  void c_recv_post(const char *name, uint32_t msgtype, caps_t args);

  void c_recv_call(const char *name, caps_t args, flora_call_reply_t reply);

public:
  static void static_init(bool capi);

  static FloraMsg flora_msgs[FLORA_MSG_COUNT];

  static flora_cli_callback_t flora_callback;

private:
  static void recv_post_s(const char *name, uint32_t msgtype, caps_t msg,
                          void *arg);

  static void recv_call_s(const char *name, caps_t msg, void *arg,
                          flora_call_reply_t reply);

public:
  int8_t subscribe_flags[FLORA_MSG_COUNT];
  int8_t declare_method_flags[FLORA_MSG_COUNT];
  int32_t post_counter[FLORA_MSG_COUNT];
  int32_t call_counter[FLORA_MSG_COUNT];
  int32_t recv_post_counter[FLORA_MSG_COUNT];
  int32_t recv_call_counter[FLORA_MSG_COUNT];

private:
  shared_ptr<Client> flora_cli;
  flora_cli_t c_flora_cli = 0;
  bool use_c_api = false;
};
