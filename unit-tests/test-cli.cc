#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "test-cli.h"
#include "rlog.h"

#define MAX_POST_COUNT 256
#define TAG "unit-test.TestClient"

FloraMsg TestClient::flora_msgs[FLORA_MSG_COUNT];
flora_cli_callback_t TestClient::flora_callback;

void TestClient::msg_received(const char* name, caps_t msg, void* arg) {
	TestClient* tc = reinterpret_cast<TestClient*>(arg);
	tc->received(name, msg);
}

static void conn_disconnected(void* arg) {
}

void TestClient::static_init() {
	int32_t i;
	for (i = 0; i < FLORA_MSG_COUNT; ++i) {
		snprintf(flora_msgs[i].name, sizeof(flora_msgs[i].name), "foo%02d", i);
		if (i % 2 == 0) {
			flora_msgs[i].args = caps_create();
			caps_write_integer(flora_msgs[i].args, i);
		} else {
			flora_msgs[i].args = 0;
		}
	}

	flora_callback.received = TestClient::msg_received;
	flora_callback.disconnected = conn_disconnected;
}

bool TestClient::init(const char* uri) {
	return flora_cli_connect(uri, &flora_callback, this, 0, &flora_cli) == FLORA_CLI_SUCCESS;
}

void TestClient::do_subscribe() {
	int32_t c = rand() % FLORA_MSG_COUNT;
	int32_t i;
	int32_t idx;

	memset(recv_counter, 0, sizeof(recv_counter));
	memset(subscribe_flags, 0, sizeof(subscribe_flags));
	for (i = 0; i < c; ++i) {
		idx = rand() % FLORA_MSG_COUNT;
		if (subscribe_flags[idx] == 0) {
			if (flora_cli_subscribe(flora_cli, flora_msgs[idx].name) == FLORA_CLI_SUCCESS) {
				subscribe_flags[idx] = 1;
			} else {
				KLOGE(TAG, "client subscribe failed");
			}
		}
	}
}

void TestClient::do_post() {
	int32_t c = rand() % MAX_POST_COUNT;
	int32_t i;
	int32_t idx;

	memset(post_counter, 0, sizeof(post_counter));
	for (i = 0; i < c; ++i) {
		idx = rand() % FLORA_MSG_COUNT;
		if (flora_cli_post(flora_cli, flora_msgs[idx].name, flora_msgs[idx].args) == FLORA_CLI_SUCCESS) {
			++post_counter[idx];
		} else {
			KLOGE(TAG, "client post failed");
		}
	}
}

void TestClient::reset() {
	int32_t i;
	for (i = 0; i < FLORA_MSG_COUNT; ++i) {
		if (flora_cli_unsubscribe(flora_cli, flora_msgs[i].name) != FLORA_CLI_SUCCESS) {
			KLOGE(TAG, "client unsubscribe failed");
		}
	}
	memset(subscribe_flags, 0, sizeof(subscribe_flags));
	memset(post_counter, 0, sizeof(post_counter));
}

void TestClient::close() {
	flora_cli_delete(flora_cli);
	flora_cli = 0;
}

void TestClient::received(const char* name, caps_t args) {
	int32_t i;
	int32_t v;

	for (i = 0; i < FLORA_MSG_COUNT; ++i) {
		if (strcmp(name, flora_msgs[i].name) == 0) {
			if (flora_msgs[i].args == 0 && args == 0) {
				++recv_counter[i];
				break;
			}
			if ((flora_msgs[i].args == 0 && args != 0)
					|| (flora_msgs[i].args != 0 && args == 0)
					|| caps_read_integer(args, &v) != CAPS_SUCCESS
					|| v != i) {
				KLOGE(TAG, "received incorrect msg args");
				break;
			}
			++recv_counter[i];
			break;
		}
	}
}
