#pragma once

/// \file flora-cli.h
/// flora客户端接口定义
/// \author chen.zhang@rokid.com
/// \version 2.0.0
/// \date 2020-12-29

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

/// \enum ClientOptions
/// flora client配置选项，用于config调用
enum class ClientOptions {
  /// flora服务uri
  URI,
  /// 指定flora客户端id
  ID,
  BUFSIZE,
  /// 是否启用同步模式
  /// 同步模式open不退出，直到调用close
  SYNC
};

typedef uint32_t sub_t;
typedef uint32_t meth_t;
/// \brief 普通广播消息回调
/// \param data 消息数据
typedef std::function<void(const rokid::Caps& data)> InstantCallback;
/// \brief 全局状态回调
/// \param data 状态数据
/// \param remove  true: 全局状态删除通知  false: 全局状态更新通知
typedef std::function<void(const rokid::Caps* data, bool remove)> PersistCallback;
/// \brief 局部状态回调
/// \param data 状态数据
/// \param remove  true: 局部状态删除通知  false: 局部状态更新通知
typedef PersistCallback StatusCallback;
class Reply;
/// \brief 远程方法被调用
/// \param args 方法参数数据
/// \param reply 用于远程方法执行后返回数据
typedef std::function<void(const rokid::Caps& args, std::shared_ptr<Reply>& reply)>
    MethodCallback;
/// \brief 异步远程方法调用时, 被调方返回数据后, 在调用方的回调
/// \param code  错误码, 0表示方法调用成功
/// \param data 远程方法返回的数据
/// \note 错误码见flora-defs.h
/// \note FLORA_CLI_ETIMEOUT 远程方法调用超时
/// \note FLORA_CLI_ENOTFOUND 远程方法未找到
typedef std::function<void(int32_t code, const rokid::Caps& data)> ReturnCallback;
/// \brief flora连接状态回调
/// \param status  1: connected  0: disconnected  <0: error code(仅当使用异步模式调用open时)
/// \note 客户端配置为异步模式时，status可能为错误码
/// \note 错误码见flora-defs.h
/// \note FLORA_CLI_EINVAL 配置的uri或id不合法
/// \note FLORA_CLI_ESYS socket连接错误
/// \note FLORA_CLI_ETIMEOUT 连接超时
/// \note FLORA_CLI_EDUP 配置的id已被其它客户端占用
typedef std::function<void(int32_t status)> ConnectionCallback;

/// \class Reply
/// 为远程方法调用返回数据
class Reply {
public:
  virtual ~Reply() = default;

  /// \brief 远程方法被调端返回给调用端数据
  /// \param data 返回数据
  /// \return \b true 成功 \b false 失败
  /// \note 失败后使用rokid::GlobalError::code获取错误码, ROKID_GERROR_STRING获取格式化错误字符串
  /// \note 错误码见flora-defs.h
  /// \note FLORA_CLI_ESYS socket错误
  /// \note FLORA_CLI_ENOT_READY 客户端未连接
  virtual bool end(const rokid::Caps* data) = 0;
};

/// \class Client
/// flora客户端
class Client {
public:
  virtual ~Client() = default;

  /// \brief 配置客户端
  /// 在客户端连接前配置
  /// 连接后再配置暂不生效
  /// 线程不安全
  /// \param opt 指定要配置的选项
  /// \note ClientOptions::ID string类型, 客户端唯一标识
  /// \note ClientOptions::URI string类型, 服务端uri
  /// \note 多次调用config(ClientOptions::URI, ...) 可配置多个uri
  /// \note unix socket uri 例: "unix:flora"
  /// \note tcp socket uri 例: "tcp://127.0.0.1:37710/"
  /// \note ClientOptions::SYNC int32_t类型, 默认0. 0(异步) 1(同步)
  /// \note ClientOptions::BUFSIZE 消息缓冲区大小, 决定了一次最大传输的消息数据量. int32_t类型, 默认4096
  void config(ClientOptions opt, ...) {
    va_list ap;
    va_start(ap, opt);
    config(opt, ap);
    va_end(ap);
  }

  virtual void config(ClientOptions opt, va_list ap) = 0;

  /// \brief 订阅普通广播消息
  /// \param name 消息名
  /// \param cb 当接收到订阅消息时回调
  /// \return 标识, 用于unsubscribe
  virtual sub_t subscribe(const std::string& name, InstantCallback cb) = 0;

  /// \brief 订阅全局状态
  /// \param name 消息名
  /// \param cb 当接收到订阅消息时回调
  /// \return 标识, 用于unsubscribe
  virtual sub_t subscribe(const std::string& name, PersistCallback cb) = 0;

  /// \brief 订阅局部状态
  /// \param name 消息名
  /// \param cb 当接收到订阅消息时回调
  /// \return 标识, 用于unsubscribe
  virtual sub_t subscribe(const std::string& name, const std::string& target,
      StatusCallback cb) = 0;

  /// \brief 声明远程方法
  /// \param name 消息名
  /// \param cb 当远程方法被调用时回调
  /// \return 标识, 用于removeMethod
  virtual meth_t declareMethod(const std::string& name, MethodCallback cb) = 0;

  /// \brief 取消订阅普通广播消息/全局状态/局部状态
  /// \param handle  取消订阅的消息标识, 由订阅函数返回.
  virtual void unsubscribe(sub_t handle) = 0;

