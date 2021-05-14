#pragma once

#include <errno.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <netinet/in.h>
#include <netdb.h>
#include <fcntl.h>
#include <unistd.h>
#ifdef FLORA_USE_EPOLL
#include <sys/epoll.h>
#include <set>
#else
#include <poll.h>
#endif
#include "flora-svc.h"
#include "flora-defs.h"
#include "rlog.h"
#include "thr-pool.h"
#include "uri.h"
#include "msg-handler.h"

#ifdef FLORA_USE_EPOLL
#define MAX_EPOLL_EVENTS 128
#endif

#define SERVICE_STATUS_STOPPED 0
#define SERVICE_STATUS_RUNNING 1

class ServiceImpl : public Service {
private:
  class Options {
  public:
    vector<string> uris;
    uint32_t bufsize{DEFAULT_BUFSIZE};
    uint32_t readThreadNum{1};
    uint32_t writeThreadNum{1};
  };

  class ReadTaskStatus {
  public:
    uint32_t taskNum{0};
    uint32_t taskDone{0};
    mutex valMutex;
    condition_variable allTasksDone;
  };

public:
  void config(ServiceOptions opt, va_list ap) {
    switch (opt) {
      case ServiceOptions::LISTEN_URI: {
        auto s = va_arg(ap, const char*);
        options.uris.emplace_back(s);
        break;
      }
      case ServiceOptions::BUFSIZE:
        options.bufsize = va_arg(ap, uint32_t);
        if (options.bufsize < MIN_BUFSIZE)
          options.bufsize = MIN_BUFSIZE;
        break;
      case ServiceOptions::READ_THREAD_NUM:
        options.readThreadNum = va_arg(ap, uint32_t);
        break;
      case ServiceOptions::WRITE_THREAD_NUM:
        options.writeThreadNum = va_arg(ap, uint32_t);
        break;
    }
  }

  bool start(bool blocking) {
    // 阻塞模式时，当前线程算一个读线程
    // 再加一个pendingCall超时计时线程
    uint32_t threadNum = (options.readThreadNum > 1 ? options.readThreadNum : 0)
          + options.writeThreadNum + 1;
    if (!blocking)
      ++threadNum;
    thrPool.init(threadNum);
    if (!init())
      return false;
    status = SERVICE_STATUS_RUNNING;
    if (blocking) {
      run();
      return true;
    }
    thrPool.push([this]() { run(); });
    return true;
  }

  void stop() {
    status = SERVICE_STATUS_STOPPED;
#ifdef FLORA_USE_EPOLL
    auto it = sockets.begin();
    ::shutdown(*it, SHUT_RDWR);
#else
    wakeupPoll();
#endif
  }

private:
  bool init() {
    if (!startListening())
      return false;
    // 读线程数大于1，需要保证线程安全
    adapterManager.init(options.readThreadNum > 1, &thrPool);
    msgHandler.init(adapterManager, msgWriter);
    bufferManager.init(options.bufsize, options.readThreadNum > 1);
    msgWriter.init(options.writeThreadNum, &bufferManager, &adapterManager);
    return true;
  }

  bool startListening() {
    if (options.uris.empty()) {
      ROKID_GERROR(STAG, FLORA_SVC_EINVAL, "no listening uri");
      return false;
    }
#ifdef FLORA_USE_EPOLL
    epollfd = epoll_create1(EPOLL_CLOEXEC);
    if (epollfd < 0) {
      ROKID_GERROR(STAG, FLORA_SVC_ESYS, "epoll_create failed: %s",
          strerror(errno));
      return false;
    }
#else
    // 在非poll线程shutdown/close socket，poll不一定会返回，不同操作系统行为不一样
    // 所以需要在poll队列中加入一个特殊的socket，用于在stop时唤醒poll线程
    // epoll没有这种问题
    addWakeupPollSocket();
#endif
    Uri urip;
    auto it = options.uris.begin();
    while (it != options.uris.end()) {
      if (!urip.parse(it->c_str())) {
        KLOGW(STAG, "uri %s parse failed", it->c_str());
        ++it;
        continue;
      }
      if (urip.scheme == "unix") {
        if (!listenUnix(urip))
          goto failed;
      } else if(urip.scheme == "tcp") {
        if (!listenTcp(urip))
          goto failed;
      } else {
        KLOGW(STAG, "uri %s's scheme is invalid", it->c_str());
        ++it;
        continue;
      }
      KLOGD(STAG, "flora service listening %s", it->c_str());
      ++it;
    }
    return true;

failed:
    closeSockets();
    return false;
  }

