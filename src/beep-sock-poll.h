#pragma once

#include "defs.h"
#include "rlog.h"
#include "sock-poll.h"
#include <chrono>
#include <memory>
#include <errno.h>
#include <stdarg.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

namespace flora {
namespace internal {

class ActiveAdapter {
public:
  std::chrono::steady_clock::time_point lastest_action_tp;
  std::shared_ptr<Adapter> adapter;
};
typedef std::list<ActiveAdapter> ActiveAdapterList;

class BeepSocketPoll : public SocketPoll {
public:
  BeepSocketPoll(const std::string &host, int32_t port)
      : SocketPoll(host, port) {}

  void config(uint32_t opt, ...) {
    va_list ap;
    va_start(ap, opt);
    switch (opt) {
    case FLORA_POLL_OPT_KEEPALIVE_TIMEOUT:
      options.beep_timeout = va_arg(ap, uint32_t);
      break;
    }
    va_end(ap);
  }

private:
  int32_t do_poll(fd_set *rfds, int max_fd) {
    int r;
    struct timeval tv;
    struct timeval *ptv;
    std::chrono::steady_clock::time_point nowtp;

    while (true) {
      if (active_adapters.empty()) {
#ifdef SELECT_BLOCK_IF_FD_CLOSED
        tv.tv_sec = 5;
        tv.tv_usec = 0;
        ptv = &tv;
#else
        ptv = nullptr;
#endif
      } else {
        nowtp = std::chrono::steady_clock::now();
        obtain_timeout(nowtp, &tv);
        ptv = &tv;
      }
      r = select(max_fd, rfds, nullptr, nullptr, ptv);
      if (r < 0) {
        if (errno == EAGAIN) {
          sleep(1);
          continue;
        }
        KLOGE(TAG, "select failed: %s", strerror(errno));
      }
      nowtp = std::chrono::steady_clock::now();
      shutdown_timeout_adapter(nowtp);
      break;
    }
    return r;
  }

  std::shared_ptr<Adapter> do_accept(int lfd) {
    auto adap = SocketPoll::do_accept(lfd);
    if (adap != nullptr) {
      auto it = active_adapters.emplace(active_adapters.end());
      it->adapter = adap;
      it->lastest_action_tp = std::chrono::steady_clock::now();
      return adap;
    }
    return adap;
  }

  bool do_read(std::shared_ptr<Adapter> &adap) {
    bool r = SocketPoll::do_read(adap);
    auto it = active_adapters.begin();
    while (it != active_adapters.end()) {
      if (it->adapter == adap)
        break;
      ++it;
    }
    if (it != active_adapters.end()) {
      if (r) {
        // move adapter to end of list
        // adapter sorted by lastest_action_tp
        it->lastest_action_tp = std::chrono::steady_clock::now();
        if (it != active_adapters.rbegin().base())
          active_adapters.splice(active_adapters.end(), active_adapters, it);
      } else {
        active_adapters.erase(it);
      }
    }
    return r;
  }

  void obtain_timeout(std::chrono::steady_clock::time_point &nowtp,
                      struct timeval *tv) {
    auto dur = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::milliseconds(options.beep_timeout) -
        (nowtp - active_adapters.front().lastest_action_tp));
    tv->tv_sec = dur.count() / 1000;
    tv->tv_usec = (dur.count() % 1000) * 1000;
  }

  void shutdown_timeout_adapter(std::chrono::steady_clock::time_point &nowtp) {
#ifdef FLORA_DEBUG
    std::chrono::steady_clock::time_point dnowtp = nowtp;
    KLOGD(TAG, "before shutdown timeout adapters, timeout = %u",
          options.beep_timeout);
    print_active_adapters(dnowtp);
#endif
    nowtp -= std::chrono::milliseconds(options.beep_timeout);
    auto it = active_adapters.begin();
    while (it != active_adapters.end()) {
      if (nowtp >= it->lastest_action_tp) {
        int fd = std::static_pointer_cast<SocketAdapter>(it->adapter)->socket();
        KLOGW(TAG, "tcp socket %d keepalive timeout, shutdown!", fd);
        ::shutdown(fd, SHUT_RDWR);
        it = active_adapters.erase(it);
        continue;
      }
      break;
    }
#ifdef FLORA_DEBUG
    KLOGD(TAG, "after shutdown timeout adapters, timeout = %u",
          options.beep_timeout);
    print_active_adapters(dnowtp);
#endif
  }

#ifdef FLORA_DEBUG
  void print_active_adapters(std::chrono::steady_clock::time_point &nowtp) {
    auto it = active_adapters.begin();
    while (it != active_adapters.end()) {
      KLOGD(TAG, "%p: %d", it->adapter.get(),
            std::chrono::duration_cast<std::chrono::milliseconds>(
                nowtp - it->lastest_action_tp)
                .count());
      ++it;
    }
  }
#endif

private:
  class Options {
  public:
    uint32_t beep_timeout = 60000;
  };
  ActiveAdapterList active_adapters;
  Options options;
};

} // namespace internal
} // namespace flora
