#pragma once

#include <assert.h>
#include "common.h"
#include "adap.h"
#include "rlog.h"
#include "cli-loop.h"

#define SUBHANDLE_MAGIC 0x10000000
#define CONNECT_TIMEOUT 10000
#define AUTH_TIMEOUT 10000
#define STATUS_RUNNING 1
#define STATUS_READY 2
#define STATUS_CLOSING 4

enum class CallbackType {
  INSTANT,
  PERSIST,
  STATUS,
  METHOD,
  RETURN,
  NUM_OF_TYPES
};

extern ClientLooper clientLooper;

template <uint32_t FSZ>
class CallbackSlotTemp {
public:
  string name;
  string target;
  uint32_t handle{0};
  char stdfunc[FSZ];

  CallbackSlotTemp() {}
  CallbackSlotTemp(const CallbackSlotTemp& o) {
    name = o.name;
    target = o.target;
    handle = o.handle;
    if (isValidSubHandle(handle)) {
      auto type = typeOfSubHandle(handle);
      switch ((CallbackType)type) {
      case CallbackType::INSTANT:
        new (stdfunc)InstantCallback(*(InstantCallback*)o.stdfunc);
        break;
      case CallbackType::PERSIST:
        new (stdfunc)PersistCallback(*(PersistCallback*)o.stdfunc);
        break;
      case CallbackType::STATUS:
        new (stdfunc)StatusCallback(*(StatusCallback*)o.stdfunc);
        break;
      case CallbackType::METHOD:
        new (stdfunc)MethodCallback(*(MethodCallback*)o.stdfunc);
        break;
      case CallbackType::RETURN:
        new (stdfunc)ReturnCallback(*(ReturnCallback*)o.stdfunc);
        break;
      default:
        assert(false);
        break;
      }
    }
  }
  ~CallbackSlotTemp() {
    destructFunc();
  }

  void destructFunc() {
    if (isValidSubHandle(handle)) {
      auto type = typeOfSubHandle(handle);
      switch ((CallbackType)type) {
      case CallbackType::INSTANT:
        ((InstantCallback*)stdfunc)->~InstantCallback();
        break;
      case CallbackType::PERSIST:
        ((PersistCallback*)stdfunc)->~PersistCallback();
        break;
      case CallbackType::STATUS:
        ((StatusCallback*)stdfunc)->~StatusCallback();
        break;
      case CallbackType::METHOD:
        ((MethodCallback*)stdfunc)->~MethodCallback();
        break;
      case CallbackType::RETURN:
        ((ReturnCallback*)stdfunc)->~ReturnCallback();
        break;
      default:
        assert(false);
        break;
      }
      handle = 0;
    }
  }

  static bool isValidSubHandle(uint32_t handle) {
    return handle & SUBHANDLE_MAGIC;
  }

  static uint32_t typeOfSubHandle(uint32_t handle) {
    return handle >> 29;
  }

  static uint32_t indexOfSubHandle(uint32_t handle) {
    return (handle << 16) >> 16;
  }
};
typedef CallbackSlotTemp<sizeof(function<void()>)> CallbackSlot;

class ClientImpl;
class ReplyImpl : public Reply {
public:
  ReplyImpl(uint32_t id, weak_ptr<ClientImpl>& cli) {
    callid = id;
    client = cli;
  }

  bool end(const Caps* data);

private:
  uint32_t callid;
  weak_ptr<ClientImpl> client;
};

