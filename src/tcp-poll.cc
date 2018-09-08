#include <sys/types.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>
#include <string.h>
#include <chrono>
#include <vector>
#include "flora-svc.h"
#include "tcp-poll.h"
#include "rlog.h"

using namespace std;

#define TAG "flora.TCPPoll"

namespace flora {
namespace internal {

TCPPoll::TCPPoll(const std::string& host, int32_t port) {
  this->host = host;
  this->port = port;
}

int32_t TCPPoll::start(shared_ptr<flora::Dispatcher>& disp) {
  lock_guard<mutex> locker(start_mutex);
  if (dispatcher.get())
    return FLORA_POLL_ALREADY_START;
  if (!init_socket()) {
    return FLORA_POLL_SYSERR;
  }
  run_thread = thread([this]() { this->run(); });
  dispatcher = static_pointer_cast<Dispatcher>(disp);
  max_msg_size = dispatcher->max_msg_size();
  return FLORA_POLL_SUCCESS;
}

void TCPPoll::stop() {
  unique_lock<mutex> locker(start_mutex);
  if (dispatcher.get() == nullptr)
    return;
  int fd = listen_fd;
  listen_fd = -1;
  ::shutdown(fd, SHUT_RDWR);
  run_thread.join();
  ::close(fd);
  dispatcher.reset();
}

void TCPPoll::run() {
  fd_set all_fds;
  fd_set rfds;
  sockaddr_in addr;
  socklen_t addr_len;
  int new_fd;
  int max_fd;
  AdapterMap::iterator adap_it;
  vector<int> pending_delete_adapters;
  size_t i;

  FD_ZERO(&all_fds);
  FD_SET(listen_fd, &all_fds);
  max_fd = listen_fd + 1;
  while (true) {
    rfds = all_fds;
    if (select(max_fd, &rfds, nullptr, nullptr, nullptr) < 0) {
      if (errno == EAGAIN) {
        sleep(1);
        continue;
      }
      KLOGE(TAG, "select failed: %s", strerror(errno));
      break;
    }
    // closed
    if (listen_fd < 0) {
      break;
    }
    // accept new connection
    if (FD_ISSET(listen_fd, &rfds)) {
      addr_len = sizeof(addr);
      new_fd = accept(listen_fd, (sockaddr*)&addr, &addr_len);
      if (new_fd < 0) {
        KLOGE(TAG, "accept failed: %s", strerror(errno));
        continue;
      }
      if (new_fd >= max_fd)
        max_fd = new_fd + 1;
      FD_SET(new_fd, &all_fds);
      KLOGI(TAG, "accept new connection %d", new_fd);
      new_adapter(new_fd);
    }
    // read
    for (adap_it = adapters.begin(); adap_it != adapters.end(); ++adap_it) {
      if (FD_ISSET(adap_it->first, &rfds)) {
        KLOGI(TAG, "read from fd %d", adap_it->first);
        if (!read_from_client(adap_it->second)) {
          pending_delete_adapters.push_back(adap_it->first);
          FD_CLR(adap_it->first, &all_fds);
        }
      }
    }
    for (i = 0; i < pending_delete_adapters.size(); ++i) {
      delete_adapter(pending_delete_adapters[i]);
    }
    pending_delete_adapters.clear();
  }

  // release resources
  while (adapters.size() > 0) {
    delete_adapter(adapters.begin()->first);
  }
}

bool TCPPoll::init_socket() {
  int fd = socket(PF_INET, SOCK_STREAM, 0);
  if (fd < 0)
    return false;
  int v = 1;
  setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &v, sizeof(v));
  struct sockaddr_in addr;
  struct hostent* hp;
  hp = gethostbyname(host.c_str());
  if (hp == nullptr) {
    KLOGE(TAG, "gethostbyname failed for host %s: %s", host.c_str(), strerror(errno));
    ::close(fd);
    return false;
  }
  memset(&addr, 0, sizeof(addr));
  addr.sin_family = AF_INET;
  memcpy(&addr.sin_addr, hp->h_addr_list[0], sizeof(addr.sin_addr));
  addr.sin_port = htons(port);
  if (::bind(fd, (sockaddr*)&addr, sizeof(addr)) < 0) {
    ::close(fd);
    KLOGE(TAG, "socket bind failed: %s", strerror(errno));
    return false;
  }
  listen(fd, 10);
  listen_fd = fd;
  return true;
}

void TCPPoll::new_adapter(int fd) {
  shared_ptr<SocketAdapter> adap = make_shared<SocketAdapter>(fd, max_msg_size);
  adapters.insert(make_pair(fd, adap));
}

void TCPPoll::delete_adapter(int fd) {
  AdapterMap::iterator it = adapters.find(fd);
  if (it != adapters.end()) {
    ::close(fd);
    it->second->close();
    adapters.erase(it);
  }
}

bool TCPPoll::read_from_client(shared_ptr<SocketAdapter>& adap) {
  int32_t r = adap->read();
  if (r != SOCK_ADAPTER_SUCCESS)
    return false;

  Frame frame;
  while (true) {
    r = adap->next_frame(frame);
    if (r == SOCK_ADAPTER_SUCCESS) {
      shared_ptr<Adapter> a = static_pointer_cast<Adapter>(adap);
      if (!dispatcher->put(frame, a))
        return false;
    } else if (r == SOCK_ADAPTER_ENOMORE) {
      break;
    } else {
      return false;
    }
  }
  return true;
}

} // namespace internal
} // namespace flora
