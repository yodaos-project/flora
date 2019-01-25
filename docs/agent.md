# Agent

flora client代理类，自动保持与flora service连接及订阅状态。

```
Agent agent;
// #exam-agent为可选项，标识此agent身份
agent.config(FLORA_AGENT_CONFIG_URI, "unix:/var/run/flora.sock#exam-agent");
agent.config(FLORA_AGENT_CONFIG_BUFSIZE, 80 * 1024);
agent.config(FLORA_AGENT_CONFIG_RECONN_INTERVAL, 5000);
// 订阅消息
agent.subscribe("foo", [](const char* name, shared_ptr<Caps>& msg, uint32_t type) {
	int32_t iv;
	string str;
	msg->read(iv);  // read integer 1
	msg->read_string(str);  // read string "hello"
});
// 声明远程方法
agent.declare_method("foo", [](const char* name, shared_ptr<Caps>& msg, shared_ptr<Reply>& reply) {
	// use 'reply->end()' to send return values to caller
  reply->write_code(FLORA_CLI_SUCCESS);
  shared_ptr<Caps> data = Caps::new_instance();
	data->write("world");
  reply->write_data(data);
  reply->end();
});

agent.start();

// 发送广播消息
shared_ptr<Caps> msg = Caps::new_instance();
msg.write(1);
msg.write("hello");
agent.post("foo", msg, FLORA_MSGTYPE_INSTANT);

// 远程方法调用
Response resp;
// blocking call(msgname, msg, target, response, timeout)
// NOTE: timeout set to 0 will use default timeout
int32_t r = agent.call("foo", msg, "exam-agent", resp, 0);
if (r == FLORA_CLI_SUCCESS) {
	string str;
	resp.data->read(str);
	printf("recv call foo reply: %d, %s\n", res.ret_code, str.c_str());
}
// non-blocking call(msgname, msg, target, callback, timeout)
agent.call("foo", msg, "exam-agent", [](int32_t rescode, Response& resp) {
	if (rescode == FLORA_CLI_SUCCESS) {
		// resp.ret_code;  // ret_code will be 0
		// resp.sender;  // sender will be "exam-agent"
		string str;
		resp.data->read(str);  // str will be "world"
	}
}, 0);
```

## Methods

### config(key, ...)

为Agent对象配置参数

#### Parameters

name | type | default | description
--- | --- | --- | ---
key | uint32_t | | FLORA_AGENT_CONFIG_URI<br>FLORA_AGENT_CONFIG_BUFSIZE<br>FLORA_AGENT_CONFIG_RECONN_INTERVAL

---

### subscribe(name, cb)

订阅消息并指定收到消息的回调函数

#### Parameters

name | type | default | description
--- | --- | --- | ---
name | const char* | | 消息名称
cb | [SubscribeCallback](#SubscribeCallback) | | 回调函数

---

### unsubscribe(name)

取消订阅

#### Parameters

name | type | default | description
--- | --- | --- | ---
name | const char* | | 消息名称

---

### declare_method(name, cb)

声明远程调用方法，可接受远程方法调用并返回数据

#### Parameters

name | type | default | description
--- | --- | --- | ---
name | const char* | | 方法名称
cb | [DeclareMethodCallback](#DeclareMethodCallback) | | 回调函数

---

### remove_method(name)

删除远程调用方法

#### Parameters

name | type | default | description
--- | --- | --- | ---
name | const char* | | 方法名称

---

### start(block)

启动Agent。需要在config指定uri后调用。可以在subscribe之后或之前调用。

#### Parameters

name | type | default | description
--- | --- | --- | ---
block | bool | false | 阻塞模式开关。如果开启阻塞模式，start不会返回，直至调用close方法。

---

### close()

关闭Agent。

---

### post(name, msg, type)

发送消息

#### Parameters

name | type | default | description
--- | --- | --- | ---
name | const char* | | 消息名称
msg | shared_ptr\<[Caps](https://github.com/Rokid/aife-mutils/blob/master/caps.md)>& | | 消息内容
type | uint32_t | FLORA_MSGTYPE_INSTANT | 消息类型<br>FLORA_MSGTYPE_INSTANT<br>FLORA_MSGTYPE_PERSIST

#### returns

Type: int32_t

value | description
--- | ---
FLORA_CLI_SUCCESS | 成功
FLORA_CLI_EINVAL | 参数非法
FLORA_CLI_ECONN | flora service连接错误

---

### call(name, msg, target, response, timeout)

向另一客户端发起远程方法调用，等待返回结果。

#### Parameters

name | type | default | description
--- | --- | --- | ---
name | const char* | | 远程方法名称
msg | shared_ptr\<[Caps](https://github.com/Rokid/aife-mutils/blob/master/caps.md)>& | | 方法参数
target | const char* | | 声明远程方法的客户端id
response | [Response](#Response) | | 远程方法的返回
timeout | uint32_t | 0 | 等待回复的超时时间，0表示使用默认超时时间。

#### returns

Type: int32_t

value | description
--- | ---
FLORA_CLI_SUCCESS | 成功
FLORA_CLI_EINVAL | 参数非法
FLORA_CLI_ECONN | flora service连接错误
FLORA_CLI_ENEXISTS | 找不到此远程调用方法
FLORA_CLI_ETIMEOUT | 超时无回复
FLORA_CLI_EDEADLOCK | 在回调函数中调用此方法，将造成无限阻塞

---

### call(name, msg, target, cb, timeout)

发送消息并等待另一客户端回复消息

#### Parameters

name | type | default | description
--- | --- | --- | ---
name | const char* | | 远程方法名称
msg | shared_ptr\<[Caps](https://github.com/Rokid/aife-mutils/blob/master/caps.md)>& | | 方法参数
target | const char* | | 声明远程方法的客户端id
cb | [CallCallback](#CallCallback) | | 回调函数
timeout | uint32_t | 0 | 等待回复的超时时间，0表示使用默认超时时间。

#### returns

Type: int32_t

value | description
--- | ---
FLORA_CLI_SUCCESS | 成功
FLORA_CLI_EINVAL | 参数非法
FLORA_CLI_ECONN | flora service连接错误

---

## Definition

### <a id="SubscribeCallback"></a>SubscribeCallback(name, msg, type)

回调函数：收到订阅的消息

#### Parameters

name | type | description
--- | --- | ---
name | string | 消息名称
msg | shared_ptr\<[Caps](https://github.com/Rokid/aife-mutils/blob/master/caps.md)>& | 消息内容
type | uint32_t | 消息类型<br>FLORA_MSGTYPE_INSTANT<br>FLORA_MSGTYPE_PERSIST

### <a id="DeclareMethodCallback"></a>DeclareMethodCallback(name, msg, reply)

回调函数：收到远程方法调用

#### Parameters

name | type | description
--- | --- | ---
name | string | 消息名称
msg | shared_ptr\<[Caps](https://github.com/Rokid/aife-mutils/blob/master/caps.md)>& | 消息内容
reply | shared_ptr\<[Reply](reply.md)\>& | reply对象，给远程方法调用者返回数据

### <a id="CallCallback"></a>CallCallback(int32_t, response)

回调函数：收到回复的消息

#### Parameters

name | type | description
--- | --- | ---
rescode | int32_t | 远程方法调用错误码
response | [Response](#Response)& | 远程调用返回结果

### <a id="Response"></a>Response

#### Members

name | type | description
--- | --- | ---
ret_code | int32_t | 返回码，远程方法返回，0为成功。
data | shared_ptr\<[Caps](https://github.com/Rokid/aife-mutils/blob/master/caps.md)>& | 消息内容
sender | string | 远程方法声明方的身份标识
