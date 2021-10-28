#pragma once

#include <map>
#include <chrono>
#include "common.h"
#include "adap.h"

#define MSGTYPE_INSTANT 0
#define MSGTYPE_PERSIST 1

using namespace std::chrono;

class MsgSubscription {
public:
  MsgSubscription(std::shared_ptr<ServiceAdapter>& a, uint32_t s)
    : id(s), adapter(a) {}

  uint32_t id;
  std::weak_ptr<ServiceAdapter> adapter;
};
typedef list<MsgSubscription> MsgSubscriptionList;
typedef map<std::string, MsgSubscriptionList> MsgSubscriptionMap;
typedef function<void(const Caps&)> ForEachStatusFunc;

class PendingCall {
public:
  weak_ptr<ServiceAdapter> caller;
  uint32_t reqid;
  uint32_t callid;
  steady_clock::time_point expireTime;

  PendingCall() {}

  PendingCall(shared_ptr<ServiceAdapter>& adap, uint32_t rid, uint32_t cid,
      steady_clock::time_point& tp) : caller{adap}, reqid{rid}, callid{cid},
      expireTime{tp} {}
};
typedef list<PendingCall> PendingCallList;

// name@target  method key
// name$target  status key
// name#0       instant subscribe key
// name#1       persist subscribe key
class AdapterManager {
public:
  void init(bool mthr, ThreadPool* tp) {
    working = true;
    multiThread = mthr;
    thrPool = tp;
    // pendingCalls超时计时
    thrPool->push([this]() {
      unique_lock<mutex> locker{pcMutex};
      while (working) {
        auto nowtp = steady_clock::now();
        auto it = pendingCalls.begin();
        // 超时后简单删除PendingCall对象，不向call请求客户端发送timeout消息
        // 因为客户端有自己的超时清理机制，不需要服务端发送timeout消息
        while (it != pendingCalls.end()) {
          if (nowtp >= it->expireTime)
            it = pendingCalls.erase(it);
          else
            break;
        }
        if (pendingCalls.empty()) {
          pcCond.wait(locker);
        } else {
          pcCond.wait_until(locker, pendingCalls.begin()->expireTime);
        }
      }
    });
  }

  void close() {
    lock_guard<mutex> locker(pcMutex);
    working = false;
    pcCond.notify_one();
  }

  shared_ptr<ServiceAdapter> newAdapter(FixedBufferManager* bm, int fd, int type) {
    if (multiThread)
      amMutex.lock();
    auto adap = make_shared<ServiceSocketAdapter>(bm, fd, type);
#ifdef NDEBUG
    adapters.insert(make_pair(fd, adap));
#else
    auto r = adapters.insert(make_pair(fd, adap));
    assert(r.second);
#endif
    if (multiThread)
      amMutex.unlock();
    return adap;
  }

  void eraseAdapter(int fd) {
    if (multiThread)
      amMutex.lock();
    adapters.erase(fd);
    monitorAdapters.erase(fd);
    if (multiThread)
      amMutex.unlock();
  }

  shared_ptr<ServiceAdapter> get(int fd) {
    unique_lock<mutex> locker(amMutex, defer_lock);
    if (multiThread)
      locker.lock();
    auto it = adapters.find(fd);
    if (it == adapters.end())
      return nullptr;
    return it->second;
  }

  bool setAdapterName(shared_ptr<ServiceAdapter>& adap, const string& name) {
    unique_lock<mutex> locker(amMutex, defer_lock);
    if (multiThread)
      locker.lock();
    auto it = namedAdapters.find(name);
    if (it == namedAdapters.end()) {
      namedAdapters.insert(make_pair(name, adap));
      adap->name = name;
      return true;
    }
    if (it->second.lock() == nullptr) {
      it->second = adap;
      adap->name = name;
      return true;
    }
    return false;
  }

  void setReady(shared_ptr<ServiceAdapter>& adap) {
    if (multiThread)
      amMutex.lock();
    TagHelper::setReady(adap->tag);
    if (multiThread)
      amMutex.unlock();
  }

  void getNamedAdapters(vector<shared_ptr<ServiceAdapter>>& out) {
    if (multiThread)
      amMutex.lock();
    out.clear();
    auto it = namedAdapters.begin();
    while (it != namedAdapters.end()) {
      auto adap = it->second.lock();
      if (adap == nullptr) {
        it = namedAdapters.erase(it);
        continue;
      }
      out.push_back(adap);
      ++it;
    }
    if (multiThread)
      amMutex.unlock();
  }

  void getReadyAdapters(vector<shared_ptr<ServiceAdapter>>& out) {
    if (multiThread)
      amMutex.lock();
    out.clear();
    auto it = namedAdapters.begin();
    while (it != namedAdapters.end()) {
      auto adap = it->second.lock();
      if (adap == nullptr) {
        it = namedAdapters.erase(it);
        continue;
      }
      if (TagHelper::ready(adap->tag))
        out.push_back(adap);
      ++it;
    }
    if (multiThread)
      amMutex.unlock();
  }

