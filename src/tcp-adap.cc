#include <unistd.h>
#include <string.h>
#include <sys/mman.h>
#include <errno.h>
#include "tcp-adap.h"
#include "rlog.h"
#include "caps.h"

#define TAG "flora.TCPAdapter"

TCPAdapter::TCPAdapter(int sock, uint32_t bufsize) : socket(sock) {
	buffer = (int8_t*)mmap(NULL, bufsize, PROT_READ | PROT_WRITE,
			MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
	buf_size = bufsize;
}

TCPAdapter::~TCPAdapter() {
	close();
#ifdef FLORA_DEBUG
	KLOGI(TAG, "tcp adapter %s: recv times = %u, recv bytes = %u",
			auth_extra.c_str(), recv_times, recv_bytes);
#endif
}

int32_t TCPAdapter::read() {
	if (buffer == nullptr)
		return TCP_ADAPTER_ECLOSED;
	ssize_t c = ::read(socket, buffer + cur_size, buf_size - cur_size);
	if (c <= 0) {
		if (c == 0)
			KLOGE(TAG, "socket closed by remote");
		else
			KLOGE(TAG, "read socket failed: %s", strerror(errno));
		close();
		return TCP_ADAPTER_ECLOSED;
	}
	cur_size += c;
#ifdef FLORA_DEBUG
	recv_bytes += c;
#endif
	return TCP_ADAPTER_SUCCESS;
}

int32_t TCPAdapter::next_frame(Frame& frame) {
	if (buffer == nullptr)
		return TCP_ADAPTER_ECLOSED;
	uint32_t sz = cur_size - frame_begin;
	if (sz < HEADER_SIZE)
		goto nomore;
	uint32_t version;
	uint32_t length;
	if (caps_binary_info(buffer + frame_begin, &version, &length) != CAPS_SUCCESS)
		return TCP_ADAPTER_EPROTO;
	if (length > buf_size)
		return TCP_ADAPTER_ENOBUF;
	if (length > sz)
		goto nomore;
	frame.data = buffer + frame_begin;
	frame.size = length;
	frame_begin += length;
#ifdef FLORA_DEBUG
	++recv_times;
#endif
	return TCP_ADAPTER_SUCCESS;

nomore:
	if (frame_begin < cur_size) {
		memmove(buffer, buffer + frame_begin, sz);
	}
	frame_begin = 0;
	cur_size = sz;
	return TCP_ADAPTER_ENOMORE;
}

void TCPAdapter::close() {
	munmap(buffer, buf_size);
	buffer = nullptr;
}

bool TCPAdapter::closed() const {
	return buffer == nullptr;
}

void TCPAdapter::write(const void* data, uint32_t size) {
	if (buffer == nullptr)
		return;
	if (::write(socket, data, size) < 0) {
		KLOGE(TAG, "write to socket failed: %s", strerror(errno));
		close();
	}
}
