#include <signal.h>
#include "cli.h"

#define EPOLL_MAX_EVENTS 4

namespace flora {

shared_ptr<Client> Client::newInstance() {
  auto ret = make_shared<ClientImpl>();
  ret->setSelfPtr(ret);
  return ret;
}

} // namespace flora

bool ReplyImpl::end(const Caps* data) {
  auto cli = client.lock();
  if (cli == nullptr) {
    ROKID_GERROR(CTAG, FLORA_CLI_ENOT_READY, "connection disconnected");
    return false;
  }
  return cli->replyCall(FLORA_CLI_SUCCESS, callid, data);
}

ClientLooper clientLooper;

ClientLooper::ClientLooper() {
  signal(SIGPIPE, SIG_IGN);
  // async call超时检测方法
  asyncCallRoutine = [this]() {
    AsyncCallInfoList::reverse_iterator it;
    steady_clock::time_point nowtp;
    unique_lock<mutex> locker(asyncCallMutex);
    while (!asyncCalls.empty()) {
      nowtp = steady_clock::now();
      it = asyncCalls.rbegin();
      while (it != asyncCalls.rend()) {
        if (nowtp >= it->tp) {
          auto cli = it->client.lock();
          if (cli != nullptr)
            cli->asyncCallTimeout(it->handle);
          auto n = asyncCalls.erase((++it).base());
          it = AsyncCallInfoList::reverse_iterator(n);
        } else {
          break;
        }
      }
      if (asyncCalls.empty())
        break;
      it = asyncCalls.rbegin();
      asyncCallChanged.wait_until(locker, it->tp);
    }
    asyncCallTaskRunning = 0;
  };
  // 连接flora服务并认证
  clientConnectRoutine = [this]() {
    unique_lock<mutex> locker(connMutex, defer_lock);
    while (true) {
      locker.lock();
      if (inactiveClients.empty()) {
        --connectingTasks;
        break;
      }
      auto wcli = inactiveClients.front();
      auto cli = wcli.lock();
      inactiveClients.pop_front();
      locker.unlock();
      if (cli != nullptr) {
        if (cli->connect() == 0)
          addPollClient(wcli);
        else {
          KLOGI(CTAG, "%s", ROKID_GERROR_STRING);
        }
      }
    }
  };
  // epoll线程
  clientPollRoutine = [this]() {
    if (!initEpoll()) {
      KLOGE(CTAG, "init epoll failed.");
      return;
    }

    struct epoll_event events[EPOLL_MAX_EVENTS];
    unique_lock<mutex> locker(pollMutex, defer_lock);
    while (true) {
      locker.lock();
      handleLoginedClients();
      if (pollClients.empty()) {
        locker.unlock();
        break;
      }
      locker.unlock();
      auto r = epoll_wait(epollfd, events, EPOLL_MAX_EVENTS, -1);
      if (r < 0) {
        if (errno == EINTR)
          continue;
        KLOGE(CTAG, "epoll_wait failed: %s", strerror(errno));
        break;
      }
      for (int32_t i = 0; i < r; ++i) {
        if (events[i].data.fd == rpipe) {
          char c;
          read(rpipe, &c, 1);
        } else {
          shared_ptr<ClientImpl> cli;
          locker.lock();
          // 将socket临时移出epoll，等数据处理完后再加回来
          // 防止数据处理过程中此fd再次出现epoll read事件
          // 这样adapter read可以不加锁
          epoll_ctl(epollfd, EPOLL_CTL_DEL, events[i].data.fd, nullptr);
          auto it = pollClients.find(events[i].data.fd);
          if (it != pollClients.end())
            cli = it->second.lock();
          locker.unlock();
          if (cli == nullptr)
            return;
          auto task = std::bind(clientReadRoutine, cli);
          thrPool.push(task);
        }
      }
    }

    closeEpoll();
    locker.lock();
    pollTaskRunning = false;
  };
  clientReadRoutine = [this](shared_ptr<ClientImpl> cli) {
    if (!cli->doReadAdapter()) {
      pollMutex.lock();
      pollClients.erase(cli->getSocket());
      pollMutex.unlock();
      cli->closeSocket();
      return;
    }
    // socket数据处理完，将其加回epoll
    pollMutex.lock();
    epoll_event ev;
    ev.events = EPOLLIN;
    ev.data.fd = cli->getSocket();
    epoll_ctl(epollfd, EPOLL_CTL_ADD, cli->getSocket(), &ev);
    pollMutex.unlock();
    wakeupPoll();
  };
  // 关闭Client
  closeClientRoutine = [this]() {
    shared_ptr<ClientImpl> cli;
    unique_lock<mutex> locker(pendingCloseMutex, defer_lock);
    while (true) {
      locker.lock();
      if (pendingClose.empty()) {
        closeTaskRunning = 0;
        break;
      }
      cli = pendingClose.front();
      pendingClose.pop_front();
      locker.unlock();
      cli->realClose();
      cli.reset();
    }
  };
}

ClientLooper::~ClientLooper() {
  // TODO:
  asyncCallMutex.lock();
  disableAsyncCallTask = 1;
  if (asyncCallTaskRunning) {
    asyncCalls.clear();
    asyncCallChanged.notify_one();
  }
  asyncCallMutex.unlock();

  connMutex.lock();
  disableConnectTask = 1;
  inactiveClients.clear();
  connMutex.unlock();

  pendingCloseMutex.lock();
  disableCloseTask = 1;
  pendingClose.clear();
  pendingCloseMutex.unlock();

  pollMutex.lock();
  loginedClients.clear();
  pollClients.clear();
  wakeupPoll();
  pollMutex.unlock();

  thrPool.finish();
}

void ClientLooper::handleLoginedClients() {
  auto it = loginedClients.begin();
  while (it != loginedClients.end()) {
    auto cli = it->lock();
    if (cli != nullptr) {
      epoll_event ev;
      ev.events = EPOLLIN;
      ev.data.fd = cli->getSocket();
      epoll_ctl(epollfd, EPOLL_CTL_ADD, cli->getSocket(), &ev);
      pollClients.insert(make_pair(cli->getSocket(), *it));
    }
    ++it;
  }
  loginedClients.clear();
}
