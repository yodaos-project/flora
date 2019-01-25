# 1. Agent

```
flora_agent_t agent = flora_agent_create();
// #exam-agent为可选项，标识此agent身份
flora_agent_config(agent, FLORA_AGENT_CONFIG_URI, "unix:/var/run/flora.sock#exam-agent");
flora_agent_config(agent, FLORA_AGENT_CONFIG_BUFSIZE, 80 * 1024);
flora_agent_config(agent, FLORA_AGENT_CONFIG_RECONN_INTERVAL, 5000);

static void foo_sub_callback(const char* name, caps_t msg, uint32_t type, void* arg) {
	int32_t iv;
	char* str;
	caps_read_integer(msg, &iv);  // read integer 1
	caps_read_string(msg, &str);  // read string "hello"
}
static void foo_method_callback(const char* name, caps_t msg, flora_call_reply_t reply, void* arg) {
	// use 'flora_call_reply_end(reply)' to send return values to caller
  caps_t data = caps_create();
  caps_write_string(data, "world");
  flora_call_reply_write_code(reply, FLORA_CLI_SUCCESS);
  flora_call_reply_write_data(reply, data);
  flora_call_reply_end(reply);
  caps_destroy(data);
}

// 最后参数(void*)1 为可选项，将传入foo_sub_callback
flora_agent_subscribe(agent, "foo", foo_sub_callback, (void*)1);
flora_agent_declare_method(agent, "foo", foo_method_callback, (void*)2);

// flora_agent_start(agent, bufsize), bufsize minimal value 32768.
// if 'bufsize' < minimal value, 'bufsize' will set to minimal value.
flora_agent_start(agent, 0);

// post message
caps_t msg = caps_create();
caps_write_integer(msg, 1);
caps_write_string(msg, "hello");
flora_agent_post(agent, "foo", msg, FLORA_MSGTYPE_INSTANT);

// rpc call
flora_call_result result;
// flora_agent_call(agent, methodName, methodParams, targetName, result, timeout)
// if timeout == 0, use default timeout
if (flora_agent_call(agent, "foo", msg, "exam-agent", &result, 0) == FLORA_CLI_SUCCESS) {
	result->ret_code;
	result->data;
	result->sender;
	flora_result_delete(&result);
}

static void foo_return_callback(int32_t rescode, flora_result_t* result, void* arg) {
	if (rescode == FLORA_CLI_SUCCESS) {
		// result->ret_code;  // ret_code will be 0
		// result->sender;  // sender will be "exam-agent"
		char* str;
		caps_read_string(result->data, &str);  // str will be "world"
	}
}
// flora_agent_call_nb(agent, methodName, methodParams, targetName, callback, arg, timeout)
// 如果超时，foo_return_callback第一个参数rescode值为FLORA_CLI_ETIMEOUT, result参数为空指针
// 最后参数(void*)2 为可选项，将传入foo_return_callback
flora_agent_call_nb(agent, "foo", msg, "exam-agent", foo_return_callback, (void*)2, 0);
caps_destroy(msg);
flora_agent_close(agent);
flora_agent_delete(agent);
```

## Methods

### flora\_agent\_config(agent, key, ...)

为Agent对象配置参数

#### Parameters

name | type | default | description
--- | --- | --- | ---
agent | flora\_agent\_t | | agent对象
key | uint32_t | | FLORA\_AGENT\_CONFIG\_URI<br>FLORA\_AGENT\_CONFIG\_BUFSIZE<br>FLORA\_AGENT\_CONFIG\_RECONN\_INTERVAL

---

### flora\_agent\_subscribe(agent, name, cb, arg)

订阅消息并指定收到消息的回调函数

#### Parameters

