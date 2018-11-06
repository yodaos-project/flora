#pragma once

#include <map>
#include <memory>
#include <string>
#include <thread>
#include <mutex>
#include <condition_variable>
#include "flora-svc.h"
#include "sock-adap.h"
#include "disp.h"

typedef std::map<int, std::shared_ptr<SocketAdapter> > AdapterMap;
// typedef std::list<std::shared_ptr<Session> > SessionList;
// typedef std::map<std::string, SessionList> SubscribeMap;

namespace flora {
namespace internal {

class UnixPoll : public flora::Poll {
public:
  UnixPoll(const std::string& name);

  int32_t start(std::shared_ptr<flora::Dispatcher>& disp);

  void stop();

private:
  void run();

  bool init_socket();

  void new_adapter(int fd);

  void delete_adapter(int fd);

  bool read_from_client(std::shared_ptr<SocketAdapter>& adap);

  int get_listen_fd();

private:
  std::shared_ptr<Dispatcher> dispatcher;
  int listen_fd = -1;
  uint32_t max_msg_size = 0;
  std::thread run_thread;
  std::mutex start_mutex;
  std::condition_variable start_cond;
  AdapterMap adapters;
  std::string name;
};

} // namespace internal
} // namespace flora
