#pragma once

#include <sys/epoll.h>
#include <unistd.h>
#include "thr-pool.h"
#include "common.h"

// 保留3个线程
// 1 用于异步call的超时判断
// 2 用于所有client sockets epoll
// 3 用于处理等待关闭的clients队列
#define MAX_THREAD_NUM 8
#define MAX_CONNECT_THREAD_NUM 2

class ClientImpl;
class AsyncCallInfo {
public:
  AsyncCallInfo(weak_ptr<ClientImpl>& cli, uint32_t h,
      steady_clock::time_point& t) : client{cli}, handle{h}, tp{t} {
  }

  weak_ptr<ClientImpl> client;
  uint32_t handle;
  steady_clock::time_point tp;
};
typedef list<AsyncCallInfo> AsyncCallInfoList;
typedef deque<weak_ptr<ClientImpl>> ClientQueue;
typedef map<int32_t, weak_ptr<ClientImpl>> SocketClientMap;
typedef deque<shared_ptr<ClientImpl>> ClientQueue2;

// 从事以下工作
// client socket epoll (macos不支持)
// client callAsync 超时判断
class ClientLooper {
public:
  ClientLooper();
  ~ClientLooper();

  // async call信息放入链表，按超时时间从大到小排序
  // 启动线程进行超时检测
  void addAsyncCall(weak_ptr<ClientImpl>& cli, uint32_t handle,
      uint32_t timeout) {
    auto tp = steady_clock::now() + milliseconds{timeout + 1};
    lock_guard<mutex> locker(asyncCallMutex);
    if (disableAsyncCallTask)
      return;
    auto it = asyncCalls.begin();
    while (it != asyncCalls.end()) {
      if (tp >= it->tp)
        break;
      ++it;
    }
    asyncCalls.emplace(it, cli, handle, tp);
    if (asyncCallTaskRunning) {
      asyncCallChanged.notify_one();
    } else {
      thrPool.push(asyncCallRoutine);
      asyncCallTaskRunning = 1;
    }
  }

  void eraseAsyncCall(uint32_t handle) {
    lock_guard<mutex> locker(asyncCallMutex);
    auto it = asyncCalls.begin();
    while (it != asyncCalls.end()) {
      if (handle == it->handle) {
        asyncCalls.erase(it);
        break;
      }
      ++it;
    }
  }

  void addClient(weak_ptr<ClientImpl>& cli) {
    lock_guard<mutex> locker(connMutex);
    inactiveClients.push_back(cli);
    if (connectingTasks < MAX_CONNECT_THREAD_NUM) {
      thrPool.push(clientConnectRoutine);
      ++connectingTasks;
    }
  }

  void addToPendingClose(weak_ptr<ClientImpl>& wcli) {
    auto cli = wcli.lock();
    if (cli != nullptr) {
      lock_guard<mutex> locker(pendingCloseMutex);
      pendingClose.push_back(cli);
      if (closeTaskRunning == 0) {
        closeTaskRunning = 1;
        thrPool.push(closeClientRoutine);
      }
    }
  }

private:
  void addPollClient(weak_ptr<ClientImpl>& cli) {
    lock_guard<mutex> locker(pollMutex);
    loginedClients.push_back(cli);
    if (pollTaskRunning) {
      wakeupPoll();
    } else {
      pollTaskRunning = true;
      thrPool.push(clientPollRoutine);
    }
  }

  bool initEpoll() {
    epollfd = epoll_create1(EPOLL_CLOEXEC);
    if (epollfd < 0)
      return false;
    int pipefds[2] = {-1, -1};
    if (pipe2(pipefds, O_CLOEXEC)) {
      ::close(epollfd);
      epollfd = -1;
      return false;
    }
    pipeMutex.lock();
    rpipe = pipefds[0];
    wpipe = pipefds[1];
    pipeMutex.unlock();
    struct epoll_event ev;
    ev.events = EPOLLIN;
    ev.data.fd = rpipe;
    epoll_ctl(epollfd, EPOLL_CTL_ADD, rpipe, &ev);
    return true;
  }

  void closeEpoll() {
    pipeMutex.lock();
    if (rpipe >= 0) {
      ::close(rpipe);
      rpipe = -1;
    }
    if (wpipe >= 0) {
      ::close(wpipe);
      wpipe = -1;
    }
    pipeMutex.unlock();
    if (epollfd >= 0) {
      ::close(epollfd);
      epollfd = -1;
    }
  }

  void wakeupPoll() {
    lock_guard<mutex> locker(pipeMutex);
    if (wpipe >= 0) {
      char c{'a'};
      write(wpipe, &c, 1);
    }
  }

  void handleLoginedClients();

  void erasePollClient(int32_t sockfd) {
    pollClients.erase(sockfd);
    epoll_ctl(epollfd, EPOLL_CTL_DEL, sockfd, nullptr);
  }

private:
  ThreadPool thrPool{MAX_THREAD_NUM};
  function<void()> asyncCallRoutine;
  uint8_t asyncCallTaskRunning = 0;
  uint8_t pollTaskRunning = 0;
  uint8_t closeTaskRunning = 0;
  uint8_t connectingTasks = 0;
  uint8_t disableAsyncCallTask = 0;
  uint8_t disableConnectTask = 0;
  uint8_t disableCloseTask = 0;
  uint8_t reserved;
  mutex asyncCallMutex;
  condition_variable asyncCallChanged;
  AsyncCallInfoList asyncCalls;
  mutex connMutex;
  ClientQueue inactiveClients;
  function<void()> clientConnectRoutine;
  // 连接并登录的client临时放在此队列
  // 等待被加入epoll
  ClientQueue loginedClients;
  // 加入了epoll的clients，从loginedClients移除，进入pollClients
  SocketClientMap pollClients;
  mutex pollMutex;
  function<void()> clientPollRoutine;
  int32_t epollfd = -1;
  // 一对管道fd，用于唤醒epoll_wait，进行增加client socket操作
  int32_t rpipe = -1;
  int32_t wpipe = -1;
  mutex pipeMutex;
  function<void()> closeClientRoutine;
  mutex pendingCloseMutex;
  ClientQueue2 pendingClose;
  function<void(shared_ptr<ClientImpl>)> clientReadRoutine;
};
