#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/un.h>
#include "unix-conn.h"
#include "rlog.h"

#define TAG "flora.UnixConn"

bool UnixConn::connect(const std::string& name) {
	int fd = socket(AF_UNIX, SOCK_STREAM, 0);
	if (fd < 0) {
		KLOGE(TAG, "socket create failed: %s", strerror(errno));
		return false;
	}
	struct sockaddr_un addr;
	memset(&addr, 0, sizeof(addr));
	addr.sun_family = AF_UNIX;
	strcpy(addr.sun_path, name.c_str());
	if (::connect(fd, (sockaddr*)&addr, sizeof(addr)) < 0) {
		KLOGE(TAG, "socket connect %s failed: %s", name.c_str(),
				strerror(errno));
		::close(fd);
		return false;
	}
	sock = fd;
	return true;
}

bool UnixConn::send(const void* data, uint32_t size) {
	if (sock < 0)
		return false;
	ssize_t c = ::write(sock, data, size);
	if (c < 0) {
		KLOGE(TAG, "write to socket failed: %s", strerror(errno));
		return false;
	}
	if (c == 0) {
		KLOGE(TAG, "write to socket failed: remote closed");
		return false;
	}
	return true;
}

int32_t UnixConn::recv(void* data, uint32_t size) {
	if (sock < 0)
		return -1;
	ssize_t c = ::read(sock, data, size);
	if (c < 0) {
		KLOGE(TAG, "read socket failed: %s", strerror(errno));
	} else if (c == 0) {
		KLOGE(TAG, "read socket failed: remote closed");
	}
	return c;
}

void UnixConn::close() {
	if (sock < 0)
		return;
	::shutdown(sock, SHUT_RDWR);
	::close(sock);
	sock = -1;
}
