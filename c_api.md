# 客户端接口定义

**\[\[flora\_cli\_connect]]**

生成flora_cli_t对象并连接服务端

* 返回值: int32_t  错误码(详见c++接口文档)

* 参数  const char* uri

uri格式(详见c++接口文档)

```
连接的服务端uri
```

* 参数  [flora_cli_callback_t](#anchor01)* callback

```
回调函数列表
```

* 参数  void* arg

```
回调函数附加参数
```

* 参数  uint32_t msg\_buf\_size

```
接收/发送消息的缓冲区大小
填0设置为默认值
```

* 参数  flora\_cli\_t* result

```
连接成功后，被设置为生成的flora_cli_t对象
```

**\[\[flora\_cli\_delete]]**

删除客户端对象

* 参数  flora\_cli\_t handle

**\[\[flora\_cli\_subscribe]]**

订阅消息

* 返回值: int32_t 错误码(详见c++接口文档)

```
FLORA_CLI_SUCCESS
FLORA_CLI_EINVAL
```

* 参数  flora\_cli\_t handle

* 参数  const char* name

```
订阅的消息名称
```

* 参数  uint32_t msgtype  消息类型(详见c++接口文档)

```
订阅的消息类型
```

**\[\[unsubscribe]]**

取消订阅

* 返回值: int32_t 错误码(详见c++接口文档)

```
FLORA_CLI_SUCCESS
FLORA_CLI_EINVAL
```

* 参数  flora\_cli\_t handle

* 参数  const char* name

* 参数  uint32_t msgtype  消息类型(详见c++接口文档)

**\[\[flora\_cli\_post]]**

发送INSTANT/PERSIST消息

* 参数  flora\_cli\_t handle

* 参数 const char* name

* 参数 [caps_t](https://github.com/Rokid/aife-mutils/blob/master/caps.md)& msg

```
发送的消息参数
Caps对象，支持多种数据类型的数据结构序列化工具
```

* 参数  uint32_t msgtype  消息类型(详见c++接口文档)

**\[\[flora\_cli\_get]]**

发送REQUEST消息，并等待回复

* 参数  flora\_cli\_t handle

* 参数  const char* name

* 参数  [caps_t](https://github.com/Rokid/aife-mutils/blob/master/caps.md)& msg

* 参数  [flora_get_result](#anchor02)** replys

```
回复的消息，可能有0或多个订阅客户端回复
```

* 参数  uint32_t* num_replys

```
回复的消息个数
```

* 参数 uint32_t timeout

```
等待回复的超时时间，单位毫秒
```

## <a id="anchor01"></a>flora_cli_callback_t

回调函数指针列表

**\[\[recv_post]]**

接收INSTANT/PERSIST消息

* 参数 const char* name

* 参数  uint32_t msgtype  [消息类型](#anchor03)

* 参数 [caps_t](https://github.com/Rokid/aife-mutils/blob/master/caps.md) msg

* 参数 void* arg

```
flora_cli_connect时传入的附加参数
```

**\[\[recv_get]]**

接收REQUEST消息并回复

* 返回值: int32_t  自定义返回码，此值将发回到此REQUEST消息的发送端

* 参数 const char* name

* 参数 [caps_t](https://github.com/Rokid/aife-mutils/blob/master/caps.md) msg

* 参数 [caps_t](https://github.com/Rokid/aife-mutils/blob/master/caps.md)* reply

```
发回到此REQUEST消息的发送端的自定义消息体
应在'recv_get'函数中填充
```

* 参数 void* arg

```
flora_cli_connect时传入的附加参数
```

**\[\[disconnected]]**

客户端断开连接

* 参数 void* arg

```
flora_cli_connect时传入的附加参数
```

## <a id="anchor02"></a>flora_get_result

数据类型 | 名称 | 描述
--- | --- | ---
int32_t | ret_code | 返回码<br>0: 正确<br>其它: 订阅客户端自定义错误
caps_t | msg | 返回的消息体，内容为回复此消息的客户端自定义
char* | extra | 回复此消息的客户端的身份标识

**\[\[flora_result_delete]]**

删除flora_get_result数组

* 参数  flora\_get\_result* results

* 参数  uint32_t size

# 服务端接口定义

与c++接口类似，请参见c++接口文档。

# 示例

## 客户端

```
flora_cli_callback_t cb;
memset(&cb, 0, sizeof(cb));
// 设置回调函数指针
cb.recv_post = ...;
cb.recv_get = ...;
cb.disconnected = ...;

void* cb_arg = ...;
flora_cli_t cli;
if (flora_cli_connect("tcp://localhost:5678/#foo1", &cb, cb_arg, 0, &cli) != FLORA_CLI_SUCCESS) {
  // TODO: 错误处理
}
flora_cli_subscribe(cli, "hello", FLORA_MSGTYPE_INSTANT);
// ...订阅其它消息...
caps_t msg = caps_create();
// ... write some data to 'msg' ...
flora_cli_post("some_msg", msg, FLORA_MSGTYPE_INSTANT);

flora_cli_delete(cli);
```

## 服务端

```
flora_dispatcher_t disp = flora_dispatcher_new(0);
flora_poll_t tcp_poll;
if (flora_poll_new("tcp://0.0.0.0:5678/", &tcp_poll) != FLORA_POLL_SUCCESS) {
  // TODO: 错误处理
}
if (flora_poll_start(disp) != FLORA_POLL_SUCCESS) {
  // TODO: 错误处理
}

flora_poll_delete(tcp_poll);
flora_dispathcer_delete(disp);
```
