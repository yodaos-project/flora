#pragma once

#include <list>
#include <vector>
#include <memory>
#include <string>
#include <chrono>
#include <mutex>
#include <condition_variable>
#include <thread>
#include "adap.h"
#include "flora-cli.h"

namespace flora {
namespace internal {

typedef std::list<std::shared_ptr<Adapter> > AdapterList;
typedef struct {
  std::shared_ptr<Adapter> sender;
  std::string msg_name;
  int32_t msgid;
  int32_t pending_id;
  std::chrono::steady_clock::time_point tp_timeout;
  std::vector<flora::Reply> replys;
  AdapterList receivers;
} PendingGet;
typedef std::list<PendingGet> PendingGetList;

class ReplyManager {
public:
  ReplyManager(uint32_t bufsize);

  ~ReplyManager();

  void add_req(std::shared_ptr<Adapter>& sender, const char* name,
      int32_t msgid, int32_t pending_id, uint32_t timeout,
      AdapterList& receivers);

  void put_reply(std::shared_ptr<Adapter>& sender, const char* name,
      int32_t pending_id, int32_t retcode, std::shared_ptr<Caps>& data);

private:
  void run();

  void handle_completed_replys();

private:
  PendingGetList pending_gets;
  std::mutex pg_mutex;
  std::condition_variable loop_cond;
  std::thread run_thread;
  int8_t* buffer;
  uint32_t buf_size;
  bool working = true;
};

} // namespace internal
} // namespace flora
