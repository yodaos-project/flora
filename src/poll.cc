#include "flora-svc.h"
#include "tcp-poll.h"
#include "uri.h"

using namespace rokid;

int32_t flora_poll_new(const char* uri, flora_poll_t* result) {
	if (result == nullptr)
		return FLORA_POLL_INVAL;
	Uri urip;
	if (!urip.parse(uri))
		return FLORA_POLL_INVAL;
	if (urip.scheme == "tcp") {
		*result = reinterpret_cast<flora_poll_t>(new TCPPoll(urip.host, urip.port));
		return FLORA_POLL_SUCCESS;
	}
	return FLORA_POLL_UNSUPP;
}

void flora_poll_delete(flora_poll_t handle) {
	if (handle)
		delete reinterpret_cast<Poll*>(handle);
}

int32_t flora_poll_start(flora_poll_t handle, flora_dispatcher_t dispatcher) {
	if (handle == 0 || dispatcher == 0)
		return FLORA_POLL_INVAL;
	Poll* fpoll = reinterpret_cast<Poll*>(handle);
	Dispatcher* disp = reinterpret_cast<Dispatcher*>(dispatcher);
	return fpoll->start(disp);
}

void flora_poll_stop(flora_poll_t handle) {
	if (handle)
		reinterpret_cast<Poll*>(handle)->close();
}
