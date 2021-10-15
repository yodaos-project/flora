#pragma once

#include <stdarg.h>
#include <memory>
#include <string>
#include <vector>
#include "flora-defs.h"

namespace flora {

enum class ServiceOptions {
  LISTEN_URI,
  BUFSIZE,
  READ_THREAD_NUM,
  WRITE_THREAD_NUM
};

class Service {
public:
  class Builder {
  public:
    /// \brief 添加服务侦听uri
    /// \note 可同时侦听多个uri
    /// \note unix socket uri 例: "unix:flora"
    /// \note tcp socket uri 例: "tcp://127.0.0.1:37710/"
    Builder& addUri(const std::string& uri) {
      uris.push_back(uri);
      return *this;
    }

    Builder& setBufsize(uint32_t size) {
      bufsize = size;
      return *this;
    }

    Builder& setReadThreadNum(uint32_t n) {
      readThreadNum = n;
      return *this;
    }

    Builder& setWriteThreadNum(uint32_t n) {
      writeThreadNum = n;
      return *this;
    }

    std::shared_ptr<Service> build();

  private:
    std::vector<std::string> uris;
    uint32_t bufsize{FLORA_DEFAULT_BUFSIZE};
    uint32_t readThreadNum{1};
    uint32_t writeThreadNum{1};
  };

  virtual ~Service() = default;

  virtual bool start(bool blocking = false) = 0;

  virtual void stop() = 0;
};

} // namespace rokid
