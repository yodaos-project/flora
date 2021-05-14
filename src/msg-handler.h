#pragma once

#include "adap-mgr.h"
#include "msg-writer.h"

class MsgHandler {
public:
  MsgHandler() {
    handlers[CMD_AUTH_REQ] = &MsgHandler::handleAuth;
    handlers[CMD_SUBSCRIBE_REQ] = &MsgHandler::handleSubscribeInstant;
    handlers[CMD_UNSUBSCRIBE_REQ] = &MsgHandler::handleUnsubscribeInstant;
    handlers[CMD_POST_REQ] = &MsgHandler::handlePostInstant;
    handlers[CMD_CALL_RETURN] = &MsgHandler::handleCallReturn;
    handlers[CMD_DECLARE_METHOD_REQ] = &MsgHandler::handleDeclareMethod;
    handlers[CMD_REMOVE_METHOD_REQ] = &MsgHandler::handleRemoveMethod;
    handlers[CMD_CALL_REQ] = &MsgHandler::handleCall;
    handlers[CMD_PING_REQ] = &MsgHandler::handlePing;
    handlers[CMD_SUB_STATUS_REQ] = &MsgHandler::handleSubscribeStatus;
    handlers[CMD_UNSUB_STATUS_REQ] = &MsgHandler::handleUnsubscribeStatus;
    handlers[CMD_UPDATE_STATUS_REQ] = &MsgHandler::handleUpdateStatus;
    handlers[CMD_DEL_STATUS_REQ] = &MsgHandler::handleDeleteStatus;
    handlers[CMD_SUB_PERSIST_REQ] = &MsgHandler::handleSubscribePersist;
    handlers[CMD_UNSUB_PERSIST_REQ] = &MsgHandler::handleUnsubscribePersist;
    handlers[CMD_UPDATE_PERSIST_REQ] = &MsgHandler::handleUpdatePersist;
    handlers[CMD_DEL_PERSIST_REQ] = &MsgHandler::handleDeletePersist;
  }

  void init(AdapterManager& mgr, MsgWriter& writer) {
    adapterManager = &mgr;
    msgWriter = &writer;
  }

  bool handle(shared_ptr<ServiceAdapter>& adap, Caps& data) {
    auto it = data.iterate();
    int32_t cmd;
    try {
      it >> cmd;
      if (cmd >= REQ_CMD_COUNT) {
        ROKID_GERROR(STAG, FLORA_SVC_EINVAL, "invalid command %d", cmd);
        return false;
      }
    } catch (exception& e) {
      invalidCommand(data);
      return false;
    }
    return (this->*(handlers[cmd]))(it, adap, data);
  }

  bool handleAuth(Caps::iterator& it, shared_ptr<ServiceAdapter>& sender,
      Caps& data) {
    int32_t res{FLORA_SVC_SUCCESS};
    uint32_t version;
    string name;
    try {
      it >> version;
      if (it.hasNext())
        it >> name;
    } catch (exception& e) {
      invalidCommand(data);
      return false;
    }
    TRACE_REQ_CMD(sender, data);
    if (!name.empty()) {
      if (!isValidIdentify(name))
        res = FLORA_SVC_EINVAL;
      else if (!adapterManager->setAdapterName(sender, name))
        res = FLORA_SVC_EDUP;
    }
    if (res == FLORA_SVC_SUCCESS)
      sender->setAuthorized();
    Caps resp;
    resp << CMD_AUTH_RESP;
    resp << res;
    msgWriter->put(sender, resp, res != FLORA_SVC_SUCCESS);
    return true;
  }

  bool handleSubscribeInstant(Caps::iterator& it, shared_ptr<ServiceAdapter>& sender,
      Caps& data) {
    string name;
    uint32_t subid;
    return handleSubscribeMsg(it, sender, data, MSGTYPE_INSTANT, name, subid);
  }

  bool handleSubscribeMsg(Caps::iterator& it, shared_ptr<ServiceAdapter>& sender,
      Caps& data, uint32_t msgtype, string& name, uint32_t& subid) {
    try {
      it >> name;
      it >> subid;
    } catch (exception& e) {
      invalidCommand(data);
      return false;
    }
    TRACE_REQ_CMD(sender, data);
    if (name.empty()) {
      ROKID_GERROR(STAG, FLORA_SVC_EINVAL, "subscribe name is empty");
      return false;
    }
    adapterManager->subscribeMsg(sender, name, subid, msgtype);
    return true;
  }

