# Agent

flora client代理类，自动保持与flora service连接及订阅状态。

```
Agent agent;
// #exam-agent为可选项，标识此agent身份
agent.config(FLORA_AGENT_CONFIG_URI, "unix:/var/run/flora.sock#exam-agent");
agent.config(FLORA_AGENT_CONFIG_BUFSIZE, 80 * 1024);
agent.config(FLORA_AGENT_CONFIG_RECONN_INTERVAL, 5000);
agent.subscribe("foo", [](shared_ptr<Caps>& msg, uint32_t type, Reply* reply) {
	int32_t iv;
	string str;
	msg->read(iv);  // read integer 1
	msg->read_string(str);  // read string "hello"
	if (type == FLORA_MSGTYPE_REQUEST) {
		// fill 'reply' content here
		// message sender will received the reply content
		reply->ret_code = 0;
		reply->data = Caps::new_instance();
		reply->data->write("world");
	}
});
agent.start();
shared_ptr<Caps> msg = Caps::new_instance();
msg.write(1);
msg.write("hello");
agent.post("foo", msg, FLORA_MSGTYPE_INSTANT);
ResponseArray resps;
agent.get("foo", msg, resps, 0);
agent.get("foo", msg, [](ResponseArray& resps) {
	// resps[0].ret_code;  // ret_code will be 0
	// resps[0].sender;  // sender will be "exam-agent"
	string str;
	resps[0].data->read_string(str);  // str will be "world"
});
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

### get(name, msg, responses, timeout)

发送消息并等待另一客户端回复消息

#### Parameters

name | type | default | description
--- | --- | --- | ---
name | const char* | | 消息名称
msg | shared_ptr\<[Caps](https://github.com/Rokid/aife-mutils/blob/master/caps.md)>& | | 消息内容
responses | vector<[Response](#Response)>& | | 收到的回复消息
timeout | uint32_t | 0 | 等待回复的超时时间，0表示无超时。

#### returns

Type: int32_t

value | description
--- | ---
FLORA_CLI_SUCCESS | 成功
FLORA_CLI_EINVAL | 参数非法
FLORA_CLI_ECONN | flora service连接错误
FLORA_CLI_ETIMEOUT | 超时无回复

---

### get(name, msg, cb)

发送消息并等待另一客户端回复消息

#### Parameters

name | type | default | description
--- | --- | --- | ---
name | const char* | | 消息名称
msg | shared_ptr\<[Caps](https://github.com/Rokid/aife-mutils/blob/master/caps.md)>& | | 消息内容
cb | [GetCallback](#GetCallback) | | 回调函数

#### returns

Type: int32_t

value | description
--- | ---
FLORA_CLI_SUCCESS | 成功
FLORA_CLI_EINVAL | 参数非法
FLORA_CLI_ECONN | flora service连接错误

---

## Definition

### <a id="SubscribeCallback"></a>SubscribeCallback(msg, type, reply)

回调函数：收到订阅的消息

#### Parameters

name | type | description
--- | --- | ---
msg | shared_ptr\<[Caps](https://github.com/Rokid/aife-mutils/blob/master/caps.md)>& | 消息内容
type | uint32_t | 消息类型<br>FLORA_MSGTYPE_INSTANT<br>FLORA_MSGTYPE_PERSIST<br>FLORA_MSGTYPE_REQUEST
reply | [Reply](#Reply)\* | 当type == FLORA_MSGTYPE_REQUEST时，填充reply指向的结构体，给消息发送者回复数据

### <a id="GetCallback"></a>GetCallback(responses)

回调函数：收到回复的消息

#### Parameters

name | type | description
--- | --- | ---
responses | vector\<[Response](#Response)>& | 回复的消息数组<br>'get'请求的每个订阅者回复一个消息内容，所以数组的长度等于消息的订阅者数量。

### <a id="Reply"></a>Reply

#### Members

name | type | description
--- | --- | ---
ret_code | int32_t | 返回码，0为成功。
data | shared_ptr\<[Caps](https://github.com/Rokid/aife-mutils/blob/master/caps.md)>& | 消息内容

### <a id="Response"></a>Response

#### Members

name | type | description
--- | --- | ---
ret_code | int32_t | 返回码，由消息订阅者设置，0为成功。
data | shared_ptr\<[Caps](https://github.com/Rokid/aife-mutils/blob/master/caps.md)>& | 消息内容
sender | string | 消息订阅者的身份标识，可选项。
