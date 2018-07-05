#pragma once

#include <stdint.h>
#include "disp.h"

class Poll {
public:
	virtual ~Poll() = default;

	virtual int32_t start(Dispatcher* disp) = 0;

	virtual void close() = 0;
};
