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

  /// \brief 配置服务端
  /// 在服务端start()前配置
  /// 线程不安全
  /// \note ServiceOptions::LISTEN_URI string类型, 服务端侦听uri
  /// \note 多次调用config(ServiceOptions::LISTEN_URI, ...) 可配置多个uri
  /// \note unix socket uri 例: "unix:flora"
  /// \note tcp socket uri 例: "tcp://127.0.0.1:37710/"
  virtual void config(ServiceOptions opt, va_list ap) = 0;

  virtual bool start(bool blocking = false) = 0;

  virtual void stop() = 0;

  static std::shared_ptr<Service> newInstance();
};

} // namespace rokid
