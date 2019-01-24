#include "clargs.h"
#include "rlog.h"
#include "test-cli.h"
#include "test-svc.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#define TAG "unit-test.main"

#define TCP_SERVICE_URI "tcp://0.0.0.0:37777/"
#define TCP_CONN_URI "tcp://localhost:37777/"
#define UNIX_URI "unix:flora-unittest"

#define COMM_TYPE_UNIX 0
#define COMM_TYPE_TCP 1

static void print_prompt(const char *progname) {
  static const char *format =
      "USAGE: %s [options]\n"
      "options:\n"
      "--client-num=$TEST_CLIENT_NUM  (default value 2)\n"
      "--repeat=$TEST_REPEAT_TIMES  (default value 1)\n"
      "--comm-type=$TYPE  (unix|tcp)  (default value unix)\n"
      "--use-c-api\n";
  KLOGE(TAG, format, progname);
}

typedef struct {
  int32_t client_num;
  int32_t repeat;
  int32_t comm_type;
  int32_t use_c_api;
} CmdlineArgs;

static bool parse_cmdline(int argc, char **argv, CmdlineArgs *args) {
  shared_ptr<CLArgs> h = CLArgs::parse(argc, argv);
  if (h == nullptr) {
    print_prompt(argv[0]);
    return false;
  }
  if (h->find("help", nullptr, nullptr)) {
    print_prompt(argv[0]);
    return false;
  }
  uint32_t begin, end;
  CLPair pair;
  if (h->find("client-num", &begin, &end)) {
    h->at(begin, pair);
    if (pair.value == nullptr)
      args->client_num = 2;
    else if (!pair.to_integer(args->client_num)) {
      KLOGE(TAG, "invalid --client-num=%s", pair.value);
      return false;
    }
  }
  if (h->find("repeat", &begin, &end)) {
    h->at(begin, pair);
    if (pair.value == nullptr)
      args->repeat = 1;
    else if (!pair.to_integer(args->repeat)) {
      KLOGE(TAG, "invalid --repeat=%s", pair.value);
      return false;
    }
  }
  if (h->find("comm-type", &begin, &end)) {
    h->at(begin, pair);
    if (pair.value == nullptr)
      args->comm_type = COMM_TYPE_UNIX;
    else {
      if (strcmp(pair.value, "tcp") == 0)
        args->comm_type = COMM_TYPE_TCP;
      else
        args->comm_type = COMM_TYPE_UNIX;
    }
  }
  if (h->find("use-c-api", nullptr, nullptr))
    args->use_c_api = 1;
  else
    args->use_c_api = 0;
  return true;
}

static bool check_results(TestClient *clients, int32_t num) {
  int32_t total_post_counter[FLORA_MSG_COUNT];
  int32_t total_call_counter[FLORA_MSG_COUNT];
  int32_t total_recv_call_counter[FLORA_MSG_COUNT];
  int32_t i, j;

  memset(total_post_counter, 0, sizeof(total_post_counter));
  for (i = 0; i < num; ++i) {
    for (j = 0; j < FLORA_MSG_COUNT; ++j) {
      total_post_counter[j] += clients[i].post_counter[j];
    }
  }
  memset(total_call_counter, 0, sizeof(total_call_counter));
  memset(total_recv_call_counter, 0, sizeof(total_recv_call_counter));

  for (i = 0; i < num; ++i) {
    for (j = 0; j < FLORA_MSG_COUNT; ++j) {
      if (clients[i].subscribe_flags[j]) {
        if (clients[i].recv_post_counter[j] != total_post_counter[j]) {
          KLOGE(TAG, "client %d, msg %d recv/post not equal %d/%d", i, j,
                clients[i].recv_post_counter[j], total_post_counter[j]);
          return false;
        }
      }
      if (clients[i].subscribe_flags[j] == 0) {
        if (clients[i].recv_post_counter[j] != 0) {
          KLOGE(TAG,
                "client %d, msg %d should not received, but recv %d "
                "times",
                i, j, clients[i].recv_post_counter[j]);
          return false;
        }
      }
      total_call_counter[j] = clients[i].call_counter[j];
      total_recv_call_counter[j] = clients[i].recv_call_counter[j];
    }
  }
  for (i = 0; i < FLORA_MSG_COUNT; ++i) {
    if (total_call_counter[i] != total_recv_call_counter[i]) {
      KLOGE(TAG, "msg %d recv/call not equal %d/%d", i,
            total_recv_call_counter[i], total_call_counter[i]);
      return false;
    }
  }
  KLOGI(TAG, "test success");
  return true;
}

int main(int argc, char **argv) {
  CmdlineArgs args;
  if (!parse_cmdline(argc, argv, &args))
    return 0;

  srand(time(nullptr));
  TestClient::static_init(args.use_c_api);

  TestService tsvc;
  const char *uri;
  if (args.comm_type == COMM_TYPE_TCP)
    uri = TCP_SERVICE_URI;
  else
    uri = UNIX_URI;
  if (!tsvc.run(uri, args.use_c_api)) {
    KLOGE(TAG, "service startup failed");
    return 1;
  }

  TestClient *clients = new TestClient[args.client_num];
  int32_t i;

  char cli_uri[64];
  if (args.comm_type == COMM_TYPE_TCP)
    uri = TCP_CONN_URI;
  else
    uri = UNIX_URI;
  for (i = 0; i < args.client_num; ++i) {
    snprintf(cli_uri, sizeof(cli_uri), "%s#%03d", uri, i);
    if (!clients[i].init(cli_uri, args.use_c_api)) {
      KLOGE(TAG, "client %d init failed", i);
      tsvc.close();
      return 1;
    }
  }

  while (args.repeat--) {
    for (i = 0; i < args.client_num; ++i) {
      clients[i].do_subscribe();
    }
    sleep(2);
    for (i = 0; i < args.client_num; ++i) {
      clients[i].do_post();
    }
    sleep(4);
    if (!check_results(clients, args.client_num)) {
      break;
    }
    for (i = 0; i < args.client_num; ++i) {
      clients[i].reset();
    }
  }

  for (i = 0; i < args.client_num; ++i) {
    clients[i].close();
  }
  delete[] clients;
  tsvc.close();
  return 0;
}