class ClientImpl : public Client {
public:
  ClientImpl() {
    cmdHandlers[CMD_POST_RESP - MIN_RESP_CMD] = &ClientImpl::handlePost;
    cmdHandlers[CMD_CALL_RESP - MIN_RESP_CMD] = &ClientImpl::handleCall;
    cmdHandlers[CMD_CALL_FORWARD - MIN_RESP_CMD]
      = &ClientImpl::handleCallForward;
    cmdHandlers[CMD_MONITOR_RESP - MIN_RESP_CMD] = nullptr;
    cmdHandlers[CMD_PONG_RESP - MIN_RESP_CMD] = &ClientImpl::handlePong;
    cmdHandlers[CMD_UPDATE_STATUS_RESP - MIN_RESP_CMD]
      = &ClientImpl::handleUpdateStatus;
    cmdHandlers[CMD_DEL_STATUS_RESP - MIN_RESP_CMD]
      = &ClientImpl::handleDeleteStatus;
    cmdHandlers[CMD_UPDATE_PERSIST_RESP - MIN_RESP_CMD]
      = &ClientImpl::handleUpdatePersist;
    cmdHandlers[CMD_DEL_PERSIST_RESP - MIN_RESP_CMD]
      = &ClientImpl::handleDeletePersist;
    subscribeFuncs[(int32_t)CallbackType::INSTANT] = &ClientImpl::doSubscribeInstant;
    subscribeFuncs[(int32_t)CallbackType::PERSIST] = &ClientImpl::doSubscribePersist;
    subscribeFuncs[(int32_t)CallbackType::STATUS] = &ClientImpl::doSubscribeStatus;
    subscribeFuncs[(int32_t)CallbackType::METHOD] = &ClientImpl::doSubscribeMethod;
    subscribeFuncs[(int32_t)CallbackType::RETURN] = &ClientImpl::doEraseCallbackSlot;
    unsubscribeFuncs[(int32_t)CallbackType::INSTANT] = &ClientImpl::doUnsubscribeInstant;
    unsubscribeFuncs[(int32_t)CallbackType::PERSIST] = &ClientImpl::doUnsubscribePersist;
    unsubscribeFuncs[(int32_t)CallbackType::STATUS] = &ClientImpl::doUnsubscribeStatus;
    unsubscribeFuncs[(int32_t)CallbackType::METHOD] = &ClientImpl::doUnsubscribeMethod;
    unsubscribeFuncs[(int32_t)CallbackType::RETURN] = &ClientImpl::doNoop;
    callbackSlots.reserve(4);
  }

  void setSelfPtr(shared_ptr<ClientImpl>& ptr) {
    self = ptr;
  }

  void config(ClientOptions opt, va_list ap) {
    switch (opt) {
    case ClientOptions::URI:
      options.uri = va_arg(ap, const char*);
      break;
    case ClientOptions::ID:
      options.id = va_arg(ap, const char*);
      break;
    case ClientOptions::BUFSIZE:
      options.bufsize = va_arg(ap, uint32_t);
      if (options.bufsize < MIN_BUFSIZE)
        options.bufsize = MIN_BUFSIZE;
      break;
    case ClientOptions::SYNC:
      options.sync = va_arg(ap, uint32_t);
      break;
    }
  }

  sub_t subscribe(const std::string& name, InstantCallback cb) {
    if (!checkIdentify(name))
      return 0;
    auto idx = getEmptyCallbackSlot();
    if (idx < 0)
      idx = newCallbackSlot();
    auto handle = makeSubHandle(idx, CallbackType::INSTANT);
    callbackSlots[idx].handle = handle;
    callbackSlots[idx].name = name;
    new (callbackSlots[idx].stdfunc)InstantCallback(cb);
    // 如果已连接，则马上发送订阅请求
    unique_lock<mutex> locker(cliMutex);
    if (status & STATUS_READY)
      doSubscribe(callbackSlots[idx]);
    locker.unlock();
    return handle;
  }

  sub_t subscribe(const string& name, PersistCallback cb) {
    if (!checkIdentify(name))
      return 0;
    auto idx = getEmptyCallbackSlot();
    if (idx < 0)
      idx = newCallbackSlot();
    auto handle = makeSubHandle(idx, CallbackType::PERSIST);
    callbackSlots[idx].handle = handle;
    callbackSlots[idx].name = name;
    new (callbackSlots[idx].stdfunc)PersistCallback(cb);
    // 如果已连接，则马上发送订阅请求
    unique_lock<mutex> locker(cliMutex);
    if (status & STATUS_READY)
      doSubscribe(callbackSlots[idx]);
    locker.unlock();
    return handle;
  }

