#pragma once

#include "flora-svc.h"

class TestService {
public:
	bool run(const char* uri);

	void close();

private:
	flora_dispatcher_t dispatcher = 0;
	flora_poll_t fpoll = 0;
};