  void getMonitorAdapters(vector<shared_ptr<ServiceAdapter>>& out) {
    if (multiThread)
      amMutex.lock();
    out.clear();
    auto it = monitorAdapters.begin();
    while (it != monitorAdapters.end()) {
      out.push_back(it->second);
      ++it;
    }
    if (multiThread)
      amMutex.unlock();
  }

  void subscribeMsg(shared_ptr<ServiceAdapter>& adap, const string& name,
      uint32_t subid, uint32_t type) {
    string key;
    makeSubKey(name, type, key);
    addSubscription(key, adap, subid);
  }

  void unsubscribeMsg(shared_ptr<ServiceAdapter>& adap, const string& name,
      uint32_t subid, uint32_t type) {
    string key;
    makeSubKey(name, type, key);
    eraseSubscription(key, adap, subid);
  }

  bool findSubscription(const string& name, uint32_t type,
      MsgSubscriptionList& res) {
    string key;
    makeSubKey(name, type, key);
    return findSubscription(key, res);
  }

  void subscribeStatus(shared_ptr<ServiceAdapter>& adap, const string& name,
      const string& target, uint32_t subid) {
    string key;
    makeSubKey(name, target, '$', key);
    addSubscription(key, adap, subid);
  }

  void unsubscribeStatus(shared_ptr<ServiceAdapter>& adap, const string& name,
      const string& target, uint32_t subid) {
    string key;
    makeSubKey(name, target, '$', key);
    eraseSubscription(key, adap, subid);
  }

  bool findSubscription(const string& name, const string& target,
      MsgSubscriptionList& res) {
    string key;
    makeSubKey(name, target, '$', key);
    return findSubscription(key, res);
  }

  bool declareMethod(shared_ptr<ServiceAdapter>& adap, const string& name,
      uint32_t methid) {
    string key;
    makeSubKey(name, adap->name, '@', key);
    unique_lock<mutex> locker(amMutex, defer_lock);
    if (multiThread)
      locker.lock();
    auto it = subscriptions.find(key);
    if (it == subscriptions.end()) {
      auto r = subscriptions.emplace(key, MsgSubscriptionList{});
      it = r.first;
    }
    if (!it->second.empty()) {
      if (it->second.front().adapter.lock() != nullptr)
        return false;
      it->second.clear();
    }
    it->second.emplace_back(adap, methid);
    return true;
  }

  void removeMethod(shared_ptr<ServiceAdapter>& adap, const string& name) {
    string key;
    makeSubKey(name, adap->name, '@', key);
    if (multiThread)
      amMutex.lock();
    subscriptions.erase(key);
    if (multiThread)
      amMutex.unlock();
  }

  shared_ptr<ServiceAdapter> findMethodTarget(const string& name,
      const string& target, uint32_t& methid) {
    string key;
    makeSubKey(name, target, '@', key);
    auto it = subscriptions.find(key);
    if (it == subscriptions.end())
      return nullptr;
    auto res = it->second.front().adapter.lock();
    if (res == nullptr)
      subscriptions.erase(it);
    else
      methid = it->second.front().id;
    return res;
  }

  uint32_t addPendingCall(shared_ptr<ServiceAdapter>& caller, uint32_t reqid,
      uint32_t timeout) {
    auto tp = steady_clock::now() + milliseconds(timeout + 1);
    lock_guard<mutex> locker{pcMutex};
    auto it = pendingCalls.begin();
    while (it != pendingCalls.end()) {
      if (tp <= it->expireTime)
        break;
      ++it;
    }
    auto callid = ++callIdSeq;
    it = pendingCalls.emplace(it, caller, reqid, callid, tp);
    // 如果插入的pending call是最先超时的，则通知超时计时线程
    if (it == pendingCalls.begin())
      pcCond.notify_one();
    return callid;
  }

  bool findPendingCall(uint32_t callid, PendingCall& res, bool remove) {
    lock_guard<mutex> locker{pcMutex};
    auto it = pendingCalls.begin();
    while (it != pendingCalls.end()) {
      if (it->callid == callid) {
        res = *it;
        if (remove)
          pendingCalls.erase(it);
        return true;
      }
      ++it;
    }
    return false;
  }

  bool getStatus(const string& name, const string& target, Caps& res) {
    string key;
    makeSubKey(name, target, '$', key);
    return getMsgValue(key, res);
  }

  void updateStatus(shared_ptr<ServiceAdapter>& adap, const string& name,
      Caps& value) {
    string key;
    makeSubKey(name, adap->name, '$', key);
    updateMsgValue(key, value);
  }

  void deleteStatus(shared_ptr<ServiceAdapter>& adap, const string& name) {
    string key;
    makeSubKey(name, adap->name, '$', key);
    deleteMsgValue(key);
  }

