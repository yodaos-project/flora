# 1. Agent

```
flora_agent_t agent = flora_agent_create();
// #exam-agent为可选项，标识此agent身份
flora_agent_config(agent, FLORA_AGENT_CONFIG_URI, "unix:/var/run/flora.sock#exam-agent");
flora_agent_config(agent, FLORA_AGENT_CONFIG_BUFSIZE, 80 * 1024);
flora_agent_config(agent, FLORA_AGENT_CONFIG_RECONN_INTERVAL, 5000);

static void foo_callback(caps_t msg, uint32_t type, flora_reply_t* reply, void* arg) {
	int32_t iv;
  char* str;
	caps_read_integer(msg, &iv);  // read integer 1
	caps_read_string(msg, &str);  // read string "hello"
	if (type == FLORA_MSGTYPE_REQUEST) {
		// fill 'reply' content here
		// message sender will received the reply content
		reply->ret_code = 0;
		reply->data = caps_create();
		caps_write_string(reply->data, "world");
	}
}
// 最后参数(void*)1 为可选项，将传入foo_callback
flora_agent_subscribe(agent, "foo", foo_callback, (void*)1);
flora_agent_start(agent, 0);
caps_t msg = caps_create();
caps_write_integer(msg, 1);
caps_write_string(msg, "hello");
flora_agent_post(agent, "foo", msg, FLORA_MSGTYPE_INSTANT);
flora_get_result* results;
uint32_t res_size;
if (flora_agent_get(agent, "foo", msg, &results, &res_size, 0) == FLORA_CLI_SUCCESS) {
  uint32_t i;
  for (i = 0; i < res_size; ++i) {
    results[i].ret_code;
    results[i].data;
    results[i].sender;
  }
  flora_result_delete(results, res_size);
}

static void get_foo_callback(flora_result_t* results, uint32_t size, void* arg) {
	// results[0].ret_code;  // ret_code will be 0
	// results[0].sender;  // sender will be "exam-agent"
	char* str;
	caps_read_string(results[0].data, &str);  // str will be "world"
  flora_result_delete(results, size);
}
// 最后参数(void*)2 为可选项，将传入get_foo_callback
flora_agent_get_nb(agent, "foo", msg, get_foo_callback, (void*)2);
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

### flora\_agent\_get(agent, name, msg, results, res_size, timeout)

发送消息并等待另一客户端回复消息

#### Parameters

name | type | default | description
--- | --- | --- | ---
agent | flora\_agent\_t | |
name | const char* | | 消息名称
msg | [caps_t](https://github.com/Rokid/aife-mutils/blob/master/caps.md) | | 消息内容
results | [flora\_result\_t](#Result)** | | 收到的回复消息
res_size | uint32_t* | | results size
timeout | uint32_t | 0 | 等待回复的超时时间，0表示无超时。

#### returns

Type: int32_t

value | description
--- | ---
FLORA\_CLI\_SUCCESS | 成功
FLORA\_CLI\_EINVAL | 参数非法
FLORA\_CLI\_ECONN | flora service连接错误
FLORA\_CLI\_ETIMEOUT | 超时无回复

---

### flora\_agent\_get(agent, name, msg, cb, arg)

发送消息并等待另一客户端回复消息

#### Parameters

name | type | default | description
--- | --- | --- | ---
agent | flora\_agent\_t | |
name | const char* | | 消息名称
msg | [caps_t](https://github.com/Rokid/aife-mutils/blob/master/caps.md) | | 消息内容
cb | [flora\_agent\_get\_callback\_t](#GetCallback) | | 回调函数
arg | void* | |

#### returns

Type: int32_t

value | description
--- | ---
FLORA_CLI_SUCCESS | 成功
FLORA_CLI_EINVAL | 参数非法
FLORA_CLI_ECONN | flora service连接错误

---

## Definition

### <a id="SubscribeCallback"></a>flora\_agent\_subscribe\_callback\_t(msg, type, reply, arg)

回调函数：收到订阅的消息

#### Parameters

name | type | description
--- | --- | ---
msg | [caps_t](https://github.com/Rokid/aife-mutils/blob/master/caps.md) | | 消息内容
type | uint32_t | 消息类型<br>FLORA_MSGTYPE_INSTANT<br>FLORA_MSGTYPE_PERSIST<br>FLORA_MSGTYPE_REQUEST
reply | [flora\_reply\_t](#Reply)\* | 当type == FLORA_MSGTYPE_REQUEST时，填充reply指向的结构体，给消息发送者回复数据
arg | void* | | subscribe传入的参数arg

### <a id="GetCallback"></a>flora_agent\_get\_callback\_t(results, size, arg)

回调函数：收到回复的消息

#### Parameters

name | type | description
--- | --- | ---
results | [flora\_result\_t](#Result)** | | 回复的消息数组<br>'get'请求的每个订阅者回复一个消息内容，所以数组的长度等于消息的订阅者数量。
size | uint32_t* | | results size
arg | void* | | get传入的参数arg

### <a id="Reply"></a>flora\_reply\_t

#### Members

name | type | description
--- | --- | ---
ret_code | int32_t | 返回码，0为成功。
data | [caps_t](https://github.com/Rokid/aife-mutils/blob/master/caps.md) | | 消息内容

### <a id="Response"></a>flora\_result\_t

#### Members

name | type | description
--- | --- | ---
ret_code | int32_t | 返回码，由消息订阅者设置，0为成功。
data | [caps_t](https://github.com/Rokid/aife-mutils/blob/master/caps.md) | | 消息内容
sender | const char* | 消息订阅者的身份标识，可选项。

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

### flora_dispatcher_delete(dispatcher)

销毁Dispatcher对象

#### Parameters

name | type | default | description
--- | --- | --- | ---
dispatcher | flora_dispatcher_t | |

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
