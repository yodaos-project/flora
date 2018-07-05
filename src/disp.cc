#include <sys/mman.h>
#include "flora-svc.h"
#include "flora-cli.h"
#include "disp.h"
#include "rlog.h"
#include "caps.h"

using namespace std;

#define TAG "flora.Dispatcher"

bool (Dispatcher::*(Dispatcher::msg_handlers[MSG_HANDLER_COUNT]))(caps_t, std::shared_ptr<Adapter>&) = {
	&Dispatcher::handle_auth_req,
	&Dispatcher::handle_subscribe_req,
	&Dispatcher::handle_unsubscribe_req,
	&Dispatcher::handle_post_req
};

Dispatcher::Dispatcher(uint32_t bufsize) {
	buf_size = bufsize > DEFAULT_MSG_BUF_SIZE ? bufsize : DEFAULT_MSG_BUF_SIZE;
	buffer = (int8_t*)mmap(NULL, buf_size, PROT_READ | PROT_WRITE,
			MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
}

Dispatcher::~Dispatcher() {
	subscriptions.clear();
	munmap(buffer, buf_size);
}

bool Dispatcher::put(Frame& frame, shared_ptr<Adapter>& sender) {
	caps_t msg_caps;

	if (caps_parse(frame.data, frame.size, &msg_caps) != CAPS_SUCCESS) {
		KLOGE(TAG, "msg caps parse failed");
		return false;
	}

	int32_t cmd;
	if (caps_read_integer(msg_caps, &cmd) != CAPS_SUCCESS) {
		caps_destroy(msg_caps);
		return false;
	}
	if (cmd < 0 || cmd >= MSG_HANDLER_COUNT) {
		caps_destroy(msg_caps);
		return false;
	}
	bool b = (this->*(msg_handlers[cmd]))(msg_caps, sender);
	caps_destroy(msg_caps);
	return b;
}

bool Dispatcher::handle_auth_req(caps_t msg_caps, shared_ptr<Adapter>& sender) {
	int32_t version;

	if (caps_read_integer(msg_caps, &version) != CAPS_SUCCESS)
		return false;
	const char* extra;
	if (caps_read_string(msg_caps, &extra) == CAPS_SUCCESS) {
		sender->auth_extra = extra;
	}

	caps_t reply_caps = caps_create();
	int32_t result = FLORA_CLI_SUCCESS;
	int32_t c;
	caps_write_integer(reply_caps, CMD_AUTH_RESP);
	if (version < FLORA_VERSION) {
		result = FLORA_CLI_EAUTH;
		KLOGE(TAG, "version %d not supported, protocol version is %d", version, FLORA_VERSION);
	}
	caps_write_integer(reply_caps, result);
	c = caps_serialize(reply_caps, buffer, buf_size);
	if (c < 0) {
		caps_destroy(reply_caps);
		return false;
	}
	sender->write(buffer, c);
	caps_destroy(reply_caps);
	return true;
}

bool Dispatcher::handle_subscribe_req(caps_t msg_caps, shared_ptr<Adapter>& sender) {
	const char* name;

	if (caps_read_string(msg_caps, &name) != CAPS_SUCCESS)
		return false;
	if (name[0] == '\0')
		return false;
	string name_str(name);
	AdapterList& adapters = subscriptions[name_str];
	AdapterList::iterator it;
	for (it = adapters.begin(); it != adapters.end(); ++it) {
		if ((*it).get() == sender.get())
			return true;
	}
	adapters.push_back(sender);
	return true;
}

bool Dispatcher::handle_unsubscribe_req(caps_t msg_caps, shared_ptr<Adapter>& sender) {
	const char* name;

	if (caps_read_string(msg_caps, &name) != CAPS_SUCCESS)
		return false;
	if (name[0] == '\0')
		return false;
	string name_str(name);
	AdapterList& adapters = subscriptions[name_str];
	AdapterList::iterator it;
	for (it = adapters.begin(); it != adapters.end(); ++it) {
		if ((*it).get() == sender.get()) {
			adapters.erase(it);
			break;
		}
	}
	return true;
}

bool Dispatcher::handle_post_req(caps_t msg_caps, shared_ptr<Adapter>& sender) {
	const char* name;
	caps_t args = 0;

	if (caps_read_string(msg_caps, &name) != CAPS_SUCCESS)
		return false;
#ifdef FLORA_DEBUG
	KLOGI(TAG, "recv msg %s from %s", name, sender->auth_extra.c_str());
#endif
	if (name[0] == '\0')
		return false;
	caps_read_object(msg_caps, &args);
	string name_str(name);
	SubscriptionMap::iterator sit;
	sit = subscriptions.find(name_str);
	if (sit == subscriptions.end()) {
		return true;
	}
	if (sit->second.empty()) {
		return true;
	}
	caps_t reply_caps = caps_create();
	caps_write_integer(reply_caps, CMD_RECV_MSG);
	caps_write_string(reply_caps, name);
	if (args)
		caps_write_object(reply_caps, args);
	int32_t c = caps_serialize(reply_caps, buffer, buf_size);
	if (c < 0) {
		caps_destroy(reply_caps);
		return false;
	}
	caps_destroy(reply_caps);
	AdapterList::iterator ait;
	ait = sit->second.begin();
	while (ait != sit->second.end()) {
		if ((*ait)->closed()) {
			AdapterList::iterator dit = ait;
			++ait;
			sit->second.erase(dit);
			continue;
		}
		if ((*ait).get() != sender.get()) {
#ifdef FLORA_DEBUG
			KLOGI(TAG, "dispatch msg %s from %s to %s", name,
					sender->auth_extra.c_str(), (*ait)->auth_extra.c_str());
#endif
			(*ait)->write(buffer, c);
		}
		++ait;
	}
	return true;
}


flora_dispatcher_t flora_dispatcher_new(uint32_t bufsize) {
	return reinterpret_cast<flora_dispatcher_t>(new Dispatcher(bufsize));
}

void flora_dispatcher_delete(flora_dispatcher_t handle) {
	if (handle) {
		delete reinterpret_cast<Dispatcher*>(handle);
	}
}