  void forEachStatus(shared_ptr<ServiceAdapter>& adap, ForEachStatusFunc fn,
      bool del) {
    string suffix = "$";
    suffix += adap->name;
    auto it = msgValues.begin();
    while (it != msgValues.end()) {
      auto len = it->first.length();
      if (len > suffix.length()) {
        auto start = len - suffix.length();
        if (memcmp(it->first.c_str() + start, suffix.c_str(),
              suffix.length()) == 0) {
          fn(it->second);
          if (del) {
            it = msgValues.erase(it);
            continue;
          }
        }
      }
      ++it;
    }
  }

  bool getPersistValue(const string& name, Caps& res) {
    string key;
    makeSubKey(name, MSGTYPE_PERSIST, key);
    return getMsgValue(key, res);
  }

  void updatePersistValue(const string& name, Caps& value) {
    string key;
    makeSubKey(name, MSGTYPE_PERSIST, key);
    updateMsgValue(key, value);
  }

  void deletePersistValue(const string& name) {
    string key;
    makeSubKey(name, MSGTYPE_PERSIST, key);
    deleteMsgValue(key);
  }

  uint32_t getAdapterNum() const {
    unique_lock<mutex> locker(amMutex, defer_lock);
    if (multiThread)
      locker.lock();
    return adapters.size();
  }

  void clearAdapters() {
    if (multiThread)
      amMutex.lock();
    adapters.clear();
    monitorAdapters.clear();
    if (multiThread)
      amMutex.unlock();
  }

  void addMonitorAdapter(shared_ptr<ServiceAdapter>& adap) {
    if (multiThread)
      amMutex.lock();
    auto fd = static_cast<ServiceSocketAdapter*>(adap.get())->getSocket();
    monitorAdapters.insert(make_pair(fd, adap));
    if (multiThread)
      amMutex.unlock();
  }

private:
  void makeSubKey(const string& name, uint32_t type, string& res) {
    res = name;
    char buf[3];
    sprintf(buf, "#%u", type);
    res += buf;
  }

  void makeSubKey(const string& name, const string& target, char c,
      string& res) {
    res = name;
    res += c;
    res += target;
  }

  void addSubscription(const string& key, shared_ptr<ServiceAdapter>& adap,
      uint32_t subid) {
    if (multiThread)
      amMutex.lock();
    auto it = subscriptions.find(key);
    if (it == subscriptions.end()) {
      auto r = subscriptions.emplace(key, MsgSubscriptionList{});
      it = r.first;
    }
    it->second.emplace_back(adap, subid);
    if (multiThread)
      amMutex.unlock();
  }

  void eraseSubscription(const string& key, shared_ptr<ServiceAdapter>& adap,
      uint32_t subid) {
    if (multiThread)
      amMutex.lock();
    auto it = subscriptions.find(key);
    if (it != subscriptions.end()) {
      auto sit = it->second.begin();
      while (sit != it->second.end()) {
        if (sit->id == subid) {
          auto a = sit->adapter.lock();
          if (a == nullptr || a == adap) {
            it->second.erase(sit);
            break;
          }
        }
        ++sit;
      }
      if (it->second.empty())
        subscriptions.erase(it);
    }
    if (multiThread)
      amMutex.unlock();
  }

  bool findSubscription(const string& key, MsgSubscriptionList& res) {
    unique_lock<mutex> locker(amMutex, defer_lock);
    if (multiThread)
      locker.lock();
    auto it = subscriptions.find(key);
    if (it == subscriptions.end())
      return false;
    res = it->second;
    return true;
  }

  bool getMsgValue(const string& key, Caps& res) {
    unique_lock<mutex> locker(amMutex, defer_lock);
    if (multiThread)
      locker.lock();
    auto it = msgValues.find(key);
    if (it == msgValues.end())
      return false;
    res = it->second;
    return true;
  }

  void updateMsgValue(const string& key, Caps& value) {
    unique_lock<mutex> locker(amMutex, defer_lock);
    if (multiThread)
      locker.lock();
    auto it = msgValues.find(key);
    if (it == msgValues.end()) {
      msgValues.insert(make_pair(key, move(value)));
      return;
    }
    it->second = move(value);
  }

  void deleteMsgValue(const string& key) {
    if (multiThread)
      amMutex.lock();
    auto it = msgValues.find(key);
    if (it != msgValues.end())
      msgValues.erase(it);
    if (multiThread)
      amMutex.unlock();
  }

private:
  typedef std::map<int32_t, std::shared_ptr<ServiceAdapter>> AdapterMap;
  typedef std::map<std::string, std::weak_ptr<ServiceAdapter>> NamedAdapterMap;
  typedef map<string, Caps> MsgValueMap;

  bool multiThread;
  bool working = false;
  mutable mutex amMutex;
  // pending call list mutex & cond
  mutex pcMutex;
  condition_variable pcCond;
  AdapterMap adapters;
  NamedAdapterMap namedAdapters;
  AdapterMap monitorAdapters;
  MsgSubscriptionMap subscriptions;
  // value of persist msgs and status msgs
  MsgValueMap msgValues;
  PendingCallList pendingCalls;
  uint32_t callIdSeq{0};
  ThreadPool* thrPool = nullptr;
};