  bool handleUnsubscribeInstant(Caps::iterator& it, shared_ptr<ServiceAdapter>& sender,
      Caps& data) {
    string name;
    uint32_t subid;
    return handleUnsubscribeMsg(it, sender, data, MSGTYPE_INSTANT, name, subid);
  }

  bool handleUnsubscribeMsg(Caps::iterator& it, shared_ptr<ServiceAdapter>& sender,
      Caps& data, uint32_t msgtype, string& name, uint32_t& subid) {
    try {
      it >> name;
      it >> subid;
    } catch (exception& e) {
      invalidCommand(data);
      return false;
    }
    TRACE_REQ_CMD(sender, data);
    if (name.empty()) {
      ROKID_GERROR(STAG, FLORA_SVC_EINVAL, "unsubscribe name is empty");
      return false;
    }
    adapterManager->unsubscribeMsg(sender, name, subid, msgtype);
    return true;
  }

  bool handlePostInstant(Caps::iterator& it, shared_ptr<ServiceAdapter>& sender,
      Caps& data) {
    string name;
    Caps value;
    return handlePost(it, sender, data, MSGTYPE_INSTANT, name, value);
  }

  bool handlePost(Caps::iterator& it, shared_ptr<ServiceAdapter>& sender,
      Caps& data, uint32_t msgtype, string& name, Caps& msg) {
    try {
      it >> name;
      if (it.hasNext())
        it >> msg;
    } catch (exception& e) {
      invalidCommand(data);
      return false;
    }
    TRACE_REQ_CMD(sender, data);
    if (name.empty()) {
      ROKID_GERROR(STAG, FLORA_SVC_EINVAL, "post name is empty");
      return false;
    }
    MsgSubscriptionList subs;
    if (!adapterManager->findSubscription(name, msgtype, subs))
      return true;
    for_each(subs.begin(), subs.end(), [this, &msg, &msgtype](MsgSubscription& sub) {
      Caps resp;
      auto adap = sub.adapter.lock();
      if (adap == nullptr)
        return;
      resp << (msgtype == 0 ? CMD_POST_RESP : CMD_UPDATE_PERSIST_RESP);
      resp << sub.id;
      resp << msg;
      msgWriter->put(adap, resp, false);
    });
    return true;
  }

  bool handleCallReturn(Caps::iterator& it, shared_ptr<ServiceAdapter>& sender,
      Caps& data) {
    int32_t retCode;
    uint32_t callid;
    Caps value;

    try {
      it >> retCode;
      it >> callid;
      if (retCode == FLORA_SVC_SUCCESS && it.hasNext())
        it >> value;
    } catch (exception& e) {
      invalidCommand(data);
      return false;
    }
    TRACE_REQ_CMD(sender, data);
    PendingCall callInfo;
    if (!adapterManager->findPendingCall(callid, callInfo, true)) {
      KLOGI(STAG, "pending call %u not exists", callid);
      return true;
    }
    auto adap = callInfo.caller.lock();
    if (adap == nullptr) {
      KLOGI(STAG, "method %u return but caller not exists", callInfo.reqid);
      return true;
    }
    Caps resp;
    resp << CMD_CALL_RESP;
    resp << retCode;
    resp << callInfo.reqid;
    resp << value;
    msgWriter->put(adap, resp, false);
    return true;
  }

  bool handleDeclareMethod(Caps::iterator& it, shared_ptr<ServiceAdapter>& sender,
      Caps& data) {
    string methName;
    uint32_t methid;

    try {
      it >> methName;
      it >> methid;
    } catch (exception& e) {
      invalidCommand(data);
      return false;
    }
    TRACE_REQ_CMD(sender, data);
    if (!isValidIdentify(methName)) {
      ROKID_GERROR(STAG, FLORA_SVC_EINVAL, "declare method name %s invalid",
          methName.c_str());
      return false;
    }
    int32_t res{FLORA_SVC_SUCCESS};
    if (sender->name.empty()) {
      res = FLORA_SVC_EPERMIT;
    } else if (!isValidIdentify(sender->name)) {
      res = FLORA_SVC_EINVAL;
    } else if (!adapterManager->declareMethod(sender, methName, methid)) {
      res = FLORA_SVC_EDUP;
    }
    if (res != FLORA_SVC_SUCCESS) {
      ROKID_GERROR(STAG, res, "declare method failed");
      return false;
    }
    return true;
  }