  sub_t subscribe(const string& name, const string& target,
      StatusCallback cb) {
    if (!checkIdentify(name))
      return 0;
    auto idx = getEmptyCallbackSlot();
    if (idx < 0)
      idx = newCallbackSlot();
    auto handle = makeSubHandle(idx, CallbackType::STATUS);
    callbackSlots[idx].handle = handle;
    callbackSlots[idx].name = name;
    callbackSlots[idx].target = target;
    new (callbackSlots[idx].stdfunc)StatusCallback(cb);
    // 如果已连接，则马上发送订阅请求
    unique_lock<mutex> locker(cliMutex);
    if (status & STATUS_READY)
      doSubscribe(callbackSlots[idx]);
    locker.unlock();
    return handle;
  }

  meth_t declareMethod(const string& name, MethodCallback cb) {
    if (!checkIdentify(name))
      return 0;
    auto r = declaredMethodNames.insert(name);
    if (!r.second) {
      ROKID_GERROR(CTAG, FLORA_CLI_EDUP, "method name %s duplicated",
          name.c_str());
      return 0;
    }
    if (options.id.empty()) {
      ROKID_GERROR(CTAG, FLORA_CLI_EPERMIT, "client id is empty, "
          "declare method not allowed", name.c_str());
      return 0;
    }
    auto idx = getEmptyCallbackSlot();
    if (idx < 0)
      idx = newCallbackSlot();
    auto handle = makeSubHandle(idx, CallbackType::METHOD);
    callbackSlots[idx].handle = handle;
    callbackSlots[idx].name = name;
    new (callbackSlots[idx].stdfunc)MethodCallback(cb);
    // 如果已连接，则马上发送订阅请求
    unique_lock<mutex> locker(cliMutex);
    if (status & STATUS_READY)
      doSubscribe(callbackSlots[idx]);
    locker.unlock();
    return handle;
  }

  void unsubscribe(sub_t handle) {
    eraseCallbackSlot(handle, true);
  }

  void removeMethod(meth_t handle) {
    auto idx = getHandleIndex(handle);
    if (idx >= 0) {
      declaredMethodNames.erase(callbackSlots[idx].name);
      eraseCallbackSlot(idx, CallbackSlot::typeOfSubHandle(handle), true);
    }
  }

  bool publish(const string& name, const Caps* msg) {
    lock_guard<mutex> locker(cliMutex);
    if (!checkReady())
      return false;
    if (!checkIdentify(name))
      return false;
    Caps req;
    req << CMD_POST_REQ;
    req << name;
    if (msg)
      req << *msg;
    return adapter->write(req);
  }

  bool updatePersist(const string& name, const Caps* msg) {
    lock_guard<mutex> locker(cliMutex);
    if (!checkReady())
      return false;
    if (!checkIdentify(name))
      return false;
    Caps req;
    req << CMD_UPDATE_PERSIST_REQ;
    req << name;
    if (msg)
      req << *msg;
    return adapter->write(req);
  }

  bool updateStatus(const string& name, const Caps* msg) {
    lock_guard<mutex> locker(cliMutex);
    if (!checkReady())
      return false;
    if (!checkIdentify(name))
      return false;
    Caps req;
    req << CMD_UPDATE_STATUS_REQ;
    req << name;
    if (msg)
      req << *msg;
    return adapter->write(req);
  }

  bool deleteStatus(const string& name) {
    lock_guard<mutex> locker(cliMutex);
    if (!checkReady())
      return false;
    if (!checkIdentify(name))
      return false;
    Caps req;
    req << CMD_DEL_STATUS_REQ;
    req << name;
    return adapter->write(req);
  }

  bool call(const string& name, const string& callee,
      const Caps* args, uint32_t timeout, Caps* ret) {
    unique_lock<mutex> cliLocker(cliMutex);
    if (!checkReady())
      return false;
    if (!checkIdentify(name) || !checkIdentify(callee))
      return false;
    mutex retMutex;
    condition_variable retCond;
    int32_t retCode{FLORA_CLI_ENOT_READY};
    auto cb = [&retMutex, &retCond, &retCode, &ret](int32_t code, const Caps& data) {
      lock_guard<mutex> locker(retMutex);
      retCode = code;
      if (ret)
        *ret = data;
      retCond.notify_one();
    };
    auto handle = setReturnCallback(cb);
    if (!sendCallReq(name, callee, args, timeout, handle)) {
      eraseCallbackSlot(handle, false);
      return false;
    }
    cliLocker.unlock();
    unique_lock<mutex> locker(retMutex);
    if (retCode == FLORA_CLI_ENOT_READY) {
      if (retCond.wait_for(locker, milliseconds(timeout))
          == cv_status::timeout) {
        retCode = FLORA_CLI_ETIMEOUT;
      }
    }
    eraseCallbackSlot(handle, false);
    if (retCode != FLORA_CLI_SUCCESS) {
      ROKID_GERROR(CTAG, retCode, "call %s@%s failed: %d",
          name.c_str(), callee.c_str(), retCode);
      return false;
    }
    return true;
  }