  bool listenUnix(Uri& urip) {
#ifdef __APPLE__
    int fd = socket(AF_UNIX, SOCK_STREAM, 0);
#else
    int fd = socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0);
#endif
    if (fd < 0) {
      ROKID_GERROR(STAG, FLORA_SVC_ESYS, "socket create failed: %s",
          strerror(errno));
      return false;
    }
#ifdef __APPLE__
    auto f = fcntl(fd, F_GETFD);
    f |= FD_CLOEXEC;
    fcntl(fd, F_SETFD, f);
#endif
    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    uint32_t abslen = urip.path.length() + 1;
    if (abslen > sizeof(addr.sun_path))
      abslen = sizeof(addr.sun_path);
#ifdef __APPLE__
    unlink(urip.path.c_str());
    strncpy(addr.sun_path, urip.path.c_str(), abslen);
#else
    addr.sun_path[0] = '\0';
    memcpy(addr.sun_path + 1, urip.path.data(), abslen - 1);
#endif
    abslen += offsetof(sockaddr_un, sun_path);
    if (::bind(fd, (sockaddr *)&addr, abslen) < 0) {
      ::close(fd);
      ROKID_GERROR(STAG, FLORA_SVC_ESYS, "socket bind failed: %s",
          strerror(errno));
      printf("%s\n", GlobalError::msg());
      return false;
    }
    listen(fd, 10);
    addSocket(fd, SOCKET_TYPE_LISTEN);
    return true;
  }

  bool listenTcp(Uri& urip) {
#ifdef __APPLE__
    int fd = socket(AF_INET, SOCK_STREAM, 0);
#else
    int fd = socket(AF_INET, SOCK_STREAM | SOCK_CLOEXEC, 0);
#endif
    if (fd < 0) {
      ROKID_GERROR(STAG, FLORA_SVC_ESYS, "socket create failed: %s",
          strerror(errno));
      return false;
    }
#ifdef __APPLE__
    auto f = fcntl(fd, F_GETFD);
    f |= FD_CLOEXEC;
    fcntl(fd, F_SETFD, f);
#endif
    int v = 1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &v, sizeof(v));
    struct sockaddr_in addr;
    struct hostent *hp;
    hp = gethostbyname(urip.host.c_str());
    if (hp == nullptr) {
      ROKID_GERROR(STAG, FLORA_SVC_ESYS,
          "gethostbyname failed for host %s: %s",
          urip.host.c_str(), strerror(errno));
      ::close(fd);
      return false;
    }
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    memcpy(&addr.sin_addr, hp->h_addr_list[0], sizeof(addr.sin_addr));
    addr.sin_port = htons(urip.port);
    if (::bind(fd, (sockaddr *)&addr, sizeof(addr)) < 0) {
      ::close(fd);
      ROKID_GERROR(STAG, FLORA_SVC_ESYS, "socket bind failed: %s",
          strerror(errno));
      return false;
    }
    listen(fd, 10);
    addSocket(fd, SOCKET_TYPE_TCP | SOCKET_TYPE_LISTEN);
    return true;
  }

  void addSocket(int fd, int type) {
    if (options.readThreadNum > 1)
      readMutex.lock();
#ifdef FLORA_USE_EPOLL
    struct epoll_event ev;
    ev.events = EPOLLIN;
    ev.data.u64 = makeEPollData(fd, type);
    epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &ev);
    sockets.insert(fd);
#else
    struct pollfd tmp;
    tmp.fd = fd;
    tmp.events = POLLIN;
    tmp.revents = 0;
    allfds.push_back(tmp);
    sockets.insert(make_pair(fd, type));
