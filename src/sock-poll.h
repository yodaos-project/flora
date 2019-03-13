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

namespace flora {
namespace internal {

typedef std::map<int, std::shared_ptr<Adapter>> AdapterMap;

class SocketPoll : public flora::Poll {
public:
  explicit SocketPoll(const std::string &name);

  SocketPoll(const std::string &host, int32_t port);

  ~SocketPoll();

  int32_t start(std::shared_ptr<flora::Dispatcher> &disp);

  void stop();

  virtual void config(uint32_t opt, ...) {}

protected:
  virtual int32_t do_poll(fd_set *rfds, int max_fd);

  virtual std::shared_ptr<Adapter> do_accept(int lfd);

  virtual bool do_read(std::shared_ptr<Adapter> &adap);

private:
  void run();

  bool init_unix_socket();

  bool init_tcp_socket();

  int get_listen_fd();

  std::shared_ptr<Adapter> new_adapter(int fd);

  void delete_adapter(std::shared_ptr<Adapter> &adap);

private:
  std::shared_ptr<Dispatcher> dispatcher;
  int listen_fd = -1;
  int max_fd = 0;
  fd_set all_fds;
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
