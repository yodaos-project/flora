#include "test-svc.h"

bool TestService::run(const char* uri) {
	dispatcher = flora_dispatcher_new(0);
	if (dispatcher == 0)
		return false;
	if (flora_poll_new(uri, &fpoll) != FLORA_POLL_SUCCESS) {
		flora_dispatcher_delete(dispatcher);
		dispatcher = 0;
		return false;
	}
	if (flora_poll_start(fpoll, dispatcher) != FLORA_POLL_SUCCESS) {
		flora_poll_delete(fpoll);
		fpoll = 0;
		flora_dispatcher_delete(dispatcher);
		dispatcher = 0;
		return false;
	}
	return true;
}

void TestService::close() {
	flora_poll_stop(fpoll);
	flora_poll_delete(fpoll);
	fpoll = 0;
	flora_dispatcher_delete(dispatcher);
	dispatcher = 0;
}
