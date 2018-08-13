#include <sys/mman.h>
#include <string.h>
#include <assert.h>
#include "cli.h"
#include "uri.h"
#include "rlog.h"
#include "tcp-conn.h"
#include "ser-helper.h"
#include "defs.h"

using namespace std;
using namespace std::chrono;
using rokid::Uri;

#define TAG "flora.Client"

int32_t flora::Client::connect(const char* uri, flora::ClientCallback* cb,
		uint32_t msg_buf_size, shared_ptr<flora::Client>& result) {
	uint32_t bufsize = msg_buf_size > DEFAULT_MSG_BUF_SIZE ?
		msg_buf_size : DEFAULT_MSG_BUF_SIZE;
	shared_ptr<flora::internal::Client> cli = make_shared<flora::internal::Client>(bufsize);
	int32_t r = cli->connect(uri, cb);
	if (r != FLORA_CLI_SUCCESS)
		return r;
	result = static_pointer_cast<flora::Client>(cli);
	return r;
}

namespace flora {
namespace internal {

Client::Client(uint32_t bufsize) : buf_size(bufsize) {
	sbuffer = (int8_t*)mmap(NULL, buf_size * 2, PROT_READ | PROT_WRITE,
			MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
	rbuffer = sbuffer + buf_size;
}

Client::~Client() noexcept {
	close(false);

	munmap(sbuffer, buf_size * 2);
#ifdef FLORA_DEBUG
	KLOGI(TAG, "client %s: post %u times, post %u bytes, "
			"send %u times, send %u bytes",
			auth_extra.c_str(), post_times, post_bytes,
			send_times, send_bytes);
#endif
}

int32_t Client::connect(const char* uri, ClientCallback* cb) {
	if (uri == nullptr)
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
	connection.reset(conn);
	if (!auth(urip.fragment)) {
		close(false);
		return FLORA_CLI_EAUTH;
	}
	callback = cb;
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
	int32_t auth_res;
	if (ResponseParser::parse_auth(rbuffer, c, auth_res) != 0)
		return false;
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
	shared_ptr<Caps> resp;
	uint32_t off = 0;
	uint32_t version;
	uint32_t length;
	int32_t cmd;

	while (off < size) {
		if (size - off < 8)
			break;
		if (Caps::binary_info(rbuffer + off, &version, &length) != CAPS_SUCCESS)
			return false;
		if (size - off < length)
			break;
		if (Caps::parse(rbuffer + off, length, resp, false) != CAPS_SUCCESS)
			return false;
		off += length;

		if (resp->read(cmd) != CAPS_SUCCESS) {
			return false;
		}
		switch (cmd) {
			case CMD_POST_RESP: {
				uint32_t msgtype;
				string name;
				int32_t msgid;
				shared_ptr<Caps> args;

				if (ResponseParser::parse_post(resp, name, msgtype, args, msgid) != 0) {
					return false;
				}
				if (callback) {
					if (msgtype == FLORA_MSGTYPE_REQUEST) {
						shared_ptr<Caps> reply_caps;
						int32_t cli_ret = callback->recv_get(name.c_str(), args,
								reply_caps);
						int32_t c = RequestSerializer::serialize_reply(
								name.c_str(), reply_caps, msgid, cli_ret, sbuffer,
								buf_size);
						if (!connection->send(sbuffer, c))
							return false;
					} else {
						callback->recv_post(name.c_str(), msgtype, args);
					}
				}
				break;
			}
			case CMD_REPLY_RESP: {
				string name;
				int32_t msgid;
				PendingRequestList::iterator it;

				req_mutex.lock();
				if (resp->read(msgid) != CAPS_SUCCESS) {
					req_mutex.unlock();
					return false;
				}

				for (it = pending_requests.begin(); it != pending_requests.end(); ++it) {
					if ((*it).id == msgid) {
						(*it).id = -1;
						if (ResponseParser::parse_reply(resp, name, *(*it).results) != 0) {
							req_mutex.unlock();
							return false;
						}
						req_reply_cond.notify_all();
						break;
					}
				}
				req_mutex.unlock();
				break;
			}
			default:
				KLOGE(TAG, "client received invalid command %d", cmd);
				return false;
		}
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
	req_reply_cond.notify_one();
	locker.unlock();

	if (passive && callback) {
		callback->disconnected();
	}
}

void Client::close(bool passive) {
	iclose(passive);

	if (recv_thread.joinable()) {
		recv_thread.join();
	}
}

int32_t Client::subscribe(const char* name, uint32_t msgtype) {
	if (name == nullptr || !is_valid_msgtype(msgtype))
		return FLORA_CLI_EINVAL;
	int32_t c = RequestSerializer::serialize_subscribe(name, msgtype,
			sbuffer, buf_size);
	if (c <= 0)
		return FLORA_CLI_EINVAL;
	cli_mutex.lock();
	if (connection.get() == nullptr || !connection->send(sbuffer, c)) {
		cli_mutex.unlock();
		return FLORA_CLI_ECONN;
	}
	cli_mutex.unlock();
#ifdef FLORA_DEBUG
	++send_times;
	send_bytes += c;
#endif
	return FLORA_CLI_SUCCESS;
}

int32_t Client::unsubscribe(const char* name, uint32_t msgtype) {
	if (name == nullptr || !is_valid_msgtype(msgtype))
		return FLORA_CLI_EINVAL;
	int32_t c = RequestSerializer::serialize_unsubscribe(name, msgtype,
			sbuffer, buf_size);
	if (c <= 0)
		return FLORA_CLI_EINVAL;
	cli_mutex.lock();
	if (connection.get() == nullptr || !connection->send(sbuffer, c)) {
		cli_mutex.unlock();
		return FLORA_CLI_ECONN;
	}
	cli_mutex.unlock();
#ifdef FLORA_DEBUG
	++send_times;
	send_bytes += c;
#endif
	return FLORA_CLI_SUCCESS;
}

int32_t Client::post(const char* name, shared_ptr<Caps>& msg,
		uint32_t msgtype) {
	if (name == nullptr || !is_valid_msgtype(msgtype)
			|| msgtype == FLORA_MSGTYPE_REQUEST)
		return FLORA_CLI_EINVAL;
	int32_t c = RequestSerializer::serialize_post(name, msgtype, msg,
			0, 0, sbuffer, buf_size);
	if (c <= 0)
		return FLORA_CLI_EINVAL;
	cli_mutex.lock();
	if (connection.get() == nullptr || !connection->send(sbuffer, c)) {
		cli_mutex.unlock();
		return FLORA_CLI_ECONN;
	}
	cli_mutex.unlock();
#ifdef FLORA_DEBUG
	++post_times;
	post_bytes += c;
	++send_times;
	send_bytes += c;
#endif
	return FLORA_CLI_SUCCESS;
}

int32_t Client::get(const char* name, shared_ptr<Caps>& msg,
		vector<Reply>& replys, uint32_t timeout) {
	if (name == nullptr)
		return FLORA_CLI_EINVAL;
	int32_t c = RequestSerializer::serialize_post(name, FLORA_MSGTYPE_REQUEST,
			msg, ++reqseq, timeout, sbuffer, buf_size);
	if (c <= 0)
		return FLORA_CLI_EINVAL;
	replys.clear();

	unique_lock<mutex> locker(req_mutex);
	PendingRequestList::iterator it = pending_requests.emplace(pending_requests.end());
	(*it).id = reqseq;
	(*it).results = &replys;
	if (timeout == 0)
		(*it).timeout = steady_clock::time_point::max();
	else
		(*it).timeout = steady_clock::now() + milliseconds(timeout);
	cli_mutex.lock();
	if (connection.get() == nullptr || !connection->send(sbuffer, c)) {
		cli_mutex.unlock();
		return FLORA_CLI_ECONN;
	}
	cli_mutex.unlock();
#ifdef FLORA_DEBUG
	++req_times;
	req_bytes += c;
	++send_times;
	send_bytes += c;
#endif
	while (true) {
		if ((*it).timeout == steady_clock::time_point::max())
			req_reply_cond.wait(locker);
		else
			req_reply_cond.wait_until(locker, (*it).timeout);

		cli_mutex.lock();
		if (connection.get() == nullptr) {
			cli_mutex.unlock();
			return FLORA_CLI_ECONN;
		}
		cli_mutex.unlock();

		if ((*it).id < 0) {
			// received reply
			if (replys.empty())
				return FLORA_CLI_ENEXISTS;
			break;
		} else {
			// not receive reply
			if ((*it).timeout <= steady_clock::now())
				return FLORA_CLI_ETIMEOUT;
		}
	}
	return FLORA_CLI_SUCCESS;
}

} // namespace internal
} // namespace flora

using flora::internal::Client;
using flora::Reply;
class WrapClientCallback : public flora::ClientCallback {
public:
	void recv_post(const char* name, uint32_t msgtype,
			shared_ptr<Caps>& msg);