  bool callAsync(const string& name, const string& callee,
      const Caps* args, uint32_t timeout, ReturnCallback cb) {
    unique_lock<mutex> locker(cliMutex);
    if (!checkReady())
      return false;
    if (!checkIdentify(name) || !checkIdentify(callee))
      return false;
    auto handle = setReturnCallback(cb);
    if (!sendCallReq(name, callee, args, timeout, handle)) {
      eraseCallbackSlot(handle, false);
      return false;
    }
    locker.unlock();
    clientLooper.addAsyncCall(self, handle, timeout);
    return true;
  }

  bool deletePersist(const string& name) {
    lock_guard<mutex> locker(cliMutex);
    if (!checkReady())
      return false;
    if (!checkIdentify(name))
      return false;
    Caps req;
    req << CMD_DEL_PERSIST_REQ;
    req << name;
    return adapter->write(req);
  }

  bool replyCall(int32_t code, uint32_t id, const Caps* data) {
    if (!checkReady())
      return false;
    Caps req;
    req << CMD_CALL_RETURN;
    req << code;
    req << id;
    if (data)
      req << *data;
    return adapter->write(req);
  }

  bool open(ConnectionCallback cb) {
    connectionCallback = cb;
    if (options.sync) {
      auto r = connect();
      // error occurs
      if (r < 0)
        return false;
      // client closed
      if (r > 0)
        return true;
      while (doReadAdapter());
      closeSocket();
    } else {
      clientLooper.addClient(self);
    }
    return true;
  }

  void close() {
    clientLooper.addToPendingClose(self);
  }

  void realClose() {
    unique_lock<mutex> locker(cliMutex);
    if ((status & STATUS_RUNNING) && (status & STATUS_CLOSING) == 0) {
      status |= STATUS_CLOSING;
      adapter->shutdown();
    } else
      return;
    locker.unlock();
    while (status & STATUS_RUNNING)
      usleep(10000);
  }

  void asyncCallTimeout(uint32_t handle) {
    Caps empty;
    doCallReturn(handle, FLORA_CLI_ETIMEOUT, empty);
  }

  int32_t connect() {
    Uri urip;
    unique_lock<mutex> locker(cliMutex);
    assert(adapter == nullptr);
    // client.close，socket shutdown RD，
    // 需要销毁adapter, close socket，
    // 这样服务端可以清理数据。
    if (status & STATUS_CLOSING) {
      status = 0;
      locker.unlock();
      goto closed;
    }
    if (!urip.parse(options.uri.c_str())) {
      ROKID_GERROR(CTAG, FLORA_CLI_EINVAL, "uri %s is invalid",
          options.uri.c_str());
      locker.unlock();
      if (connectionCallback != nullptr)
        connectionCallback(FLORA_CLI_EINVAL);
      return -1;
    }
    adapter = make_shared<ClientSocketAdapter>(options.bufsize);
    status = STATUS_RUNNING;
    locker.unlock();
    KLOGI(CTAG, "connect to %s", options.uri.c_str());
    if (!adapter->connect(urip, CONNECT_TIMEOUT)) {
      locker.lock();
      adapter->close();
      adapter.reset();
      status = 0;
      locker.unlock();
      goto failed;
    }
    adapter->setReadTimeout(AUTH_TIMEOUT);
    if (!auth()) {
      locker.lock();
      adapter->close();
      adapter.reset();
      status = 0;
      locker.unlock();
      goto failed;
    }
    adapter->setReadTimeout(-1);
    locker.lock();
    if (status & STATUS_CLOSING) {
      adapter->close();
      adapter.reset();
      status = 0;
      locker.unlock();
      goto closed;
    }
    if (!doSubscribe()) {
      adapter->close();
      adapter.reset();
      status = 0;
      locker.unlock();
      goto failed;
    }
    status |= STATUS_READY;
    locker.unlock();
    if (connectionCallback != nullptr)
      connectionCallback(1);
    return 0;
failed:
    if (connectionCallback != nullptr)
      connectionCallback(GlobalError::code());
    return -1;
closed:
    if (connectionCallback != nullptr)
      connectionCallback(0);
    return 1;
  }

