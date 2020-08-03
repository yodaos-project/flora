#pragma once

#ifdef TRACE_COMMANDS

#include <string.h>
#include <mutex>
#include "adap.h"
#include "caps.h"
#include "common.h"

class DataTracer {
public:
  // << ${id} ${cmd}\n${data}
  static void traceReqCmd(shared_ptr<ServiceAdapter>& adap, Caps& data) {
    int32_t cmd;
    auto it = data.iterate();
    it >> cmd;
    lock_guard<mutex> locker(traceMutex);
    auto datastr = dumpCaps(data);
    KLOGI(STAG, "<< %s %s\n%s", adap->name.empty() ? "[anon]" : adap->name.c_str(),
        reqCmdStr[cmd], datastr);
  }

  // >> ${id} ${cmd}\n${data}
  static void traceRespCmd(shared_ptr<ServiceAdapter>& adap, Caps& data) {
    int32_t cmd;
    auto it = data.iterate();
    it >> cmd;
    lock_guard<mutex> locker(traceMutex);
    auto datastr = dumpCaps(data);
    KLOGI(STAG, ">> %s %s\n%s", adap->name.empty() ? "[anon]" : adap->name.c_str(),
        respCmdStr[cmd - MIN_RESP_CMD], datastr);
  }

private:
  static const char* dumpCaps(Caps& data) {
    auto r = data.dump(dataBuffer, sizeof(dataBuffer));
    if (r) {
      dataBuffer[r - 1] = '\0';
      auto p = strchr(dataBuffer, '\n');
      if (p) {
        return p + 1;
      }
    }
    return "";
  }

private:
  static const char* reqCmdStr[REQ_CMD_COUNT];
  static const char* respCmdStr[RESP_CMD_COUNT];
  static char dataBuffer[1024];
  static mutex traceMutex;
};

#endif // TRACE_COMMANDS
