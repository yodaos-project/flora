#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "test-cli.h"
#include "rlog.h"

#define MAX_POST_COUNT 256
#define TAG "unit-test.TestClient"

FloraMsg TestClient::flora_msgs[FLORA_MSG_COUNT];
flora_cli_callback_t TestClient::flora_callback;

void TestClient::recv_post_s(const char* name, uint32_t msgtype, caps_t msg, void* arg) {
	TestClient* tc = reinterpret_cast<TestClient*>(arg);
	tc->c_recv_post(name, msgtype, msg);
}

int32_t TestClient::recv_get_s(const char* name, caps_t msg, void* arg, caps_t* reply) {
	TestClient* tc = reinterpret_cast<TestClient*>(arg);
	return tc->c_recv_get(name, msg, reply);
}

static void conn_disconnected(void* arg) {
}

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
		flora_callback.recv_get = TestClient::recv_get_s;
	}
}

bool TestClient::init(const char* uri, bool capi) {
	use_c_api = capi;
	if (capi)
		return flora_cli_connect(uri, &flora_callback, this, 0,
				&c_flora_cli) == FLORA_CLI_SUCCESS;
	return Client::connect(uri, this, 0, flora_cli) == FLORA_CLI_SUCCESS;
}

void TestClient::do_subscribe() {
	int32_t c = rand() % FLORA_MSG_COUNT;
	int32_t i;
	int32_t idx;
	int32_t r;

	memset(recv_instant_counter, 0, sizeof(recv_instant_counter));
	memset(recv_persist_counter, 0, sizeof(recv_persist_counter));
	memset(recv_request_counter, 0, sizeof(recv_request_counter));
	memset(subscribe_flags, 0, sizeof(subscribe_flags));
	for (i = 0; i < c; ++i) {
		idx = rand() % FLORA_MSG_COUNT;
		if (subscribe_flags[idx] == 0) {
			if (use_c_api) {
				r = flora_cli_subscribe(c_flora_cli, flora_msgs[idx].name);
			} else {
				r = flora_cli->subscribe(flora_msgs[idx].name);
			}

			if (r != FLORA_CLI_SUCCESS)
				KLOGE(TAG, "client subscribe failed");
			else
				subscribe_flags[idx] = 1;
		}
	}
}