name | type | default | description
--- | --- | --- | ---
agent | flora\_agent\_t | |
name | const char* | | 消息名称
cb | [flora\_agent\_subscribe\_callback\_t](#SubscribeCallback) | | 回调函数
arg | void* | |

---

### flora\_agent\_unsubscribe(agent, name)

取消订阅

#### Parameters

name | type | default | description
--- | --- | --- | ---
agent | flora\_agent\_t | |
name | const char* | | 消息名称

---

### flora\_agent\_declare\_method(agent, name, cb, arg)

声明远程方法

#### Parameters

name | type | default | description
--- | --- | --- | ---
agent | flora\_agent\_t | |
name | const char* | | 远程方法名称
cb | [flora\_agent\_declare\_method\_callback\_t](#DeclareMethodCallback) | | 回调函数
arg | void* | |

---

### flora\_agent\_start(agent, block)

启动Agent。需要在config指定uri后调用。可以在subscribe之后或之前调用。

#### Parameters

name | type | default | description
--- | --- | --- | ---
agent | flora\_agent\_t | |
block | int32_t | | 阻塞模式开关。如果开启阻塞模式，start不会返回，直至调用close方法。

---

### flora\_agent\_close(agent)

关闭Agent。

#### Parameters

name | type | default | description
--- | --- | --- | ---
agent | flora\_agent\_t | |

---

### flora\_agent\_post(agent, name, msg, type)

发送消息

#### Parameters

name | type | default | description
--- | --- | --- | ---
agent | flora\_agent\_t | |
name | const char* | | 消息名称
msg | [caps_t](https://github.com/Rokid/aife-mutils/blob/master/caps.md) | | 消息内容
type | uint32_t | FLORA\_MSGTYPE\_INSTANT | 消息类型<br>FLORA\_MSGTYPE\_INSTANT<br>FLORA\_MSGTYPE\_PERSIST

#### returns

Type: int32_t

value | description
--- | ---
FLORA\_CLI\_SUCCESS | 成功
FLORA\_CLI\_EINVAL | 参数非法
FLORA\_CLI\_ECONN | flora service连接错误

---

### flora\_agent\_call(agent, name, msg, target, result, timeout)

远程方法调用(同步)

#### Parameters

name | type | default | description
--- | --- | --- | ---
agent | flora\_agent\_t | |
name | const char* | | 远程方法名称
msg | [caps_t](https://github.com/Rokid/aife-mutils/blob/master/caps.md) | | 方法参数
target | const char* | | 远程方法声明客户端id
result | [flora\_result\_t](#Result)* | | 远程方法返回信息
timeout | uint32_t | 0 | 等待回复的超时时间，0表示使用默认超时。

#### returns

Type: int32_t

value | description
--- | ---
FLORA\_CLI\_SUCCESS | 成功
FLORA\_CLI\_EINVAL | 参数非法
FLORA\_CLI\_ECONN | flora service连接错误
FLORA\_CLI\_ETIMEOUT | 超时无回复
FLORA\_CLI\_ENEXISTS | 远程方法未找到
FLORA_CLI_EDEADLOCK | 在回调函数中调用此方法，将造成无限阻塞

---

### flora\_agent\_call\_nb(agent, name, msg, target, cb, arg, timeout)

远程方法调用(异步回调)

#### Parameters

name | type | default | description
--- | --- | --- | ---
agent | flora\_agent\_t | |
name | const char* | | 远程方法名称
msg | [caps_t](https://github.com/Rokid/aife-mutils/blob/master/caps.md) | | 方法参数
target | const char* | | 远程方法声明客户端id
cb | [flora\_call\_callback\_t](#CallCallback) | | 回调函数
arg | void* | |
timeout | uint32_t | 0 | 等待回复的超时时间，0表示使用默认超时。

#### returns

Type: int32_t

value | description
--- | ---
FLORA_CLI_SUCCESS | 成功
FLORA_CLI_EINVAL | 参数非法
FLORA_CLI_ECONN | flora service连接错误

---

### <a id="Reply"></a>flora_call_reply_write_code

设置远程函数返回码

#### Parameters

name | type | default | description
--- | --- | --- | ---
reply | flora_call_reply_t | | 详见[flora\_agent\_declare\_method\_callback\_t](#DeclareMethodCallback)
code | int32_t | 0 |

---

### flora_call_reply_write_data

设置远程函数返回值

#### Parameters

name | type | default | description
--- | --- | --- | ---
reply | flora_call_reply_t | | 详见[flora\_agent\_declare\_method\_callback\_t](#DeclareMethodCallback)
data | [caps_t](https://github.com/Rokid/aife-mutils/blob/master/caps.md) | | 消息内容

---

### flora_call_reply_end

销毁flora_call_reply_t对象并将返回码与返回值发送至flora服务，flora服务将发送给远程函数调用者

#### Parameters

name | type | default | description
--- | --- | --- | ---
reply | flora_call_reply_t | | 详见[flora\_agent\_declare\_method\_callback\_t](#DeclareMethodCallback)

---

## Definition

### <a id="SubscribeCallback"></a>flora\_agent\_subscribe\_callback\_t(name, msg, type, arg)

回调函数：收到订阅的消息

#### Parameters

name | type | description
--- | --- | ---
name | string | 消息名称
msg | [caps_t](https://github.com/Rokid/aife-mutils/blob/master/caps.md) | | 消息内容
type | uint32_t | 消息类型<br>FLORA_MSGTYPE_INSTANT<br>FLORA_MSGTYPE_PERSIST
arg | void* | subscribe传入的参数arg

### <a id="DeclareMethodCallback"></a>flora\_agent\_declare\_method\_callback\_t(name, msg, reply, arg)

回调函数：远程方法被调用

#### Parameters

name | type | description
--- | --- | ---
name | string | 远程方法名称
msg | [caps_t](https://github.com/Rokid/aife-mutils/blob/master/caps.md) | | 方法参数
reply | [flora\_call_reply\_t](#Reply) | reply对象，通过flora_call_reply_\*系列函数给远程方法调用者返回数据
arg | void* | declare_method传入的参数arg

### <a id="CallCallback"></a>flora_call\_callback\_t(rescode, result, arg)

回调函数：远程方法调用返回值

#### Parameters

name | type | description
--- | --- | ---
rescode | int32_t | 远程方法调用错误码
result | [flora\_result\_t](#Result)* | 远程方法调用返回值
arg | void* | call_nb传入的参数arg

### <a id="Response"></a>flora\_result\_t

#### Members

name | type | description
--- | --- | ---
ret_code | int32_t | 返回码，由消息订阅者设置，0为成功。
data | [caps_t](https://github.com/Rokid/aife-mutils/blob/master/caps.md) | 远程方法返回数据
sender | string | 远程方法定义者身份标识

# 2. <a id="dispatcher"></a>Dispatcher

flora service消息分发模块，需与[Poll](#poll)配合使用

## Methods

### flora_dispatcher_new(bufsize)

创建Dispatcher对象

#### Parameters

name | type | default | description
--- | --- | --- | ---
bufsize | uint32_t | 0 | 消息缓冲区大小（决定了一个消息最大大小），最小值32K，小于32K的值会被改为32K。

#### Returns

Type: flora_dispatcher_t

---

### flora_dispatcher_delete(dispatcher)

销毁Dispatcher对象

#### Parameters

name | type | default | description
--- | --- | --- | ---
dispatcher | flora_dispatcher_t | |

---

### flora_dispatcher_run(dispatcher, block)

开始运行

#### Parameters

name | type | default | descriptions
--- | --- | --- | ---
dispatcher | flora_dispatcher_t | |
block | int32_t | | 0: 异步运行模式，此函数立即返回<br>1: 同步运行模式，此函数阻塞直至flora_dispatcher_close被调用

---

### flora_dispatcher_close(dispatcher)

停止运行

#### Parameters

name | type | default | descriptions
--- | --- | --- | ---
dispatcher | flora_dispatcher_t | |

---

# 3. <a id="poll"></a>Poll

flora service连接管理，需与[Dispatcher](#dispatcher)配合使用

## Methods

### flora_poll_new(uri, result)

创建Poll对象

#### Parameters

name | type | default | description
--- | --- | --- | ---
uri | const char* | | uri - 服务侦听地址<br>支持unix domain socket及tcp socket
result | flora_poll_t* | | 创建的Poll对象

#### Returns

Type: int32_t

value | description
--- | ---
FLORA_POLL_SUCCESS | 成功
FLORA_POLL_INVAL | 参数不合法
FLORA_POLL_UNSUPP | uri scheme不支持(目前仅支持unix:及tcp:)

### flora_poll_delete(poll)

#### Parameters

name | type | default | description
--- | --- | --- | ---
poll | flora_poll_t | |

### flora_poll_start(poll, dispatcher)

启动Poll，侦听服务地址

#### Parameters

name | type | default | description
--- | --- | --- | ---
poll | flora_poll_t | |
dispatcher | flora_dispatcher_t | |

#### Returns

Type: int32_t

value | description
--- | ---
FLORA_POLL_SUCCESS | 成功
FLORA_POLL_INVAL | 参数不合法，poll或dispatcher对象句柄为0
FLORA_POLL_SYSERR | socket或其它系统调用错误

### flora_poll_stop(poll)

停止侦听并关闭所有连接

#### Parameters

name | type | default | description
--- | --- | --- | ---
poll | flora_poll_t | |
