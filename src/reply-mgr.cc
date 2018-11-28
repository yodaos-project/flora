#include <sys/mman.h>
#include "reply-mgr.h"
#include "defs.h"
#include "ser-helper.h"
#include "rlog.h"

#define TAG "flora.ReplyManager"

using namespace std;
using std::chrono::steady_clock;
using std::chrono::milliseconds;
using std::chrono::duration_cast;

namespace flora {
namespace internal {

ReplyManager::ReplyManager(uint32_t bufsize) {
  buf_size = bufsize > DEFAULT_MSG_BUF_SIZE ? bufsize : DEFAULT_MSG_BUF_SIZE;
  buffer = (int8_t*)mmap(NULL, buf_size, PROT_READ | PROT_WRITE,
      MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
  run_thread = thread([this]() { this->run(); });
}

ReplyManager::~ReplyManager() {
  pg_mutex.lock();
  working = false;
  pending_gets.clear();
  loop_cond.notify_one();
  pg_mutex.unlock();
  run_thread.join();

  munmap(buffer, buf_size);
}

void ReplyManager::add_req(shared_ptr<Adapter>& sender,
    const char* name, int32_t msgid, int32_t pending_id,
    uint32_t timeout, AdapterList& receivers) {
  steady_clock::time_point now = steady_clock::now();
  steady_clock::time_point tp = now + milliseconds(timeout);
  PendingGetList::iterator it;
  AdapterList::iterator ait;
  lock_guard<mutex> locker(pg_mutex);
  for (it = pending_gets.begin(); it != pending_gets.end(); ++it) {
    if ((*it).tp_timeout > tp)
      break;
  }
  it = pending_gets.emplace(it);
  (*it).sender = sender;
  (*it).msg_name = name;
  if (timeout == 0)
    (*it).tp_timeout = steady_clock::time_point::max();
  else
    (*it).tp_timeout = tp;
  (*it).msgid = msgid;
  (*it).pending_id = pending_id;
  // request的接收者不包括发送者本身
  for (ait = receivers.begin(); ait != receivers.end(); ++ait) {
    if ((*ait).get() != sender.get())
      (*it).receivers.push_back(*ait);
  }
  loop_cond.notify_one();
}

void ReplyManager::put_reply(shared_ptr<Adapter>& sender, const char* name,
    int32_t pending_id, int32_t retcode, shared_ptr<Caps>& data) {
  lock_guard<mutex> locker(pg_mutex);
  PendingGetList::iterator it1 = pending_gets.begin();
  AdapterList::iterator it2;
  while (it1 != pending_gets.end()) {
    if (pending_id == (*it1).pending_id) {
      it2 = (*it1).receivers.begin();
      while (it2 != (*it1).receivers.end()) {
        if (sender.get() == (*it2).get()) {
          (*it1).receivers.erase(it2);
          (*it1).replys.emplace_back();
          (*it1).replys.back().ret_code = retcode;
          (*it1).replys.back().data = data;
          (*it1).replys.back().extra = sender->auth_extra;
          if ((*it1).receivers.empty())
            loop_cond.notify_one();
          break;
        }
        ++it2;
      }
      break;
    }
    ++it1;
  }
}

void ReplyManager::run() {
  milliseconds dur(0);
  steady_clock::time_point now;
  unique_lock<mutex> locker(pg_mutex);

  while (working) {
    // check timeout reply
    now = steady_clock::now();
    while (!pending_gets.empty()) {
      dur = duration_cast<milliseconds>(pending_gets.front().tp_timeout - now);
      if (dur.count() <= 0) {
        pending_gets.pop_front();
        KLOGI(TAG, "erase timeout request");
      } else
        break;
    }

    // check completed replys
    handle_completed_replys();

    if (pending_gets.empty()
        || pending_gets.front().tp_timeout == steady_clock::time_point::max())
      loop_cond.wait(locker);
    else
      loop_cond.wait_until(locker, pending_gets.front().tp_timeout);
  }
}

void ReplyManager::handle_completed_replys() {
  if (pending_gets.empty())
    return;
  PendingGetList::iterator it = pending_gets.begin();
  PendingGetList::iterator dit;
  int32_t c;
  steady_clock::time_point now;
  
  while (it != pending_gets.end()) {
    if ((*it).receivers.empty()) {
      c = ResponseSerializer::serialize_reply(
          (*it).msg_name.c_str(), (*it).msgid,
          (*it).replys, buffer, buf_size, (*it).sender->serialize_flags);
      if (c > 0)
        (*it).sender->write(buffer, c);
      dit = it;
      ++it;
      pending_gets.erase(dit);
      continue;
    }
    ++it;
  }
}

} // namespace internal
} // namespace flora
