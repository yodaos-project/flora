#pragma once

#include "caps.h"
#include "conn.h"
#include "flora-cli.h"
#include <chrono>
#include <condition_variable>
#include <list>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace flora {
namespace internal {

typedef struct {
  int32_t id;
  ResponseArray *results;
  std::function<void(ResponseArray &)> callback;
  std::chrono::steady_clock::time_point timeout;
} PendingRequest;
typedef std::list<PendingRequest> PendingRequestList;

class Client : public flora::Client {
public:
  explicit Client(uint32_t bufsize);

  ~Client() noexcept;

  int32_t connect(const char *uri, ClientCallback *cb);

  void close(bool passive);

  // implementation of flora::Client
  int32_t subscribe(const char *name);

  int32_t unsubscribe(const char *name);

  int32_t post(const char *name, std::shared_ptr<Caps> &msg, uint32_t msgtype);

  int32_t get(const char *name, std::shared_ptr<Caps> &msg,
              ResponseArray &replys, uint32_t timeout);

  int32_t get(const char *name, std::shared_ptr<Caps> &msg,
              std::function<void(ResponseArray &)> &&cb);

  int32_t get(const char *name, std::shared_ptr<Caps> &msg,
              std::function<void(ResponseArray &)> &cb);

private:
  bool auth(const std::string &extra);

  void recv_loop();

  bool handle_received(int32_t size);

  void iclose(bool passive);

private:
  uint32_t buf_size;
  int8_t *sbuffer = nullptr;
  int8_t *rbuffer = nullptr;
  uint32_t rbuf_off = 0;
  std::shared_ptr<Connection> connection;
  std::thread recv_thread;
  std::mutex cli_mutex;
  std::mutex req_mutex;
  std::condition_variable req_reply_cond;
  PendingRequestList pending_requests;
  int32_t reqseq = 0;
  uint32_t serialize_flags = 0;

public:
  std::string auth_extra;
  ClientCallback *callback = nullptr;
#ifdef FLORA_DEBUG
  uint32_t post_times = 0;
  uint32_t post_bytes = 0;
  uint32_t req_times = 0;
  uint32_t req_bytes = 0;
  uint32_t send_times = 0;
  uint32_t send_bytes = 0;
#endif
};

} // namespace internal
} // namespace flora
