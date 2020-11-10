#pragma once

#include <stdint.h>
#include <stdarg.h>
#include <memory>
#include <string>
#include <functional>
#include "flora-defs.h"

namespace rokid {
class Caps;
} // namespace rokid

namespace flora {

enum class ClientOptions {
  URI,
  ID,
  BUFSIZE,
  SYNC
};

typedef uint32_t sub_t;
typedef uint32_t meth_t;
typedef std::function<void(const rokid::Caps&)> InstantCallback;
/// \param remove  true: persist消息删除通知  false: persist消息更新通知
typedef std::function<void(const rokid::Caps*, bool remove)> PersistCallback;
typedef PersistCallback StatusCallback;
class Reply;
typedef std::function<void(const rokid::Caps&, std::shared_ptr<Reply>&)>
    MethodCallback;
typedef std::function<void(int32_t, const rokid::Caps&)> ReturnCallback;
/// \param status  1: connected  0: disconnected  <0: error code(仅当使用异步模式调用open时)
typedef std::function<void(int32_t status)> ConnectionCallback;

class Reply {
public:
  virtual ~Reply() = default;

  virtual bool end(const rokid::Caps* data) = 0;
};

class Client {
public:
  virtual ~Client() = default;

  /// 在client连接前配置
  /// 连接后再配置暂不生效，重连后生效
  /// 线程不安全
  void config(ClientOptions opt, ...) {
    va_list ap;
    va_start(ap, opt);
    config(opt, ap);
    va_end(ap);
  }

  virtual void config(ClientOptions opt, va_list ap) = 0;

  virtual sub_t subscribe(const std::string& name, InstantCallback cb) = 0;

  virtual sub_t subscribe(const std::string& name, PersistCallback cb) = 0;

  virtual sub_t subscribe(const std::string& name, const std::string& target,
      StatusCallback cb) = 0;

  virtual meth_t declareMethod(const std::string& name, MethodCallback cb) = 0;

  virtual void unsubscribe(sub_t handle) = 0;

  virtual void removeMethod(meth_t handle) = 0;

  virtual bool publish(const std::string& name, const rokid::Caps* msg) = 0;

  virtual bool updatePersist(const std::string& name,
      const rokid::Caps* msg) = 0;

  virtual bool updateStatus(const std::string& name,
      const rokid::Caps* msg) = 0;

  virtual bool call(const std::string& name, const std::string& callee,
      const rokid::Caps* args, uint32_t timeout, rokid::Caps* ret) = 0;

  virtual bool callAsync(const std::string& name, const std::string& callee,
      const rokid::Caps* args, uint32_t timeout, ReturnCallback cb) = 0;

  virtual bool deletePersist(const std::string& name) = 0;

  virtual bool deleteStatus(const std::string& name) = 0;

  virtual bool open(ConnectionCallback cb) = 0;

  // NOTE: 销毁已经open的Client对象前一定要先close，否则不保证进程不崩溃
  virtual void close() = 0;

  static std::shared_ptr<Client> newInstance();
};

} // namespace flora