  /// \brief 删除远程方法
  /// \param handle  删除的远程方法标识, 由declareMethod返回.
  virtual void removeMethod(meth_t handle) = 0;

  /// \brief 发布普通广播消息
  /// \param name 消息名
  /// \param msg 结构化消息数据, 以Caps作为容器
  /// \return \b true 成功 \b false 失败
  /// \note 失败后使用rokid::GlobalError::code获取错误码, ROKID_GERROR_STRING获取格式化错误字符串
  /// \note 错误码见flora-defs.h
  /// \note FLORA_CLI_EINVAL name格式不合法
  /// \note FLORA_CLI_ENOT_READY 客户端未连接服务端
  /// \note FLORA_CLI_ESYS socket错误
  /// \note https://github.com/mingzc0508/mutils
  /// \note branch: dev/1.1.x
  /// \note include/global-error.h
  virtual bool publish(const std::string& name, const rokid::Caps* msg) = 0;

  /// \brief 发布/更新全局状态
  /// \param name 全局状态名
  /// \param msg 结构化状态数据, 以Caps作为容器
  /// \return \b true 成功 \b false 失败
  /// \note 失败后使用rokid::GlobalError::code获取错误码, ROKID_GERROR_STRING获取格式化错误字符串
  /// \note 错误码见flora-defs.h
  /// \note FLORA_CLI_EINVAL name格式不合法
  /// \note FLORA_CLI_ENOT_READY 客户端未连接服务端
  /// \note FLORA_CLI_ESYS socket错误
  virtual bool updatePersist(const std::string& name,
      const rokid::Caps* msg) = 0;

  /// \brief 发布/更新局部状态
  /// \param name 局部状态名
  /// \param msg 结构化状态数据, 以Caps作为容器
  /// \return \b true 成功 \b false 失败
  /// \note 失败后使用rokid::GlobalError::code获取错误码, ROKID_GERROR_STRING获取格式化错误字符串
  /// \note 错误码见flora-defs.h
  /// \note FLORA_CLI_EINVAL name格式不合法
  /// \note FLORA_CLI_ENOT_READY 客户端未连接服务端
  /// \note FLORA_CLI_ESYS socket错误
  virtual bool updateStatus(const std::string& name,
      const rokid::Caps* msg) = 0;

  /// \brief 调用远程方法(同步)
  /// \param name  远程方法名
  /// \param callee  被调远程方法所属flora客户端id
  /// \param args  远程方法参数
  /// \param timeout  调用超时时间
  /// \param ret  获取远程方法返回的数据
  /// \return \b true 成功 \b false 失败
  /// \note 失败后使用rokid::GlobalError::code获取错误码, ROKID_GERROR_STRING获取格式化错误字符串
  /// \note 错误码见flora-defs.h
  /// \note FLORA_CLI_EINVAL name或callee格式不合法
  /// \note FLORA_CLI_ENOT_READY 客户端未连接服务端
  /// \note FLORA_CLI_ESYS socket错误
  /// \note FLORA_CLI_ETIMEOUT 远程方法调用超时
  /// \note FLORA_CLI_ENOTFOUND 远程方法未找到
  virtual bool call(const std::string& name, const std::string& callee,
      const rokid::Caps* args, uint32_t timeout, rokid::Caps* ret) = 0;

  /// \brief 调用远程方法(异步)
  /// \param name  远程方法名
  /// \param callee  被调远程方法所属flora客户端id
  /// \param args  远程方法参数
  /// \param timeout  调用超时时间
  /// \param cb  远程方法数据返回回调
  /// \return \b true 成功 \b false 失败
  /// \note 失败后使用rokid::GlobalError::code获取错误码, ROKID_GERROR_STRING获取格式化错误字符串
  /// \note 错误码见flora-defs.h
  /// \note FLORA_CLI_EINVAL name或callee格式不合法
  /// \note FLORA_CLI_ENOT_READY 客户端未连接服务端
  /// \note FLORA_CLI_ESYS socket错误
  virtual bool callAsync(const std::string& name, const std::string& callee,
      const rokid::Caps* args, uint32_t timeout, ReturnCallback cb) = 0;

  /// \brief 删除全局状态
  virtual bool deletePersist(const std::string& name) = 0;

  /// \brief 删除局部状态
  virtual bool deleteStatus(const std::string& name) = 0;

  /// \brief 启动客户端
  /// \param cb  连接状态回调
  /// \return \b true 成功 \b false 失败
  /// \note 失败后使用rokid::GlobalError::code获取错误码, ROKID_GERROR_STRING获取格式化错误字符串
  /// \note 错误码见flora-defs.h
  /// \note FLORA_CLI_EINVAL 配置的uri或id不合法
  /// \note FLORA_CLI_ESYS socket连接错误
  /// \note FLORA_CLI_ETIMEOUT 连接超时
  /// \note FLORA_CLI_EDUP 配置的id已被其它客户端占用
  /// \note 默认SYNC为0
  /// config设置ClientOptions::SYNC为1时，open调用为阻塞模式
  virtual bool open(ConnectionCallback cb) = 0;

  /// \breif 关闭客户端
  virtual void close() = 0;

  /// \brief 创建客户端对象
  static std::shared_ptr<Client> newInstance();
};

} // namespace flora
