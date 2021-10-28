#pragma once

#include <arpa/inet.h>
#include <assert.h>
#include <string>
#include <vector>
#include <list>
#include <set>
#include <deque>
#include <map>
#include <memory>
#include <chrono>
#include <mutex>
#include <condition_variable>
#include <cstddef>
#include "global-error.h"
#include "flora-svc.h"
#include "flora-cli.h"
#include "flora-defs.h"

using namespace std;
using namespace std::chrono;
using namespace flora;
using namespace rokid;

#define MIN_BUFSIZE 4096
#define SOCKET_TYPE_LISTEN 1
#define SOCKET_TYPE_TCP 2
#define CLIENT_FLAG_MONITOR 1u
#define STAG "flora-svc"
#define CTAG "flora-cli"

class TagHelper {
public:
  static uint64_t create(struct sockaddr_in& addr) {
    return (((uint64_t)(0x80000000 | addr.sin_port)) << 32) | addr.sin_addr.s_addr;
  }

  static uint64_t create(uint32_t pid) { return pid; }

  // return: 0  unix
  //         1  tcp
  static uint32_t type(uint64_t tag) {
    return ((tag >> 32) & 0x80000000) ? 1 : 0;
  }

  static pid_t pid(uint64_t tag) {
    return tag & 0xffffffff;
  }

  static const char* ipaddr(uint64_t tag) {
    struct in_addr iaddr;
    iaddr.s_addr = (tag & 0xffffffff);
    return inet_ntoa(iaddr);
  }

  static uint16_t port(uint64_t tag) {
    return (tag >> 32) & 0xffff;
  }

  static void setReady(uint64_t& tag) {
    tag |= 0x2000000000000000L;
  }

  static bool ready(uint64_t tag) {
    return tag & 0x2000000000000000L;
  }

  static void toString(uint64_t tag, std::string& str) {
    char buf[16];
    if (type(tag) == 0) {
      snprintf(buf, sizeof(buf), "%d", pid(tag));
      str = buf;
    } else {
      str = ipaddr(tag);
      str += ':';
      snprintf(buf, sizeof(buf), "%d", port(tag));
      str += buf;
    }
  }
};

bool isValidIdentify(const string& id);

void beReadU32(const uint8_t* in, uint32_t& v);

void printGlobalError();

bool setSocketTimeout(int socket, int32_t tm, bool rd);

// 版本5，对应release/2.0.0，大更新，不向前兼容
#define FLORA_VERSION 5

#ifdef TRACE_COMMANDS
#define TRACE_REQ_CMD(a, d) DataTracer::traceReqCmd(a, d);
#define TRACE_RESP_CMD(a, d) DataTracer::traceRespCmd(a, d);
#else
#define TRACE_REQ_CMD(a, d)
#define TRACE_RESP_CMD(a, d)
#endif

////// value of commands //////
// client --> server
#define CMD_AUTH_REQ 0
#define CMD_SUBSCRIBE_REQ 1
#define CMD_UNSUBSCRIBE_REQ 2
#define CMD_POST_REQ 3
#define CMD_CALL_RETURN 4
#define CMD_DECLARE_METHOD_REQ 5
#define CMD_REMOVE_METHOD_REQ 6
#define CMD_CALL_REQ 7
#define CMD_PING_REQ 8
// subscribe status msg
#define CMD_SUB_STATUS_REQ 9
#define CMD_UNSUB_STATUS_REQ 10
// update client status msg
#define CMD_UPDATE_STATUS_REQ 11
#define CMD_DEL_STATUS_REQ 12
#define CMD_SUB_PERSIST_REQ 13
#define CMD_UNSUB_PERSIST_REQ 14
#define CMD_UPDATE_PERSIST_REQ 15
#define CMD_DEL_PERSIST_REQ 16
#define CMD_MONITOR_REQ 17
// 连接, 认证, 初始消息订阅完后, 客户端发送ready指令
// 服务端处理ready指令后，该客户端的连接状态可被通知到其它监视端
#define CMD_READY_REQ 18
#define REQ_CMD_COUNT 19
// server --> client
#define MIN_RESP_CMD 101
#define CMD_AUTH_RESP 101
#define CMD_POST_RESP 102
#define CMD_CALL_RESP 103
#define CMD_CALL_FORWARD 104
#define CMD_MONITOR_RESP 105
#define CMD_PONG_RESP 106
#define CMD_UPDATE_STATUS_RESP 107
#define CMD_DEL_STATUS_RESP 108
#define CMD_UPDATE_PERSIST_RESP 109
#define CMD_DEL_PERSIST_RESP 110
#define CMD_CONNECTION_INFO_NOTIFY 111
#define MAX_RESP_CMD 111
#define RESP_CMD_COUNT 11