void TestClient::do_post() {
	int32_t c = rand() % MAX_POST_COUNT;
	int32_t i;
	int32_t idx;
	vector<Response> results;
	flora_get_result* c_results;
	uint32_t res_count;
	int32_t r[3];

	memset(post_counter, 0, sizeof(post_counter));
	for (i = 0; i < c; ++i) {
		idx = rand() % FLORA_MSG_COUNT;
		if (use_c_api) {
			r[0] = flora_cli_post(c_flora_cli, flora_msgs[idx].name,
					flora_msgs[idx].c_args, FLORA_MSGTYPE_INSTANT);
			r[1] = flora_cli_post(c_flora_cli, flora_msgs[idx].name,
					flora_msgs[idx].c_args, FLORA_MSGTYPE_PERSIST);
			r[2] = flora_cli_get(c_flora_cli, flora_msgs[idx].name,
					flora_msgs[idx].c_args, &c_results, &res_count, 0);
		} else {
			r[0] = flora_cli->post(flora_msgs[idx].name, flora_msgs[idx].args,
					FLORA_MSGTYPE_INSTANT);
			r[1] = flora_cli->post(flora_msgs[idx].name, flora_msgs[idx].args,
					FLORA_MSGTYPE_PERSIST);
			r[2] = flora_cli->get(flora_msgs[idx].name, flora_msgs[idx].args,
					results, 0);
		}
		if (r[0] != FLORA_CLI_SUCCESS)
			KLOGE(TAG, "client instant post failed");
		else
			++post_counter[idx];
		if (r[1] != FLORA_CLI_SUCCESS)
			KLOGE(TAG, "client persist post failed");
		if (r[2] != FLORA_CLI_SUCCESS)
			KLOGE(TAG, "client request failed: %d", r[2]);
		else {
			if (use_c_api) {
				KLOGI(TAG, "client request result count = %u", res_count);
				flora_result_delete(c_results, res_count);
			} else
				KLOGI(TAG, "client request result count = %u", results.size());
		}
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

void TestClient::recv_post(const char* name, uint32_t msgtype,
		shared_ptr<Caps>& args) {
	int32_t i;
	int32_t v;

	for (i = 0; i < FLORA_MSG_COUNT; ++i) {
		if (strcmp(name, flora_msgs[i].name) == 0) {
			if (msgtype == FLORA_MSGTYPE_INSTANT)
				++recv_instant_counter[i];
			else if (msgtype == FLORA_MSGTYPE_PERSIST)
				++recv_persist_counter[i];
			else
				KLOGE(TAG, "client recv invalid msgtype %u", msgtype);
			if (flora_msgs[i].args.get() == nullptr && args.get() == nullptr)
				break;
			if ((flora_msgs[i].args.get() == nullptr && args.get())
					|| (flora_msgs[i].args.get() && args.get() == nullptr)
					|| args->read(v) != CAPS_SUCCESS
					|| v != i) {
				KLOGE(TAG, "received incorrect msg args, msgtype = %u", msgtype);
				break;
			}
			break;
		}
	}
}

int32_t TestClient::recv_get(const char* name, shared_ptr<Caps>& args,
		shared_ptr<Caps>& reply) {
	int32_t i;
	int32_t v;

	for (i = 0; i < FLORA_MSG_COUNT; ++i) {
		if (strcmp(name, flora_msgs[i].name) == 0) {
			++recv_request_counter[i];
			if (flora_msgs[i].args.get() == nullptr && args.get() == nullptr)
				break;
			if ((flora_msgs[i].args.get() == nullptr && args.get())
					|| (flora_msgs[i].args.get() && args.get() == nullptr)
					|| args->read(v) != CAPS_SUCCESS
					|| v != i) {
				KLOGE(TAG, "received incorrect msg args, msgtype = request");
				break;
			}
			break;
		}
	}
	reply = args;
	return 0;
}

void TestClient::c_recv_post(const char* name, uint32_t msgtype, caps_t args) {
	int32_t i;
	int32_t v;

	for (i = 0; i < FLORA_MSG_COUNT; ++i) {
		if (strcmp(name, flora_msgs[i].name) == 0) {
			if (msgtype == FLORA_MSGTYPE_INSTANT)
				++recv_instant_counter[i];
			else if (msgtype == FLORA_MSGTYPE_PERSIST)
				++recv_persist_counter[i];
			else
				KLOGE(TAG, "client recv invalid msgtype %u", msgtype);
			if (flora_msgs[i].c_args == 0 && args == 0)
				break;
			if ((flora_msgs[i].c_args == 0 && args != 0)
					|| (flora_msgs[i].c_args != 0 && args == 0)
					|| caps_read_integer(args, &v) != CAPS_SUCCESS
					|| v != i) {
				KLOGE(TAG, "received incorrect msg args, msgtype = %u", msgtype);
				break;
			}
			break;
		}
	}
}

int32_t TestClient::c_recv_get(const char* name, caps_t args, caps_t* reply) {
	int32_t i;
	int32_t v;

	for (i = 0; i < FLORA_MSG_COUNT; ++i) {
		if (strcmp(name, flora_msgs[i].name) == 0) {
			++recv_request_counter[i];
			if (flora_msgs[i].c_args == 0 && args == 0)
				break;
			if ((flora_msgs[i].c_args == 0 && args != 0)
					|| (flora_msgs[i].c_args != 0 && args == 0)
					|| caps_read_integer(args, &v) != CAPS_SUCCESS
					|| v != i) {
				KLOGE(TAG, "received incorrect msg args, msgtype = request");
				break;
			}
			break;
		}
	}
	*reply = caps_create();
	caps_write_integer(*reply, v);
	return 0;
}
