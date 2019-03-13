#include "sock-adap.h"
#include "caps.h"
#include "defs.h"
#include "rlog.h"
#include <chrono>
#include <errno.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

using namespace std;

SocketAdapter::SocketAdapter(int sock, uint32_t bufsize, uint32_t flags)
    : Adapter(flags), socketfd(sock) {
  buffer = (int8_t *)mmap(NULL, bufsize, PROT_READ | PROT_WRITE,
                          MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
  buf_size = bufsize;
}

SocketAdapter::~SocketAdapter() { close(); }

int32_t SocketAdapter::read() {
  if (buffer == nullptr)
    return SOCK_ADAPTER_ECLOSED;
  ssize_t c = ::read(socketfd, buffer + cur_size, buf_size - cur_size);
  if (c <= 0) {
    if (c == 0) {
      KLOGD(TAG, "socket closed by remote");
    } else {
      KLOGE(TAG, "read socket failed: %s", strerror(errno));
    }
    return SOCK_ADAPTER_ECLOSED;
  }
  cur_size += c;
#ifdef FLORA_DEBUG
  recv_bytes += c;
#endif
  return SOCK_ADAPTER_SUCCESS;
}

int32_t SocketAdapter::next_frame(Frame &frame) {
  if (buffer == nullptr)
    return SOCK_ADAPTER_ECLOSED;
  uint32_t sz = cur_size - frame_begin;
  if (sz < HEADER_SIZE)
    goto nomore;
  uint32_t version;
  uint32_t length;
  if (Caps::binary_info(buffer + frame_begin, &version, &length) !=
      CAPS_SUCCESS)
    return SOCK_ADAPTER_EPROTO;
  if (length > buf_size)
    return SOCK_ADAPTER_ENOBUF;
  if (length > sz)
    goto nomore;
  frame.data = buffer + frame_begin;
  frame.size = length;
  frame_begin += length;
#ifdef FLORA_DEBUG
  ++recv_times;
#endif
  return SOCK_ADAPTER_SUCCESS;

nomore:
  if (frame_begin < cur_size) {
    memmove(buffer, buffer + frame_begin, sz);
  }
  frame_begin = 0;
  cur_size = sz;
  return SOCK_ADAPTER_ENOMORE;
}

void SocketAdapter::close() {
  lock_guard<mutex> locker(write_mutex);
  close_nolock();
}

void SocketAdapter::close_nolock() {
  if (buffer) {
    munmap(buffer, buf_size);
    buffer = nullptr;
#ifdef FLORA_DEBUG
    KLOGI(TAG, "socket adapter %s: recv times = %u, recv bytes = %u",
          info ? info->name.c_str() : "", recv_times, recv_bytes);
#endif
  }
}

bool SocketAdapter::closed() {
  lock_guard<mutex> locker(write_mutex);
  return buffer == nullptr;
}

void SocketAdapter::write(const void *data, uint32_t size) {
  lock_guard<mutex> locker(write_mutex);
  if (buffer == nullptr)
    return;
  if (::write(socketfd, data, size) < 0) {
    KLOGE(TAG, "write to socket failed: %s", strerror(errno));
    close_nolock();
  }
}