#endif
    if (options.readThreadNum > 1)
      readMutex.unlock();
  }

  uint64_t makeEPollData(int32_t fd, uint32_t type) {
    return ((uint64_t)type << 32) | (uint32_t)fd;
  }

  int32_t fdOfEPollData(uint64_t data) {
    return (int32_t)((data << 32) >> 32);
  }

  uint32_t typeOfEPollData(uint64_t data) {
    return (uint32_t)(data >> 32);
  }

  void closeSockets() {
#ifdef FLORA_USE_EPOLL
    ::close(epollfd);
    epollfd = -1;
    auto it = sockets.begin();
    while (it != sockets.end()) {
      ::close(*it);
      ++it;
    }
    sockets.clear();
#else
    auto it = sockets.begin();
    while (it != sockets.end()) {
      ::close(it->first);
      ++it;
    }
    sockets.clear();
#endif
  }

  void run() {
    while (status == SERVICE_STATUS_RUNNING) {
      if (!doPoll()) {
        printGlobalError();
        break;
      }
      doReadAccept();
    }
    closeSockets();
    adapterManager.clearAdapters();
    msgWriter.finish();
  }

  bool doPoll() {
    while (true) {
#ifdef FLORA_USE_EPOLL
      auto r = epoll_wait(epollfd, epollEvents, MAX_EPOLL_EVENTS, -1);
      polloutNum = r;
#else
      auto r = poll(allfds.data(), allfds.size(), -1);
#endif
      if (r < 0) {
        if (errno == EINTR)
          continue;
        if (errno == EAGAIN) {
          sleep(1);
          continue;
        }
        ROKID_GERROR(STAG, FLORA_SVC_ESYS, "select failed: %s",
            strerror(errno));
        return false;
      }
      break;
    }
    return true;
  }

  void doReadAccept() {
    readTaskStatus.taskNum = 0;
    readTaskStatus.taskDone = 0;
#ifdef FLORA_USE_EPOLL
    for (int32_t i = 0; i < polloutNum; ++i) {
      auto fd = fdOfEPollData(epollEvents[i].data.u64);
      auto type = typeOfEPollData(epollEvents[i].data.u64);
      if (options.readThreadNum > 1) {
        ++readTaskStatus.taskNum;
        thrPool.push(bind(&ServiceImpl::doReadTask, this, fd, type));
      } else {
        doReadAccept(fd, type);
      }
    }
#else
    vector<struct pollfd> rfds;
    auto it = allfds.begin();
    while (it != allfds.end()) {
      if (it->revents)
        rfds.push_back(*it);
      ++it;
    }
    it = rfds.begin();
    map<int, int>::iterator sit;
    while (it != rfds.end()) {
      sit = sockets.find(it->fd);
      assert(sit != sockets.end());
      if (options.readThreadNum > 1) {
        ++readTaskStatus.taskNum;
        thrPool.push(bind(&ServiceImpl::doReadTask, this, sit->first,
              sit->second));
      } else {
        doReadAccept(sit->first, sit->second);
      }
      ++it;
    }
#endif
    if (options.readThreadNum > 1) {
      unique_lock<mutex> locker(readTaskStatus.valMutex);
      if (readTaskStatus.taskDone == readTaskStatus.taskNum)
        return;
      readTaskStatus.allTasksDone.wait(locker);
    }
  }

  void doReadAccept(int fd, int type) {
    if (type & SOCKET_TYPE_LISTEN) {
      if (!doAccept(fd, type))
        printGlobalError();
    } else {
      if (!doRead(fd, type))
        printGlobalError();
    }
  }

  void doReadTask(int fd, int type) {
    doReadAccept(fd, type);
    readTaskStatus.valMutex.lock();
    ++readTaskStatus.taskDone;
    if (readTaskStatus.taskDone == readTaskStatus.taskNum)
      readTaskStatus.allTasksDone.notify_one();
    readTaskStatus.valMutex.unlock();
  }

  bool doAccept(int fd, int type) {
    int newfd;
    uint64_t tag;

    if (type & SOCKET_TYPE_TCP)
      newfd = tcpAccept(fd, tag);
    else
      newfd = unixAccept(fd, tag);
    if (newfd < 0) {
      ROKID_GERROR(STAG, FLORA_SVC_ESYS, "accept failed: %s", strerror(errno));
      return false;
    }
    int tp = type & SOCKET_TYPE_TCP ? SOCKET_TYPE_TCP : 0;
    addSocket(newfd, tp);
    auto adap = adapterManager.newAdapter(&bufferManager, newfd, tp);
    msgWriter.bind(adap);
    KLOGD(STAG, "accept new connection %d", newfd);
    return true;
  }

  int unixAccept(int lfd, uint64_t& tag) {
    sockaddr_un addr;
    socklen_t addr_len = sizeof(addr);
#ifdef __APPLE__
    auto fd = accept(lfd, (sockaddr *)&addr, &addr_len);
    if (fd < 0)
      return fd;
    auto f = fcntl(fd, F_GETFD);
    f |= FD_CLOEXEC;
    fcntl(fd, F_SETFD, f);
    // there is no good way to get pid of client socket on macos
    tag = TagHelper::create(0);
#else
    auto fd = accept4(lfd, (sockaddr *)&addr, &addr_len, SOCK_CLOEXEC);
    if (fd < 0)
      return fd;
    struct ucred cred;
    socklen_t len = sizeof(cred);
    getsockopt(fd, SOL_SOCKET, SO_PEERCRED, &cred, &len);
    tag = TagHelper::create(cred.pid);
#endif
    return fd;
  }

  int tcpAccept(int lfd, uint64_t& tag) {
    sockaddr_in addr;
    socklen_t addr_len = sizeof(addr);
#ifdef __APPLE__
    auto fd = accept(lfd, (sockaddr *)&addr, &addr_len);
    if (fd < 0)
      return fd;
    auto f = fcntl(fd, F_GETFD);
    f |= FD_CLOEXEC;
    fcntl(fd, F_SETFD, f);
#else
    auto fd = accept4(lfd, (sockaddr *)&addr, &addr_len, SOCK_CLOEXEC);
    if (fd < 0)
      return fd;
#endif
    tag = TagHelper::create(addr);
    return fd;
  }

  bool doRead(int fd, int type) {
    auto adap = adapterManager.get(fd);
    assert(adap != nullptr);
    if (!adap->read()) {
      msgHandler.cleanStatus(adap);
      closeSocket(fd);
      return false;
    }
    Caps caps;
    int32_t r;
    do {
      r = adap->read(caps);
      if (r < 0) {
        msgHandler.cleanStatus(adap);
        closeSocket(fd);
        return false;
      }
      if (r == 1)
        break;
      if (!msgHandler.handle(adap, caps)) {
        msgHandler.cleanStatus(adap);
        closeSocket(fd);
        return false;
      }
    } while (r == 0);
    return true;
  }

  void closeSocket(int fd) {
    if (options.readThreadNum > 1)
      readMutex.lock();
#ifdef FLORA_USE_EPOLL
    epoll_ctl(epollfd, EPOLL_CTL_DEL, fd, nullptr);
    sockets.erase(fd);
#else
    auto it = allfds.begin();
    while (it != allfds.end()) {
      if (it->fd == fd) {
        allfds.erase(it);
        break;
      }
      ++it;
    }
    sockets.erase(fd);
#endif
    ::close(fd);
    if (options.readThreadNum > 1)
      readMutex.unlock();
    adapterManager.eraseAdapter(fd);
  }

#ifndef FLORA_USE_EPOLL
  void addWakeupPollSocket() {
    Uri urip;
    urip.parse(wakeupPollUri.c_str());
    listenUnix(urip);
  }

  void wakeupPoll() {
    ClientSocketAdapter adap{options.bufsize};
    Uri urip;
    urip.parse(wakeupPollUri.c_str());
    adap.connect(urip, 10);
  }
#endif

private:
  Options options;
  ThreadPool thrPool;
#ifdef FLORA_USE_EPOLL
  int epollfd{-1};
  struct epoll_event epollEvents[MAX_EPOLL_EVENTS];
  int32_t polloutNum{0};
  set<int32_t> sockets;
#else
  // key: fd, value: socketType
  map<int, int> sockets;
  vector<struct pollfd> allfds;
  string wakeupPollUri{"unix:flora-svc-special.sock"};
#endif
  ReadTaskStatus readTaskStatus;
  AdapterManager adapterManager;
  mutex readMutex;
  MsgHandler msgHandler;
  MsgWriter msgWriter;
  FixedBufferManager bufferManager;
  uint32_t status{SERVICE_STATUS_STOPPED};
};