  bool doReadAdapter() {
    Caps caps;
    int32_t r;
    if (!adapter->read())
      goto disconn;
    do {
      r = adapter->read(caps);
      if (r == 1)
        break;
      if (r < 0 || !handleReadData(caps))
        goto disconn;
    } while (r == 0);
    KLOGD(CTAG, "client %s doReadAdapter success", options.id.c_str());
    return true;

disconn:
    if (connectionCallback != nullptr)
      connectionCallback(0);
    KLOGD(CTAG, "client %s doReadAdapter failed", options.id.c_str());
    KLOGD(CTAG, "%s", ROKID_GERROR_STRING);
    return false;
  }

  int32_t getSocket() {
    lock_guard<mutex> locker(cliMutex);
    if (adapter == nullptr) {
      return -1;
    }
    return static_pointer_cast<ClientSocketAdapter>(adapter)->getSocket();
  }

  void closeSocket() {
    lock_guard<mutex> locker(cliMutex);
    if (adapter != nullptr) {
      status = 0;
      adapter->close();
      adapter.reset();
    }
  }

private:
  int32_t getEmptyCallbackSlot() {
    auto it = callbackSlots.begin();
    int32_t idx{0};
    while (it != callbackSlots.end()) {
      if (it->handle == 0)
        return idx;
      ++idx;
      ++it;
    }
    return -1;
  }

  int32_t newCallbackSlot() {
    callbackSlots.emplace_back();
    return callbackSlots.size() - 1;
  }

  bool auth() {
    Caps req;
    req << CMD_AUTH_REQ;
    req << (uint32_t)FLORA_VERSION;
    if (!options.id.empty())
      req << options.id;
    if (!adapter->write(req))
      return false;
    if (!adapter->read())
      return false;
    Caps resp;
    auto r = adapter->read(resp);
    if (r)
      return false;
    auto it = resp.iterate();
    int32_t cmd;
    int32_t result;
    try {
      it >> cmd;
      assert(cmd == CMD_AUTH_RESP);
      it >> result;
    } catch (exception& e) {
      invalidCommand(resp);
      return false;
    }
    if (result) {
      ROKID_GERROR(STAG, result, "auth failed");
      return false;
    }
    return true;
  }

  bool doSubscribe() {
    for (size_t i = 0; i < callbackSlots.size(); ++i) {
      if (!doSubscribe(callbackSlots[i]))
        return false;
    }
    return true;
  }

  bool doSubscribe(CallbackSlot& slot) {
    if (CallbackSlot::isValidSubHandle(slot.handle)) {
      auto type = CallbackSlot::typeOfSubHandle(slot.handle);
      if (!(this->*(subscribeFuncs[type]))(slot))
        return false;
    }
    return true;
  }

  bool doSubscribeInstant(CallbackSlot& slot) {
    Caps req;
    req << CMD_SUBSCRIBE_REQ;
    req << slot.name;
    req << slot.handle;
    return adapter->write(req);
  }

  bool doSubscribePersist(CallbackSlot& slot) {
    Caps req;
    req << CMD_SUB_PERSIST_REQ;
    req << slot.name;
    req << slot.handle;
    return adapter->write(req);
  }

  bool doSubscribeStatus(CallbackSlot& slot) {
    Caps req;
    req << CMD_SUB_STATUS_REQ;
    req << slot.name;
    req << slot.target;
    req << slot.handle;
    return adapter->write(req);
  }

  bool doSubscribeMethod(CallbackSlot& slot) {
    Caps req;
    req << CMD_DECLARE_METHOD_REQ;
    req << slot.name;
    req << slot.handle;
    return adapter->write(req);
  }