  bool handleRemoveMethod(Caps::iterator& it, shared_ptr<ServiceAdapter>& sender,
      Caps& data) {
    string methName;
    try {
      it >> methName;
    } catch (exception& e) {
      invalidCommand(data);
      return false;
    }
    TRACE_REQ_CMD(sender, data);
    if (!isValidIdentify(methName)) {
      ROKID_GERROR(STAG, FLORA_SVC_EINVAL, "remove method name %s invalid",
          methName.c_str());
      return false;
    }
    adapterManager->removeMethod(sender, methName);
    return true;
  }

  bool handleCall(Caps::iterator& it, shared_ptr<ServiceAdapter>& sender,
      Caps& data) {
    string methName;
    string target;
    uint32_t timeout;
    uint32_t reqid;
    Caps args;

    try {
      it >> methName;
      it >> target;
      it >> timeout;
      it >> reqid;
      if (it.hasNext())
        it >> args;
    } catch (exception& e) {
      invalidCommand(data);
      return false;
    }
    TRACE_REQ_CMD(sender, data);
    uint32_t methid;
    auto adap = adapterManager->findMethodTarget(methName, target, methid);
    Caps resp;
    if (adap == nullptr) {
      resp << CMD_CALL_RESP;
      resp << FLORA_SVC_ENEXISTS;
      resp << reqid;
      msgWriter->put(sender, resp, false);
    } else {
      auto callid = adapterManager->addPendingCall(sender, reqid, timeout);
      resp << CMD_CALL_FORWARD;
      resp << methid;
      resp << args;
      resp << callid;
      resp << sender->tag;
      resp << sender->name;
      msgWriter->put(adap, resp, false);
    }
    return true;
  }

  bool handlePing(Caps::iterator& it, shared_ptr<ServiceAdapter>& sender,
      Caps& data) {
    TRACE_REQ_CMD(sender, data);
    return false;
  }

  bool handleSubscribeStatus(Caps::iterator& it, shared_ptr<ServiceAdapter>& sender,
      Caps& data) {
    string name;
    string target;
    uint32_t subid;

    try {
      it >> name;
      it >> target;
      it >> subid;
    } catch (exception& e) {
      invalidCommand(data);
      return false;
    }
    TRACE_REQ_CMD(sender, data);
    if (name.empty() || target.empty()) {
      ROKID_GERROR(STAG, FLORA_SVC_EINVAL,
          "subscribe status name or target is empty");
      return false;
    }
    adapterManager->subscribeStatus(sender, name, target, subid);
    Caps value;
    if (adapterManager->getStatus(name, target, value)) {
      Caps resp;
      resp << CMD_UPDATE_STATUS_RESP;
      resp << subid;
      resp << value;
      msgWriter->put(sender, resp, false);
    }
    return true;
  }

  bool handleUnsubscribeStatus(Caps::iterator& it, shared_ptr<ServiceAdapter>& sender,
      Caps& data) {
    string name;
    string target;
    uint32_t subid;
    try {
      it >> name;
      it >> target;
      it >> subid;
    } catch (exception& e) {
      invalidCommand(data);
      return false;
    }
    TRACE_REQ_CMD(sender, data);
    if (name.empty() || target.empty()) {
      ROKID_GERROR(STAG, FLORA_SVC_EINVAL,
          "unsubscribe status failed: name or target is empty");
      return false;
    }
    adapterManager->unsubscribeStatus(sender, name, target, subid);
    return true;
  }

  bool handleUpdateStatus(Caps::iterator& it, shared_ptr<ServiceAdapter>& sender,
      Caps& data) {
    string name;
    Caps value;
    try {
      it >> name;
      if (it.hasNext())
        it >> value;
    } catch (exception& e) {
      invalidCommand(data);
      return false;
    }
    TRACE_REQ_CMD(sender, data);
    if (name.empty()) {
      ROKID_GERROR(STAG, FLORA_SVC_EINVAL, "update status: name is empty");
      return false;
    }
    if (sender->name.empty()) {
      ROKID_GERROR(STAG, FLORA_SVC_EPERMIT,
          "not permit update status %s because client id not specified",
          name.c_str());
      return false;
    }
    MsgSubscriptionList subs;
    adapterManager->findSubscription(name, sender->name, subs);
    for_each(subs.begin(), subs.end(), [this, &value](MsgSubscription& sub) {
      Caps resp;
      auto adap = sub.adapter.lock();
      if (adap == nullptr)
        return;
      resp << CMD_UPDATE_STATUS_RESP;
      resp << sub.id;
      resp << value;
      msgWriter->put(adap, resp, false);
    });
    adapterManager->updateStatus(sender, name, value);
    sender->addStatus(name);
    return true;
  }

