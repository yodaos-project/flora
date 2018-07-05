#pragma once

#include <stdint.h>
#include "caps.h"

class RequestSerializer {
public:
	static int32_t serialize_auth(uint32_t version, const char* extra, void* data, uint32_t size);

	static int32_t serialize_subscribe(const char* name, void* data, uint32_t size);

	static int32_t serialize_unsubscribe(const char* name, void* data, uint32_t size);

	static int32_t serialize_post(const char* name, caps_t args, void* data, uint32_t size);
};
