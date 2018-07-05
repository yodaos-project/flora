#include "reqser.h"
#include "defs.h"

int32_t RequestSerializer::serialize_auth(uint32_t version, const char* extra, void* data, uint32_t size) {
	caps_t caps = caps_create();
	caps_write_integer(caps, CMD_AUTH_REQ);
	caps_write_integer(caps, version);
	if (extra[0] != '\0')
		caps_write_string(caps, extra);
	int32_t r = caps_serialize(caps, data, size);
	caps_destroy(caps);
	if (r < 0)
		return -1;
	if (r > size)
		return -1;
	return r;
}

int32_t RequestSerializer::serialize_subscribe(const char* name, void* data, uint32_t size) {
	caps_t caps = caps_create();
	caps_write_integer(caps, CMD_SUBSCRIBE_REQ);
	caps_write_string(caps, name);
	int32_t r = caps_serialize(caps, data, size);
	caps_destroy(caps);
	if (r < 0)
		return -1;
	if (r > size)
		return -1;
	return r;
}

int32_t RequestSerializer::serialize_unsubscribe(const char* name, void* data, uint32_t size) {
	caps_t caps = caps_create();
	caps_write_integer(caps, CMD_UNSUBSCRIBE_REQ);
	caps_write_string(caps, name);
	int32_t r = caps_serialize(caps, data, size);
	caps_destroy(caps);
	if (r < 0)
		return -1;
	if (r > size)
		return -1;
	return r;
}

int32_t RequestSerializer::serialize_post(const char* name, caps_t args, void* data, uint32_t size) {
	caps_t caps = caps_create();
	caps_write_integer(caps, CMD_POST_REQ);
	caps_write_string(caps, name);
	if (args)
		caps_write_object(caps, args);
	int32_t r = caps_serialize(caps, data, size);
	caps_destroy(caps);
	if (r < 0)
		return -1;
	if (r > size)
		return -1;
	return r;
}
