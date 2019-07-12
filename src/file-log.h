#pragma once

#ifdef DEBUG_FOR_YODAV8

#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include "rlog.h"
#include "defs.h"

class FileLogWriter : public RLogWriter {
public:
  bool init(void* arg) {
    return true;
  }

  void destroy() {
  }

  int32_t raw_write(const char* file, int line, RokidLogLevel lv,
      const char* tag, const char* fmt, va_list ap) {
    if (strcmp(tag, FILE_TAG) == 0)
      return 0;
    return 1;
  }

  bool write(const char* data, uint32_t size) {
    if (fileName.empty())
      return false;
    auto fd = open(fileName.c_str(), O_WRONLY | O_CREAT | O_APPEND, 0644);
    if (fd < 0)
      return false;
    auto r = ::write(fd, data, size);
    close(fd);
    return r > 0;
  }

private:
  std::string fileName{"/data/flora-dispatcher.fatal.log"};
};

#endif // DEBUG_FOR_YODAV8
