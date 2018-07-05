#include <sys/mman.h>
#include <string.h>
#include <chrono>
#include "cli.h"
#include "uri.h"
#include "rlog.h"
#include "tcp-conn.h"
#include "reqser.h"
#include "defs.h"

using namespace std;
using rokid::Uri;

#define TAG "flora.Client"

Client::Client(uint32_t bufsize) : buf_size(bufsize) {
	sbuffer = (int8_t*)mmap(NULL, bufsize * 2, PROT_READ | PROT_WRITE,
			MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
	rbuffer = sbuffer + buf_size;
}

Client::~Client() {
	munmap(sbuffer, buf_size * 2);
#ifdef FLORA_DEBUG
	KLOGI(TAG, "client %s: post %u times, post %u bytes, "
			"send %u times, send %u bytes",
			auth_extra.c_str(), post_times, post_bytes,
			send_times, send_bytes);
#endif
}

static bool check_callback(flora_cli_callback_t* cb) {
	if (cb == nullptr)
		return false;
	if (cb->disconnected == nullptr || cb->received == nullptr)
		return false;
	return true;
}

int32_t Client::connect(const char* uri, flora_cli_callback_t* cb, void* cbarg) {
	if (uri == nullptr || !check_callback(cb))
		return FLORA_CLI_EINVAL;

	Uri urip;
	if (!urip.parse(uri)) {
		KLOGE(TAG, "invalid uri %s", uri);
		return FLORA_CLI_EINVAL;
	}

	TCPConn* conn;
	if (urip.scheme == "tcp") {
		conn = new TCPConn();
		if (!conn->connect(urip.host, urip.port))
			return FLORA_CLI_ECONN;
	} else {
		KLOGE(TAG, "unsupported uri scheme %s", urip.scheme.c_str());
		return FLORA_CLI_EINVAL;
	}
	cli_mutex.lock();
	connection.reset(conn);
	cli_mutex.unlock();
	if (!auth(urip.fragment)) {
		close(false);
		return FLORA_CLI_EAUTH;
	}
	callback = cb;
	cb_arg = cbarg;
	auth_extra = urip.fragment;

	recv_thread = thread([this]() { this->recv_loop(); });
	return FLORA_CLI_SUCCESS;
}

bool Client::auth(const string& extra) {
	int32_t c = RequestSerializer::serialize_auth(FLORA_VERSION, extra.c_str(), sbuffer, buf_size);
	if (c <= 0)
		return false;
	if (!connection->send(sbuffer, c))
		return false;
#ifdef FLORA_DEBUG
	++send_times;
	send_bytes += c;
#endif
	c = connection->recv(rbuffer, buf_size);
	if (c <= 0)
		return false;
	caps_t resp;
	if (caps_parse(rbuffer, c, &resp) != CAPS_SUCCESS)
		return false;
	int32_t cmd;
	int32_t auth_res;
	if (caps_read_integer(resp, &cmd) != CAPS_SUCCESS) {
		caps_destroy(resp);
		return false;
	}
	if (cmd != CMD_AUTH_RESP) {
		caps_destroy(resp);
		return false;
	}
	if (caps_read_integer(resp, &auth_res) != CAPS_SUCCESS) {
		caps_destroy(resp);
		return false;
	}
	caps_destroy(resp);
	return auth_res == FLORA_CLI_SUCCESS;
}

void Client::recv_loop() {
	shared_ptr<Connection> conn;
	int32_t c;

	cli_mutex.lock();
	conn = connection;
	cli_mutex.unlock();
	if (conn.get() == nullptr) {
		KLOGE(TAG, "start recv loop, but connection is closed");
		return;
	}

	while (true) {
		c = conn->recv(rbuffer + rbuf_off, buf_size - rbuf_off);
		if (c <= 0)
			break;
		if (!handle_received(rbuf_off + c))
			break;
	}
	iclose(true);
}

bool Client::handle_received(int32_t size) {
	caps_t resp;
	uint32_t off = 0;
	uint32_t version;
	uint32_t length;
	int32_t cmd;

	while (off < size) {
		if (size - off < 8)
			break;
		if (caps_binary_info(rbuffer + off, &version, &length) != CAPS_SUCCESS)
			return false;
		if (size - off < length)
			break;
		if (caps_parse(rbuffer + off, length, &resp) != CAPS_SUCCESS)
			return false;
		off += length;

		if (caps_read_integer(resp, &cmd) != CAPS_SUCCESS) {
			caps_destroy(resp);
			return false;
		}
		switch (cmd) {
			case CMD_RECV_MSG: {
				const char* name;
				caps_t args = 0;
				if (caps_read_string(resp, &name) != CAPS_SUCCESS) {
					caps_destroy(resp);
					return false;
				}
				caps_read_object(resp, &args);
				callback->received(name, args, cb_arg);
			}
			break;
			default:
				KLOGE(TAG, "client received invalid command %d", cmd);
				caps_destroy(resp);
				return false;
		}
		caps_destroy(resp);
	}
	if (off > 0 && size - off > 0) {
		memmove(rbuffer, rbuffer + off, size - off);
	}
	rbuf_off = size - off;
	return true;
}

void Client::iclose(bool passive) {
	unique_lock<mutex> locker(cli_mutex);

	if (connection.get() == nullptr)
		return;
	connection->close();
	connection.reset();
	locker.unlock();

	if (passive) {
		callback->disconnected(cb_arg);
	}
}

void Client::close(bool passive) {
	iclose(passive);

	if (recv_thread.joinable()) {
		recv_thread.join();
	}
}

bool Client::subscribe(const char* name) {
	shared_ptr<Connection> conn;

	cli_mutex.lock();
	conn = connection;
	cli_mutex.unlock();

	if (conn.get() == nullptr)
		return false;
	int32_t c = RequestSerializer::serialize_subscribe(name, sbuffer, buf_size);
	if (c <= 0)
		return false;
#ifdef FLORA_DEBUG
	++send_times;
	send_bytes += c;
#endif
	return conn->send(sbuffer, c);
}

bool Client::unsubscribe(const char* name) {
	shared_ptr<Connection> conn;

	cli_mutex.lock();
	conn = connection;
	cli_mutex.unlock();

	if (conn.get() == nullptr)
		return false;
	int32_t c = RequestSerializer::serialize_unsubscribe(name, sbuffer, buf_size);
	if (c <= 0)
		return false;
#ifdef FLORA_DEBUG
	++send_times;
	send_bytes += c;
#endif
	return conn->send(sbuffer, c);
}

bool Client::post(const char* name, caps_t msg) {
	shared_ptr<Connection> conn;

	cli_mutex.lock();
	conn = connection;
	cli_mutex.unlock();

	if (conn.get() == nullptr)
		return false;
	int32_t c = RequestSerializer::serialize_post(name, msg, sbuffer, buf_size);
	if (c <= 0)
		return false;
#ifdef FLORA_DEBUG
	++post_times;
	post_bytes += c;
	++send_times;
	send_bytes += c;
#endif
	return conn->send(sbuffer, c);
}

int32_t flora_cli_connect(const char* uri, flora_cli_callback_t* cb,
		void* arg, uint32_t msg_buf_size, flora_cli_t* result) {
	if (result == nullptr)
		return FLORA_CLI_EINVAL;

	uint32_t bufsize = msg_buf_size > DEFAULT_MSG_BUF_SIZE
		? msg_buf_size : DEFAULT_MSG_BUF_SIZE;
	Client* cli = new Client(bufsize);
	int32_t r;

	r = cli->connect(uri, cb, arg);
	if (r != FLORA_CLI_SUCCESS) {
		delete cli;
		return r;
	}
	*result = reinterpret_cast<flora_cli_t>(cli);
	return FLORA_CLI_SUCCESS;
}

void flora_cli_delete(flora_cli_t handle) {
	if (handle) {
		Client* cli = reinterpret_cast<Client*>(handle);
		cli->close(false);
		delete cli;
	}
}

int32_t flora_cli_subscribe(flora_cli_t handle, const char* name) {
	if (handle == 0 || name == nullptr)
		return FLORA_CLI_EINVAL;
	if (!reinterpret_cast<Client*>(handle)->subscribe(name))
		return FLORA_CLI_ECONN;
	return FLORA_CLI_SUCCESS;
}

int32_t flora_cli_unsubscribe(flora_cli_t handle, const char* name) {
	if (handle == 0 || name == nullptr)
		return FLORA_CLI_EINVAL;
	if (!reinterpret_cast<Client*>(handle)->unsubscribe(name))
		return FLORA_CLI_ECONN;
	return FLORA_CLI_SUCCESS;
}

int32_t flora_cli_post(flora_cli_t handle, const char* name, caps_t msg) {
	if (handle == 0 || name == nullptr)
		return FLORA_CLI_EINVAL;
	if (!reinterpret_cast<Client*>(handle)->post(name, msg))
		return FLORA_CLI_ECONN;
	return FLORA_CLI_SUCCESS;
}
