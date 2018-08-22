#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "clargs.h"
#include "rlog.h"
#include "test-cli.h"
#include "test-svc.h"

#define TAG "unit-test.main"

#define TCP_SERVICE_URI "tcp://0.0.0.0:37777/"
#define TCP_CONN_URI "tcp://localhost:37777/"
#define UNIX_URI "unix:flora-unittest"

#define COMM_TYPE_UNIX 0
#define COMM_TYPE_TCP 1

static void print_prompt(const char* progname) {
	static const char* format =
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

static bool parse_cmdline(int argc, char** argv, CmdlineArgs* args) {
	clargs_h h = clargs_parse(argc, argv);
	if (h == 0) {
		print_prompt(argv[0]);
		return false;
	}
	if (clargs_opt_has(h, "help")) {
		print_prompt(argv[0]);
		clargs_destroy(h);
		return false;
	}
	const char* v = clargs_opt_get(h, "client-num");
	char* ep;
	if (v == nullptr) {
		args->client_num = 2;
	} else {
		args->client_num = strtol(v, &ep, 10);
		if (ep[0] != '\0' || args->client_num <= 1) {
			KLOGE(TAG, "invalid --client-num=%s", v);
			clargs_destroy(h);
			return false;
		}
	}
	v = clargs_opt_get(h, "repeat");
	if (v == nullptr) {
		args->repeat = 1;
	} else {
		args->repeat = strtol(v, &ep, 10);
		if (ep[0] != '\0' || args->repeat <= 0) {
			KLOGE(TAG, "invalid --repeat=%s", v);
			clargs_destroy(h);
			return false;
		}
	}
	v = clargs_opt_get(h, "comm-type");
	if (v == nullptr) {
		args->comm_type = COMM_TYPE_UNIX;
	} else {
		if (strcmp(v, "tcp") == 0)
			args->comm_type = COMM_TYPE_TCP;
		else
			args->comm_type = COMM_TYPE_UNIX;
	}
	if (clargs_opt_has(h, "use-c-api"))
		args->use_c_api = 1;
	else
		args->use_c_api = 0;
	clargs_destroy(h);
	return true;
}

static bool check_results(TestClient* clients, int32_t num) {
	int32_t total_post_counter[FLORA_MSG_COUNT];
	int32_t i, j;

	memset(total_post_counter, 0, sizeof(total_post_counter));
	for (i = 0; i < num; ++i) {
		for (j = 0; j < FLORA_MSG_COUNT; ++j) {
			total_post_counter[j] += clients[i].post_counter[j];
		}
	}

	int32_t post_counter[FLORA_MSG_COUNT];
	for (i = 0; i < num; ++i) {
		memcpy(post_counter, total_post_counter, sizeof(post_counter));
		for (j = 0; j < FLORA_MSG_COUNT; ++j) {
			post_counter[j] -= clients[i].post_counter[j];
		}

		for (j = 0; j < FLORA_MSG_COUNT; ++j) {
			if (clients[i].subscribe_flags[j]) {
				if (clients[i].recv_instant_counter[j] != post_counter[j]) {
					KLOGE(TAG, "client %d, msg(instant) %d recv/post not equal %d/%d",
							i, j, clients[i].recv_instant_counter[j], post_counter[j]);
					return false;
				}
				if (clients[i].recv_persist_counter[j] != post_counter[j]) {
					KLOGE(TAG, "client %d, msg(persist) %d recv/post not equal %d/%d",
							i, j, clients[i].recv_persist_counter[j], post_counter[j]);
					return false;
				}
				if (clients[i].recv_request_counter[j] != post_counter[j]) {
					KLOGE(TAG, "client %d, msg(request) %d recv/post not equal %d/%d",
							i, j, clients[i].recv_request_counter[j], post_counter[j]);
					return false;
				}
			}
			if (clients[i].subscribe_flags[j] == 0) {
				if (clients[i].recv_instant_counter[j] != 0) {
					KLOGE(TAG, "client %d, msg(instant) %d should not received, but recv %d times",
							i, j, clients[i].recv_instant_counter[j]);
					return false;
				}
				if (clients[i].recv_persist_counter[j] != 0) {
					KLOGE(TAG, "client %d, msg(persist) %d should not received, but recv %d times",
							i, j, clients[i].recv_persist_counter[j]);
					return false;
				}
				if (clients[i].recv_request_counter[j] != 0) {
					KLOGE(TAG, "client %d, msg(request) %d should not received, but recv %d times",
							i, j, clients[i].recv_request_counter[j]);
					return false;
				}
			}
		}
	}
	KLOGI(TAG, "test success");
	return true;
}

int main(int argc, char** argv) {
	CmdlineArgs args;
	if (!parse_cmdline(argc, argv, &args))
		return 0;

	srand(time(nullptr));
	TestClient::static_init(args.use_c_api);

	TestService tsvc;
	const char* uri;
	if (args.comm_type == COMM_TYPE_TCP)
		uri = TCP_SERVICE_URI;
	else
		uri = UNIX_URI;
	if (!tsvc.run(uri, args.use_c_api)) {
		KLOGE(TAG, "service startup failed");
		return 1;
	}

	TestClient* clients = new TestClient[args.client_num];
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
