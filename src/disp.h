#pragma once

#include <list>
#include <map>
#include <string>
#include <memory>
#include <functional>
#include "adap.h"
#include "defs.h"
#include "caps.h"

typedef std::list<std::shared_ptr<Adapter> > AdapterList;
typedef std::map<std::string, AdapterList> SubscriptionMap;

class Dispatcher  {
public:
	Dispatcher(uint32_t bufsize);

	~Dispatcher();

	bool put(Frame& frame, std::shared_ptr<Adapter>& sender);

	inline uint32_t max_msg_size() const { return buf_size; }

private:
	bool handle_auth_req(caps_t msg_caps, std::shared_ptr<Adapter>& sender);

	bool handle_subscribe_req(caps_t msg_caps, std::shared_ptr<Adapter>& sender);

	bool handle_unsubscribe_req(caps_t msg_caps, std::shared_ptr<Adapter>& sender);

	bool handle_post_req(caps_t msg_caps, std::shared_ptr<Adapter>& sender);

private:
	SubscriptionMap subscriptions;
	int8_t* buffer;
	uint32_t buf_size;

	static bool (Dispatcher::*msg_handlers[MSG_HANDLER_COUNT])(caps_t, std::shared_ptr<Adapter>&);
};
