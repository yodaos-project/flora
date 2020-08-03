#pragma once

#include <stdarg.h>
#include <memory>

namespace flora {

enum class ServiceOptions {
  LISTEN_URI,
  BUFSIZE,
  READ_THREAD_NUM,
  WRITE_THREAD_NUM
};

class Service {
public:
  virtual ~Service() = default;

  void config(ServiceOptions opt, ...) {
    va_list ap;
    va_start(ap, opt);
    config(opt, ap);
    va_end(ap);
  }

  virtual void config(ServiceOptions opt, va_list ap) = 0;

  virtual bool start(bool blocking = false) = 0;

  virtual void stop() = 0;

  static std::shared_ptr<Service> newInstance();
};

} // namespace rokid
