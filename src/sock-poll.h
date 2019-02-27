#pragma once

#include "disp.h"
#include "flora-svc.h"
#include "sock-adap.h"
#include <condition_variable>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <sys/select.h>
#include <thread>

typedef std::map<int, std::shared_ptr<SocketAdapter>> AdapterMap;
// typedef std::list<std::shared_ptr<Session> > SessionList;
// typedef std::map<std::string, SessionList> SubscribeMap;

namespace flora {
namespace internal {

class SocketPoll : public flora::Poll {
public:
  explicit SocketPoll(const std::string &name);

  SocketPoll(const std::string &host, int32_t port);

  ~SocketPoll();

  int32_t start(std::shared_ptr<flora::Dispatcher> &disp);

  void stop();

private:
  void run();

  bool init_unix_socket();

  bool init_tcp_socket();

  void new_adapter(int fd);

  void delete_adapter(int fd);

  bool read_from_client(std::shared_ptr<SocketAdapter> &adap);

  int get_listen_fd();

  int32_t do_poll(fd_set *rfds, int max_fd);

private:
  std::shared_ptr<Dispatcher> dispatcher;
  int listen_fd = -1;
  uint32_t max_msg_size = 0;
  std::thread run_thread;
  std::mutex start_mutex;
  std::condition_variable start_cond;
  AdapterMap adapters;
  uint32_t type;
  // for unix socket
  std::string name;
  // for tcp socket
  std::string host;
  int32_t port;
};

} // namespace internal
} // namespace flora