  bool doEraseCallbackSlot(CallbackSlot& slot) {
    slot.destructFunc();
    return true;
  }

  void doUnsubscribeInstant(CallbackSlot& slot) {
    Caps req;
    req << CMD_UNSUBSCRIBE_REQ;
    req << slot.name;
    req << slot.handle;
    adapter->write(req);
  }

  void doUnsubscribePersist(CallbackSlot& slot) {
    Caps req;
    req << CMD_UNSUB_PERSIST_REQ;
    req << slot.name;
    req << slot.handle;
    adapter->write(req);
  }

  void doUnsubscribeStatus(CallbackSlot& slot) {
    Caps req;
    req << CMD_UNSUB_STATUS_REQ;
    req << slot.name;
    req << slot.target;
    req << slot.handle;
    adapter->write(req);
  }

  void doUnsubscribeMethod(CallbackSlot& slot) {
    Caps req;
    req << CMD_REMOVE_METHOD_REQ;
    req << slot.name;
    adapter->write(req);
  }

  void doNoop(CallbackSlot& slot) {}

  bool handleReadData(Caps& data) {
    auto it = data.iterate();
    int32_t cmd;
    try {
      it >> cmd;
      if (cmd < MIN_RESP_CMD || cmd > MAX_RESP_CMD) {
        ROKID_GERROR(CTAG, FLORA_SVC_EINVAL,
            "unknown response command %u", cmd);
        return false;
      }
      return (this->*(cmdHandlers[cmd - MIN_RESP_CMD]))(it);
    } catch (exception& e) {
      invalidCommand(data);
    }
    return false;
  }

  void invalidCommand(Caps& data) {
    char buf[64];
    try {
      data.dump(buf, sizeof(buf));
    } catch (out_of_range& e) {
      buf[63] = '\0';
    } catch (exception& e) {
      KLOGW(CTAG, "invalidCommand exception %s", e.what());
      return;
    }
    ROKID_GERROR(STAG, FLORA_CLI_EINVAL, "invalid command format: %s", buf);
    KLOGW(CTAG, "invalidCommand %s", buf);
  }

  // 接收订阅的消息
  bool handlePost(Caps::iterator& it) {
    uint32_t handle;
    Caps args;
    it >> handle;
    it >> args;
    unique_lock<mutex> locker(cliMutex);
    auto idx = checkSubHandle(handle, CallbackType::INSTANT);
    if (idx < 0)
      return true;
    CallbackSlot& slot = callbackSlots[idx];
    InstantCallback cb;
    if (slot.handle == handle)
      cb = *reinterpret_cast<InstantCallback*>(slot.stdfunc);
    locker.unlock();
    if (cb) {
      try {
        cb(args);
      } catch (exception& e) {
        exceptionInCallback("POST", e);
      }
    }
    return true;
  }

  // 接收远程方法调用返回值
  bool handleCall(Caps::iterator& it) {
    int32_t code;
    uint32_t handle;
    Caps value;
    it >> code;
    it >> handle;
    if (it.hasNext())
      it >> value;
    clientLooper.eraseAsyncCall(handle);
    doCallReturn(handle, code, value);
    return true;
  }

  void doCallReturn(uint32_t handle, int32_t code, Caps& value) {
    unique_lock<mutex> locker(cliMutex);
    auto idx = checkSubHandle(handle, CallbackType::RETURN);
    if (idx < 0)
      return;
    CallbackSlot& slot = callbackSlots[idx];
    ReturnCallback cb;
    if (slot.handle == handle)
      cb = *reinterpret_cast<ReturnCallback*>(slot.stdfunc);
    eraseCallbackSlot(handle, false);
    locker.unlock();
    if (cb) {
      try {
        cb(code, value);
      } catch (exception& e) {
        exceptionInCallback("METHOD-RETURN", e);
      }
    }
  }

