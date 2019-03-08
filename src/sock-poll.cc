#include "sock-poll.h"
#include "flora-svc.h"
#include "rlog.h"
#include <chrono>
#include <netdb.h>
#include <netinet/in.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>
#include <unistd.h>
#include <vector>

using namespace std;

#define POLL_TYPE_UNIX 0
#define POLL_TYPE_TCP 1

namespace flora {
namespace internal {

SocketPoll::SocketPoll(const std::string &name) {
  this->name = name;
  type = POLL_TYPE_UNIX;
  this->port = 0;
}

SocketPoll::SocketPoll(const std::string &host, int32_t port) {
  this->host = host;
  this->port = port;
  type = POLL_TYPE_TCP;
}

SocketPoll::~SocketPoll() { stop(); }

int32_t SocketPoll::start(shared_ptr<flora::Dispatcher> &disp) {
  unique_lock<mutex> locker(start_mutex);
  if (dispatcher.get())
    return FLORA_POLL_ALREADY_START;
  bool r;
  if (type == POLL_TYPE_TCP)
    r = init_tcp_socket();
  else
    r = init_unix_socket();
  if (!r)
    return FLORA_POLL_SYSERR;
  run_thread = thread([this]() { this->run(); });
  dispatcher = static_pointer_cast<Dispatcher>(disp);
  max_msg_size = dispatcher->max_msg_size();
  // wait until thread running
  start_cond.wait(locker);
  return FLORA_POLL_SUCCESS;
}

void SocketPoll::stop() {
  unique_lock<mutex> locker(start_mutex);
  if (dispatcher.get() == nullptr)
    return;
  int fd = listen_fd;
  listen_fd = -1;
  ::shutdown(fd, SHUT_RDWR);
  locker.unlock();
  run_thread.join();
  ::close(fd);
  dispatcher.reset();
}

int32_t SocketPoll::do_poll(fd_set *rfds, int max_fd) {
  int r;
#ifdef SELECT_BLOCK_IF_FD_CLOSED
  struct timeval tv;
  tv.tv_sec = 5;
  tv.tv_usec = 0;
#endif
  while (true) {
#ifdef SELECT_BLOCK_IF_FD_CLOSED
    r = select(max_fd, rfds, nullptr, nullptr, &tv);
#else
    r = select(max_fd, rfds, nullptr, nullptr, nullptr);
#endif
    if (r < 0) {
      if (errno == EAGAIN) {
        sleep(1);
        continue;
      }
      KLOGE(TAG, "select failed: %s", strerror(errno));
    }
    break;
  }
  return r;
}

static int unix_accept(int lfd) {
  sockaddr_un addr;
  socklen_t addr_len = sizeof(addr);
  return accept(lfd, (sockaddr *)&addr, &addr_len);
}

static int tcp_accept(int lfd) {
  sockaddr_in addr;
  socklen_t addr_len = sizeof(addr);
  return accept(lfd, (sockaddr *)&addr, &addr_len);
}

void SocketPoll::run() {
  fd_set rfds;
  int ifd;

  start_mutex.lock();
  FD_ZERO(&all_fds);
  FD_SET(listen_fd, &all_fds);
  max_fd = listen_fd + 1;
  start_cond.notify_one();
  start_mutex.unlock();
  while (true) {
    rfds = all_fds;
    int32_t r = do_poll(&rfds, max_fd);
    // system call error
    if (r < 0)
      break;
    // closed
    int lfd = get_listen_fd();
    if (lfd < 0) {
      KLOGI(TAG, "unix poll closed, quit");
      break;
    }
    // select timeout, this Poll not closed, continue
    if (r == 0)
      continue;
    for (ifd = 0; ifd < max_fd; ++ifd) {
      if (FD_ISSET(ifd, &rfds)) {
        if (ifd == lfd) {
          auto new_adap = do_accept(lfd);
          if (new_adap == nullptr) {
            KLOGE(TAG, "accept failed: %s", strerror(errno));
            continue;
          }
          KLOGI(TAG, "accept new connection %d",
                static_pointer_cast<SocketAdapter>(new_adap)->socket());
        } else {
          auto it = adapters.find(ifd);
          if (it != adapters.end()) {
            KLOGD(TAG, "read from fd %d", ifd);
            if (!do_read(it->second)) {
              delete_adapter(it->second);
              adapters.erase(it);
            }
          }
        }
      }
    }
  }

  // release resources
  while (adapters.size() > 0) {
    auto it = adapters.begin();
    delete_adapter(it->second);
    adapters.erase(it);
  }
  KLOGI(TAG, "unix poll: run thread quit");
}

shared_ptr<Adapter> SocketPoll::do_accept(int lfd) {
  int new_fd = -1;

  if (type == POLL_TYPE_TCP)
    new_fd = tcp_accept(lfd);
  else
    new_fd = unix_accept(lfd);
  if (new_fd < 0)
    return nullptr;
  auto adap = new_adapter(new_fd);
  adapters.insert(make_pair(new_fd, adap));
  return adap;
}

bool SocketPoll::init_unix_socket() {
  int fd = socket(PF_UNIX, SOCK_STREAM, 0);
  if (fd < 0)
    return false;
  struct sockaddr_un addr;
  memset(&addr, 0, sizeof(addr));
  addr.sun_family = AF_UNIX;
  strcpy(addr.sun_path, name.c_str());
  unlink(name.c_str());
  if (::bind(fd, (sockaddr *)&addr, sizeof(addr)) < 0) {
    ::close(fd);
    KLOGE(TAG, "socket bind failed: %s", strerror(errno));
    return false;
  }
  listen(fd, 10);
  listen_fd = fd;
  return true;
}

bool SocketPoll::init_tcp_socket() {
  int fd = socket(PF_INET, SOCK_STREAM, 0);
  if (fd < 0)
    return false;
  int v = 1;
  setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &v, sizeof(v));
  struct sockaddr_in addr;
  struct hostent *hp;
  hp = gethostbyname(host.c_str());
  if (hp == nullptr) {
    KLOGE(TAG, "gethostbyname failed for host %s: %s", host.c_str(),
          strerror(errno));
    ::close(fd);
    return false;
  }
  memset(&addr, 0, sizeof(addr));
  addr.sin_family = AF_INET;
  memcpy(&addr.sin_addr, hp->h_addr_list[0], sizeof(addr.sin_addr));
  addr.sin_port = htons(port);
  if (::bind(fd, (sockaddr *)&addr, sizeof(addr)) < 0) {
    ::close(fd);
    KLOGE(TAG, "socket bind failed: %s", strerror(errno));
    return false;
  }
  listen(fd, 10);
  listen_fd = fd;
  return true;
}

shared_ptr<Adapter> SocketPoll::new_adapter(int fd) {
  shared_ptr<SocketAdapter> adap = make_shared<SocketAdapter>(
      fd, max_msg_size, type == POLL_TYPE_TCP ? CAPS_FLAG_NET_BYTEORDER : 0);
  if (fd >= max_fd)
    max_fd = fd + 1;
  FD_SET(fd, &all_fds);
  return static_pointer_cast<Adapter>(adap);
}

void SocketPoll::delete_adapter(shared_ptr<Adapter> &adap) {
  dispatcher->erase_adapter(adap);
  int fd = static_pointer_cast<SocketAdapter>(adap)->socket();
  adap->close();
  FD_CLR(fd, &all_fds);
  ::close(fd);
}

bool SocketPoll::do_read(shared_ptr<Adapter> &adap) {
  int32_t r = adap->read();
  if (r != SOCK_ADAPTER_SUCCESS)
    return false;

  Frame frame;
  while (true) {
    r = adap->next_frame(frame);
    if (r == SOCK_ADAPTER_SUCCESS) {
      if (!dispatcher->put(frame.data, frame.size, adap)) {
        KLOGE(TAG, "dispatcher put failed");
        return false;
      }
    } else if (r == SOCK_ADAPTER_ENOMORE) {
      break;
    } else {
      KLOGE(TAG, "adapter next frame return %d", r);
      return false;
    }
  }
  return true;
}

int SocketPoll::get_listen_fd() {
  lock_guard<mutex> locker(start_mutex);
  return listen_fd;
}

} // namespace internal
} // namespace flora
