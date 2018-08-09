# 客户端接口定义

## 类Client静态成员函数

**\[\[connect]]**

生成Client对象并连接服务端

* 返回值: int32_t  [错误码](#anchor01)

```
FLORA_CLI_SUCCESS  成功
FLORA_CLI_EAUTH    认证失败
FLORA_CLI_EINVAL   参数错误
FLORA_CLI_ECONN    连接错误
```

* 参数  const char* uri

[uri格式](#anchor05)

```
连接的服务端uri
```

* 参数  [ClientCallback](#anchor02)* cb

```
回调类对象
```

* 参数  uint32_t msg_buf_size

```
接收/发送消息的缓冲区大小
填0设置为默认值
```

* 参数  shared_ptr<Client>& result

```
连接成功后，被设置为生成的Client对象的指针
```

## 类Client非静态成员函数

**\[\[subscribe]]**

订阅消息

* 返回值: int32_t [错误码](#anchor01)

```
FLORA_CLI_SUCCESS
FLORA_CLI_EINVAL
```

* 参数  const char* name

```
订阅的消息名称
```

* 参数  uint32_t msgtype  [消息类型](#anchor03)

```
订阅的消息类型
```

**\[\[unsubscribe]]**

取消订阅

* 返回值: int32_t [错误码](#anchor01)

```
FLORA_CLI_SUCCESS
FLORA_CLI_EINVAL
```

* 参数  const char* name

* 参数  uint32_t msgtype  [消息类型](#anchor03)

**\[\[post]]**

发送INSTANT/PERSIST消息

* 参数 const char* name

* 参数 shared_ptr\<[Caps](https://github.com/Rokid/aife-mutils/blob/master/caps.md)>& msg

```
发送的消息参数
Caps对象，支持多种数据类型的数据结构序列化工具
```

* 参数  uint32_t msgtype  [消息类型](#anchor03)

**\[\[get]]**

发送REQUEST消息，并等待回复

* 参数 const char* name

* 参数 shared_ptr\<[Caps](https://github.com/Rokid/aife-mutils/blob/master/caps.md)>& msg

* 参数 vector<[Reply](#anchor04)>& replys

```
回复的消息，可能有0或多个订阅客户端回复
```

* 参数 uint32_t timeout

```
等待回复的超时时间，单位毫秒
```

## <a id="anchor02"></a>类ClientCallback

**\[\[recv_post]]**

接收INSTANT/PERSIST消息

* 参数 const char* name

* 参数  uint32_t msgtype  [消息类型](#anchor03)

* 参数 shared_ptr\<[Caps](https://github.com/Rokid/aife-mutils/blob/master/caps.md)>& msg

**\[\[recv_get]]**

接收REQUEST消息并回复

* 返回值: int32_t  自定义返回码，此值将发回到此REQUEST消息的发送端

* 参数 const char* name

* 参数 shared_ptr\<[Caps](https://github.com/Rokid/aife-mutils/blob/master/caps.md)>& msg

* 参数 shared_ptr\<[Caps](https://github.com/Rokid/aife-mutils/blob/master/caps.md)>& reply

```
发回到此REQUEST消息的发送端的自定义消息体
应在'recv_get'函数中填充
```

**\[\[disconnected]]**

客户端断开连接

## <a id="anchor04"></a>Reply

数据类型 | 名称 | 描述
--- | --- | ---
int32_t | ret_code | 返回码<br>0: 正确<br>其它: 订阅客户端自定义错误
shared_ptr\<Caps> | msg | 返回的消息体，内容为回复此消息的客户端自定义
string | extra | 回复此消息的客户端的身份标识

## <a id="anchor01"></a>错误码

名称 | 值 | 描述
--- | --- | ---
FLORA_CLI_SUCCESS | 0 | 成功
FLORA_CLI_EAUTH | -1 | 认证失败
FLORA_CLI_EINVAL | -2 | 参数不合法
FLORA_CLI_ECONN | -3 | 连接错误
FLORA_CLI_ETIMEOUT | -4 | get请求超时
FLORA_CLI_ENEXISTS | -5 | get请求目标不存在

# 服务端接口定义

```
服务端由1个Dispatcher及至少1个Poll组成
Dispatcher为消息转发核心逻辑
Poll决定服务端的连接方式(协议，地址)
目前支持的协议：
    tcp
```

## 类Dispatcher静态成员函数

**\[\[new\_instance]]**

创建Dispatcher对象

* 返回值: shared_ptr\<Dispatcher>

* 参数  msg_buf_size

```
接收/发送消息的缓冲区大小
填0设置为默认值
```

## 类Poll静态成员函数

**\[\[new\_instance]]**

创建Poll对象

* 返回值: shared_ptr\<Poll>

* 参数  const char* uri

[uri格式](#anchor05)

## 类Poll非静态成员函数

**\[\[start]]**

开始接受客户端连接

* 返回值: int32_t  [错误码](#anchor06)

* 参数: shared_ptr\<Dispatcher>& dispatcher

```
Poll绑定Dispatcher
一个Dispatcher可与多个Poll绑定
```

**\[\[stop]]**

停止接受客户端连接并断开现有的客户端连接

## <a id="anchor06"></a>错误码

名称 | 值 | 描述
--- | --- | ---
FLORA_POLL_SUCCESS | 0 | 成功
FLORA_POLL_ALREADY_START | -1 | poll已经start
FLORA_POLL_SYSERR | -2 | 由于系统错误，start失败
FLORA_POLL_INVAL | -3 | 参数非法
FLORA_POLL_UNSUPP | -4 | 不支持的uri scheme

# 示例

## 客户端

```
using namespace flora;

class MyFloraClientCallback : public ClientCallback {
public:
  void recv_post(const char* name, uint32_t msgtype, shared_ptr<Caps>& msg); // 自定义实现

	int32_t recv_get(const char* name, shared_ptr<Caps>& msg, shared_ptr<Caps>& reply) {
	  // ... do something ...
		reply = Caps::new_instance();
		// ... write some data to 'reply' ...
	  return FLORA_CLI_SUCCESS;  // 视情况，可以返回自定义错误码
	}

	void disconnected(); // 自定义实现
};

shared_ptr<Client> cli;
MyFloraClientCallback cb;
if (Client::connect("tcp://localhost:5678/#foo1", &cb, 0, cli) != FLORA_CLI_SUCCESS) {
  // TODO: 错误处理
}
cli.subscribe("hello", FLORA_MSGTYPE_INSTANT);
// ...订阅其它消息...
shared_ptr<Caps> msg = Caps::new_instance();
// ... write some data to 'msg' ...
cli.post("some_msg", msg, FLORA_MSGTYPE_INSTANT);
```

## 服务端

```
using namespace flora;

shared_ptr<Dispatcher> disp = Dispatcher::new_instance();
shared_ptr<Poll> tcp_poll = Poll::new_instance("tcp://0.0.0.0:5678/");
if (tcp_poll->start(disp) != FLORA_POLL_SUCCESS) {
  // TODO: 错误处理
}
```

# <a id="anchor03"></a>消息类型

名称 | 值 | 描述
--- | --- | ---
FLORA_MSGTYPE_INSTANT | 0 | 消息发送后，被当前已经订阅此消息的客户端接收
FLORA_MSGTYPE_PERSIST | 1 | 消息发送后，被当前已经订阅此消息的客户端接收，同时在服务端保留副本。<br>在此后任意时刻，任一客户端订阅此消息，将马上收到此消息一次。<br>每次发送此消息，都会更新服务端保存的副本。
FLORA_MSGTYPE_REQUEST | 2 | 消息发送后，当前已经订阅此消息的客户端接收并需要返回一段自定义数据。<br>消息的发送端将等待订阅客户端（可能0或多个）返回的自定义数据，可设置等待超时时间。

# <a id="anchor05"></a>uri格式

scheme:[//host[:port]][/path][#fragment]

```
scheme
  tcp  -  tcp socket

path
  /  -  目前固定为/，不支持其它值

fragment
  客户端自定义身份标识
```