  // 接收远程方法调用请求
  bool handleCallForward(Caps::iterator& it) {
    uint32_t handle;
    Caps args;
    uint32_t callid;
    it >> handle;
    it >> args;
    it >> callid;
    unique_lock<mutex> locker(cliMutex);
    auto idx = checkSubHandle(handle, CallbackType::METHOD);
    if (idx < 0) {
      replyCall(FLORA_CLI_ENOTFOUND, callid, nullptr);
      return true;
    }
    CallbackSlot& slot = callbackSlots[idx];
    MethodCallback cb;
    if (slot.handle == handle)
      cb = *reinterpret_cast<MethodCallback*>(slot.stdfunc);
    locker.unlock();
    if (cb) {
      auto reply = static_pointer_cast<Reply>(make_shared<ReplyImpl>(
            callid, self));
      try {
        cb(args, reply);
      } catch (exception& e) {
        exceptionInCallback("METHOD", e);
      }
    }
    return true;
  }

  bool handleMonitor(Caps::iterator& it) {
    return false;
  }

  bool handlePong(Caps::iterator& it) {
    return false;
  }

  bool handleUpdateStatus(Caps::iterator& it) {
    uint32_t handle;
    Caps value;
    it >> handle;
    it >> value;
    unique_lock<mutex> locker(cliMutex);
    auto idx = checkSubHandle(handle, CallbackType::STATUS);
    if (idx < 0)
      return true;
    CallbackSlot& slot = callbackSlots[idx];
    StatusCallback cb;
    if (slot.handle == handle)
      cb = *reinterpret_cast<StatusCallback*>(slot.stdfunc);
    locker.unlock();
    if (cb) {
      try {
        cb(&value, false);
      } catch (exception& e) {
        exceptionInCallback("UPDATE-STATUS", e);
      }
    }
    return true;
  }

  bool handleDeleteStatus(Caps::iterator& it) {
    uint32_t handle;
    it >> handle;
    unique_lock<mutex> locker(cliMutex);
    auto idx = checkSubHandle(handle, CallbackType::STATUS);
    if (idx < 0)
      return true;
    CallbackSlot& slot = callbackSlots[idx];
    StatusCallback cb;
    if (slot.handle == handle)
      cb = *reinterpret_cast<StatusCallback*>(slot.stdfunc);
    locker.unlock();
    if (cb) {
      try {
        cb(nullptr, true);
      } catch (exception& e) {
        exceptionInCallback("DELETE-STATUS", e);
      }
    }
    return true;
  }

  bool handleUpdatePersist(Caps::iterator& it) {
    uint32_t handle;
    Caps args;
    it >> handle;
    it >> args;
    unique_lock<mutex> locker(cliMutex);
    auto idx = checkSubHandle(handle, CallbackType::PERSIST);
    if (idx < 0)
      return true;
    CallbackSlot& slot = callbackSlots[idx];
    PersistCallback cb;
    if (slot.handle == handle)
      cb = *reinterpret_cast<PersistCallback*>(slot.stdfunc);
    locker.unlock();
    if (cb) {
      try {
        cb(&args, false);
      } catch (exception& e) {
        exceptionInCallback("UPDATE-PERSIST", e);
      }
    }
    return true;
  }

  bool handleDeletePersist(Caps::iterator& it) {
    uint32_t handle;
    it >> handle;
    unique_lock<mutex> locker(cliMutex);
    auto idx = checkSubHandle(handle, CallbackType::PERSIST);
    if (idx < 0)
      return true;
    CallbackSlot& slot = callbackSlots[idx];
    PersistCallback cb;
    if (slot.handle == handle)
      cb = *reinterpret_cast<PersistCallback*>(slot.stdfunc);
    locker.unlock();
    if (cb) {
      try {
        cb(nullptr, true);
      } catch (exception& e) {
        exceptionInCallback("DELETE-PERSIST", e);
      }
    }
    return true;
  }

  int32_t getHandleIndex(uint32_t handle) {
    if (!CallbackSlot::isValidSubHandle(handle))
      return -1;
    auto idx = CallbackSlot::indexOfSubHandle(handle);
    if (idx >= callbackSlots.size())
      return -1;
    if (callbackSlots[idx].handle != handle)
      return -1;
    return idx;
  }

  void eraseCallbackSlot(uint32_t handle, bool req) {
    auto idx = getHandleIndex(handle);
    if (idx < 0)
      return;
    eraseCallbackSlot(idx, CallbackSlot::typeOfSubHandle(handle), req);
  }