  bool handleDeleteStatus(Caps::iterator& it, shared_ptr<ServiceAdapter>& sender,
      Caps& data) {
    string name;
    try {
      it >> name;
    } catch (exception& e) {
      invalidCommand(data);
      return false;
    }
    TRACE_REQ_CMD(sender, data);
    if (name.empty()) {
      ROKID_GERROR(STAG, FLORA_SVC_EINVAL, "delete status: name is empty");
      return false;
    }
    if (sender->name.empty()) {
      ROKID_GERROR(STAG, FLORA_SVC_EPERMIT,
          "not permit delete status %s because client id not specified",
          name.c_str());
      return false;
    }
    notifyRemoveStatus(name, sender->name);
    adapterManager->deleteStatus(sender, name);
    sender->eraseStatus(name);
    return true;
  }

  bool handleSubscribePersist(Caps::iterator& it, shared_ptr<ServiceAdapter>& sender,
      Caps& data) {
    string name;
    uint32_t subid;
    if (!handleSubscribeMsg(it, sender, data, MSGTYPE_PERSIST, name, subid))
      return false;
    Caps value;
    if (adapterManager->getPersistValue(name, value)) {
      Caps resp;
      resp << CMD_UPDATE_PERSIST_RESP;
      resp << subid;
      resp << value;
      msgWriter->put(sender, resp, false);
    }
    return true;
  }

  bool handleUnsubscribePersist(Caps::iterator& it, shared_ptr<ServiceAdapter>& sender,
      Caps& data) {
    string name;
    uint32_t subid;
    return handleUnsubscribeMsg(it, sender, data, MSGTYPE_PERSIST, name, subid);
  }

  bool handleUpdatePersist(Caps::iterator& it, shared_ptr<ServiceAdapter>& sender,
      Caps& data) {
    string name;
    Caps value;
    if (!handlePost(it, sender, data, MSGTYPE_PERSIST, name, value))
      return false;
    adapterManager->updatePersistValue(name, value);
    return true;
  }

  bool handleDeletePersist(Caps::iterator& it, shared_ptr<ServiceAdapter>& sender,
      Caps& data) {
    string name;
    try {
      it >> name;
    } catch (exception& e) {
      invalidCommand(data);
      return false;
    }
    TRACE_REQ_CMD(sender, data);
    if (name.empty()) {
      ROKID_GERROR(STAG, FLORA_SVC_EINVAL, "delete persist: name is empty");
      return false;
    }
    MsgSubscriptionList subs;
    adapterManager->findSubscription(name, MSGTYPE_PERSIST, subs);
    adapterManager->deletePersistValue(name);
    for_each(subs.begin(), subs.end(), [this](MsgSubscription& sub) {
      Caps resp;
      auto adap = sub.adapter.lock();
      if (adap == nullptr)
        return;
      resp << CMD_DEL_PERSIST_RESP;
      resp << sub.id;
      msgWriter->put(adap, resp, false);
    });
    return true;
  }

  void invalidCommand(Caps& data) {
    char buf[64];
    data.dump(buf, sizeof(buf));
    ROKID_GERROR(STAG, FLORA_SVC_EINVAL, "invalid command format: %s", buf);
  }

  void cleanStatus(shared_ptr<ServiceAdapter>& adapter) {
    if (!adapter->allStatus.empty() && !adapter->name.empty()) {
      auto it = adapter->allStatus.begin();
      while (it != adapter->allStatus.end()) {
        notifyRemoveStatus(*it, adapter->name);
        adapterManager->deleteStatus(adapter, *it);
        ++it;
      }
    }
  }

private:
  void notifyRemoveStatus(const string& statusName, const string& adapName) {
    MsgSubscriptionList subs;
    adapterManager->findSubscription(statusName, adapName, subs);
    for_each(subs.begin(), subs.end(), [this](MsgSubscription& sub) {
      Caps resp;
      auto adap = sub.adapter.lock();
      if (adap == nullptr)
        return;
      resp << CMD_DEL_STATUS_RESP;
      resp << sub.id;
      msgWriter->put(adap, resp, false);
    });
  }

private:
  typedef bool (MsgHandler::*Handler)(
      Caps::iterator&, shared_ptr<ServiceAdapter>&, Caps&);
  AdapterManager* adapterManager;
  MsgWriter* msgWriter;
  Handler handlers[REQ_CMD_COUNT];
};
