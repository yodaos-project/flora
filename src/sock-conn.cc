#include "sock-conn.h"
#include "defs.h"
#include "rlog.h"
#include <arpa/inet.h>
#include <chrono>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>
#include <unistd.h>

using namespace std;

SocketConn::SocketConn(uint32_t rtmo) : recv_timeout(rtmo) {}

SocketConn::~SocketConn() {
  if (sock >= 0) {
    ::close(sock);
  }
}

bool SocketConn::connect(const std::string &name) {
  int fd = socket(AF_UNIX, SOCK_STREAM, 0);
  if (fd < 0) {
    KLOGE(TAG, "socket create failed: %s", strerror(errno));
    return false;
  }
  // set socket recv timeout
  if (recv_timeout > 0) {
    struct timeval tv;
    tv.tv_sec = recv_timeout / 1000;
    tv.tv_usec = (recv_timeout % 1000) * 1000;
    setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
  }
  struct sockaddr_un addr;
  memset(&addr, 0, sizeof(addr));
  addr.sun_family = AF_UNIX;
  strcpy(addr.sun_path, name.c_str());
  if (::connect(fd, (sockaddr *)&addr, sizeof(addr)) < 0) {
    KLOGE(TAG, "socket connect %s failed: %s", name.c_str(), strerror(errno));
    ::close(fd);
    return false;
  }
  sock = fd;
  sock_ready = true;
  return true;
}

bool SocketConn::connect(const std::string &host, int32_t port) {
  int fd = socket(AF_INET, SOCK_STREAM, 0);
  if (fd < 0) {
    KLOGE(TAG, "socket create failed: %s", strerror(errno));
    return false;
  }
  // set socket recv timeout
  if (recv_timeout > 0) {
    struct timeval tv;
    tv.tv_sec = recv_timeout / 1000;
    tv.tv_usec = (recv_timeout % 1000) * 1000;
    setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
  }
  struct sockaddr_in addr;
  struct hostent *hp;
  hp = gethostbyname(host.c_str());
  if (hp == nullptr) {
    KLOGE(TAG, "dns failed for host %s: %s", host.c_str(), strerror(errno));
    ::close(fd);
    return false;
  }
  memset(&addr, 0, sizeof(addr));
  addr.sin_family = AF_INET;
  memcpy(&addr.sin_addr, hp->h_addr_list[0], sizeof(addr.sin_addr));
  addr.sin_port = htons(port);
  if (::connect(fd, (sockaddr *)&addr, sizeof(addr)) < 0) {
    KLOGE(TAG, "socket connect failed: %s", strerror(errno));
    ::close(fd);
    return false;
  }
  sock = fd;
  sock_ready = true;
  return true;
}

bool SocketConn::send(const void *data, uint32_t size) {
  lock_guard<mutex> locker(write_mutex);
  if (!sock_ready)
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

int32_t SocketConn::recv(void *data, uint32_t size) {
  unique_lock<mutex> locker(write_mutex);
  if (!sock_ready)
    return -1;
  locker.unlock();

  ssize_t c;
  do {
    c = ::read(sock, data, size);
    if (c < 0) {
      if (errno == EAGAIN)
        return -2;
      if (errno == EINTR) {
        KLOGE(TAG, "read socket failed: %s", strerror(errno));
        continue;
      }
      KLOGE(TAG, "read socket failed: %s", strerror(errno));
    } else if (c == 0) {
      KLOGD(TAG, "read socket failed: remote closed");
    }
    break;
  } while (true);
  return c;
}

void SocketConn::close() {
  lock_guard<mutex> locker(write_mutex);
  if (sock_ready) {
    sock_ready = false;
    ::shutdown(sock, SHUT_RDWR);
  }
}