  void eraseCallbackSlot(int32_t idx, uint32_t type, bool req) {
    if (req) {
      cliMutex.lock();
      if (status & STATUS_READY)
        (this->*(unsubscribeFuncs[type]))(callbackSlots[idx]);
      cliMutex.unlock();
    }
    callbackSlots[idx].destructFunc();
  }

  // sub_t, meth_t bits format
  // |0-15 |16-27|  28 |29-31|
  // |index|seqno|magic|type |
  uint32_t makeSubHandle(int32_t idx, CallbackType type) {
    auto seq = ++subHandleSeqno;
    return ((uint32_t)type << 29) | ((seq << 20) >> 4)
      | (((uint32_t)idx << 16) >> 16) | SUBHANDLE_MAGIC;
  }

  int32_t checkSubHandle(uint32_t handle, CallbackType expectType) {
    auto idx = CallbackSlot::indexOfSubHandle(handle);
    if (idx >= callbackSlots.size()) {
      ROKID_GERROR(CTAG, FLORA_CLI_EINVAL,
          "index of sub handle out of range, %u/%u",
          idx, callbackSlots.size());
      return -1;
    }
    auto type = CallbackSlot::typeOfSubHandle(handle);
    if (type != (uint32_t)expectType) {
      ROKID_GERROR(CTAG, FLORA_CLI_EINVAL,
          "type of sub handle not match, expect %u, actual %u",
          (uint32_t)expectType, type);
      return -1;
    }
    return idx;
  }

  bool checkReady() {
    if ((status & STATUS_READY) == 0) {
      ROKID_GERROR(CTAG, FLORA_CLI_ENOT_READY,
          "connection to flora service not ready");
      return false;
    }
    return true;
  }

  bool checkIdentify(const string& id) {
    if (!isValidIdentify(id)) {
      ROKID_GERROR(CTAG, FLORA_CLI_EINVAL, "%s not valid string", id.c_str());
      return false;
    }
    return true;
  }

  uint32_t setReturnCallback(ReturnCallback cb) {
    auto idx = getEmptyCallbackSlot();
    if (idx < 0)
      idx = newCallbackSlot();
    auto handle = makeSubHandle(idx, CallbackType::RETURN);
    callbackSlots[idx].handle = handle;
    new (callbackSlots[idx].stdfunc)ReturnCallback(cb);
    return handle;
  }

  bool sendCallReq(const string& name, const string& callee, const Caps* args,
      uint32_t timeout, uint32_t handle) {
    Caps req;
    req << CMD_CALL_REQ;
    req << name;
    req << callee;
    req << timeout;
    req << handle;
    if (args)
      req << *args;
    return adapter->write(req);
  }

  void exceptionInCallback(const string& msg, exception& e) {
    KLOGW(CTAG, "exception occurs in %s callback: %s", msg.c_str(), e.what());
  }

private:
  class Options {
  public:
    string uri;
    string id;
    uint32_t bufsize{DEFAULT_BUFSIZE};
    uint32_t norespTimeout{50000};
    uint32_t sync{0};
  };
  typedef bool (ClientImpl::*CommandHandler)(Caps::iterator&);
  typedef bool (ClientImpl::*SubscribeFunc)(CallbackSlot&);
  typedef void (ClientImpl::*UnsubscribeFunc)(CallbackSlot&);

  Options options;
  vector<CallbackSlot> callbackSlots;
  shared_ptr<ClientAdapter> adapter;
  // bits value:
  // |       0       |  1  |
  // |STOPPED/RUNNING|READY|
  uint32_t status{0};
  mutex cliMutex;
  CommandHandler cmdHandlers[RESP_CMD_COUNT];
  uint32_t subHandleSeqno{0};
  SubscribeFunc subscribeFuncs[(int32_t)CallbackType::NUM_OF_TYPES];
  UnsubscribeFunc unsubscribeFuncs[(int32_t)CallbackType::NUM_OF_TYPES];
  set<string> declaredMethodNames;
  ConnectionCallback connectionCallback;
  weak_ptr<ClientImpl> self;
};
