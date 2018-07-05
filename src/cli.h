#pragma once

#include <thread>
#include <mutex>
#include <memory>
#include <string>
#include "flora-cli.h"
#include "conn.h"
#include "caps.h"

class Client {
public:
	Client(uint32_t bufsize);

	~Client();

	int32_t connect(const char* uri, flora_cli_callback_t* cb, void* cbarg);

	void close(bool passive);

	bool subscribe(const char* name);

	bool unsubscribe(const char* name);

	bool post(const char* name, caps_t msg);

private:
	bool auth(const std::string& extra);

	void recv_loop();

	bool handle_received(int32_t size);

	void iclose(bool passive);

private:
	uint32_t buf_size;
	int8_t* sbuffer = nullptr;
	int8_t* rbuffer = nullptr;
	uint32_t rbuf_off = 0;
	flora_cli_callback_t* callback = nullptr;
	void* cb_arg = nullptr;
	std::shared_ptr<Connection> connection;
	std::thread recv_thread;
	std::mutex cli_mutex;

public:
	std::string auth_extra;
#ifdef FLORA_DEBUG
	uint32_t post_times = 0;
	uint32_t post_bytes = 0;
	uint32_t send_times = 0;
	uint32_t send_bytes = 0;
#endif
};