	int32_t recv_get(const char* name, shared_ptr<Caps>& msg,
			shared_ptr<Caps>& reply);

	void disconnected();

	flora_cli_callback_t cb_funcs;
	void* arg;
};

void WrapClientCallback::recv_post(const char* name, uint32_t msgtype,
		shared_ptr<Caps>& msg) {
	if (cb_funcs.recv_post) {
		caps_t cmsg = Caps::convert(msg);
		cb_funcs.recv_post(name, msgtype, cmsg, arg);
		caps_destroy(cmsg);
	}
}

int32_t WrapClientCallback::recv_get(const char* name, shared_ptr<Caps>& msg,
		shared_ptr<Caps>& reply) {
	if (cb_funcs.recv_get) {
		caps_t cmsg = Caps::convert(msg);
		caps_t creply = 0;
		int32_t r = cb_funcs.recv_get(name, cmsg, arg, &creply);
		reply = Caps::convert(creply);
		caps_destroy(cmsg);
		caps_destroy(creply);
		return r;
	}
	return FLORA_CLI_SUCCESS;
}

void WrapClientCallback::disconnected() {
	if (cb_funcs.disconnected) {
		cb_funcs.disconnected(arg);
	}
}

int32_t flora_cli_connect(const char* uri, flora_cli_callback_t* cb,
		void* arg, uint32_t msg_buf_size, flora_cli_t* result) {
	if (result == nullptr)
		return FLORA_CLI_EINVAL;

	uint32_t bufsize = msg_buf_size > DEFAULT_MSG_BUF_SIZE ?
		msg_buf_size : DEFAULT_MSG_BUF_SIZE;
	Client* cli = new Client(bufsize);
	int32_t r;

	WrapClientCallback* wcb = nullptr;
	if (cb) {
		wcb = new WrapClientCallback();
		wcb->cb_funcs = *cb;
		wcb->arg = arg;
	}

	r = cli->connect(uri, wcb);
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
		if (cli->callback)
			delete cli->callback;
		delete cli;
	}
}

int32_t flora_cli_subscribe(flora_cli_t handle, const char* name, uint32_t msgtype) {
	if (handle == 0)
		return FLORA_CLI_EINVAL;
	return reinterpret_cast<Client*>(handle)->subscribe(name, msgtype);
}

int32_t flora_cli_unsubscribe(flora_cli_t handle, const char* name, uint32_t msgtype) {
	if (handle == 0)
		return FLORA_CLI_EINVAL;
	return reinterpret_cast<Client*>(handle)->unsubscribe(name, msgtype);
}

int32_t flora_cli_post(flora_cli_t handle, const char* name, caps_t msg, uint32_t msgtype) {
	if (handle == 0)
		return FLORA_CLI_EINVAL;
	shared_ptr<Caps> pmsg = Caps::convert(msg);
	return reinterpret_cast<Client*>(handle)->post(name, pmsg, msgtype);
}

int32_t flora_cli_get(flora_cli_t handle, const char* name, caps_t msg,
		flora_get_result** results, uint32_t* res_buf_size, uint32_t timeout) {
	if (handle == 0)
		return FLORA_CLI_EINVAL;
	vector<Reply> resarr;
	shared_ptr<Caps> pmsg = Caps::convert(msg);
	int32_t r = reinterpret_cast<Client*>(handle)->get(name, pmsg, resarr, timeout);
	if (r != FLORA_CLI_SUCCESS)
		return r;
	if (results == nullptr || res_buf_size == nullptr)
		return FLORA_CLI_SUCCESS;
	assert(resarr.size() > 0);
	*results = new flora_get_result[resarr.size()];
	*res_buf_size = resarr.size();

	size_t i;
	string* extra;
	for (i = 0; i < resarr.size(); ++i) {
		(*results)[i].ret_code = resarr[i].ret_code;
		(*results)[i].data = Caps::convert(resarr[i].data);
		extra = &resarr[i].extra;
		if (extra->length()) {
			(*results)[i].extra = new char[extra->length() + 1];
			strcpy((*results)[i].extra, extra->c_str());
		} else
			(*results)[i].extra = nullptr;
	}
	return FLORA_CLI_SUCCESS;
}

void flora_result_delete(flora_get_result* results, uint32_t size) {
	if (results == nullptr || size == 0)
		return;

	uint32_t i;
	for (i = 0; i < size; ++i) {
		if (results[i].extra)
			delete[] results[i].extra;
		if (results[i].data)
			caps_destroy(results[i].data);
	}
	delete[] results;
}
