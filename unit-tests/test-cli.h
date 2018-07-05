#pragma once

#include "caps.h"
#include "flora-cli.h"

#define FLORA_MSG_COUNT 32

typedef struct {
	char name[8];
	caps_t args;
} FloraMsg;

class TestClient {
public:
	bool init(const char* uri);

	void do_subscribe();

	void do_post();

	void reset();

	void close();

private:
	void received(const char* name, caps_t args);

public:
	static void static_init();

	static FloraMsg flora_msgs[FLORA_MSG_COUNT];

	static flora_cli_callback_t flora_callback;

private:
	static void msg_received(const char* name, caps_t msg, void* arg);

public:
	int8_t subscribe_flags[FLORA_MSG_COUNT];
	int32_t post_counter[FLORA_MSG_COUNT];
	int32_t recv_counter[FLORA_MSG_COUNT];

private:
	flora_cli_t flora_cli = 0;
};
