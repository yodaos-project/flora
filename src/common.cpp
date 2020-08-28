#include <errno.h>
#include <string.h>
#include "rlog.h"
#include "common.h"
#include "flora-defs.h"
#include "data-tracer.h"

static bool isValidIdChar(char c) {
  // TODO: use bits field
  if (c >= 'a' && c <= 'z')
    return true;
  if (c >= '0' && c <= '9')
    return true;
  if (c >= 'A' && c <= 'Z')
    return true;
  return c == '.' || c == '-' || c == '_';
}

bool isValidIdentify(const string& id) {
  if (id.empty())
    return false;
  auto s = id.c_str();
  while (*s) {
    if (!isValidIdChar(*s))
      return false;
    ++s;
  }
  return true;
}

void beReadU32(const uint8_t* in, uint32_t& v) {
  v = in[0] << 24;
  v |= in[1] << 16;
  v |= in[2] << 8;
  v |= in[3];
}

void printGlobalError() {
  KLOGW(STAG, "%s", GlobalError::msg());
}

bool setSocketTimeout(int socket, int32_t tm, bool rd) {
  struct timeval tv;
  if (tm > 0) {
    tv.tv_sec = tm / 1000;
    tv.tv_usec = (tm % 1000) * 1000;
  } else {
    tv.tv_sec = 0;
    tv.tv_usec = 0;
  }
  int opt = rd ? SO_RCVTIMEO : SO_SNDTIMEO;
  if (setsockopt(socket, SOL_SOCKET, opt, &tv, sizeof(tv)) < 0) {
    ROKID_GERROR(STAG, FLORA_SVC_ESYS, "set socket[%d] %s timeout failed: %s",
        socket, rd ? "read" : "write", strerror(errno));
    return false;
  }
  return true;
}

#ifdef TRACE_COMMANDS
char DataTracer::dataBuffer[1024];
mutex DataTracer::traceMutex;
const char* DataTracer::reqCmdStr[REQ_CMD_COUNT] = {
  "AUTH",
  "SUB_INSTANT",
  "UNSUB_INSTANT",
  "PUBLISH",
  "CALL_RETURN",
  "DECLARE_METHOD",
  "REMOVE_METHOD",
  "CALL",
  "PING",
  "SUB_STATUS",
  "UNSUB_STATUS",
  "UPDATE_STATUS",
  "DELETE_STATUS",
  "SUB_PERSIST",
  "UNSUB_PERSIST",
  "UPDATE_PERSIST",
  "DELETE_PERSIST",
  "MONITOR"
};
const char* DataTracer::respCmdStr[RESP_CMD_COUNT] = {
  "AUTH",
  "PUBLISH",
  "CALL_RETURN",
  "CALL",
  "MONITOR",
  "PONG",
  "UPDATE_STATUS",
  "DELETE_STATUS",
  "UPDATE_PERSIST",
  "DELETE_PERSIST"
};
#endif // TRACE_COMMANDS
