# Client

flora客户端低级接口，与Agent功能相同，建议使用Agent.

```
class FloraCallback : public ClientCallback {
public:
	void recv_post(const char *name, uint32_t msgtype, shared_ptr<Caps> &msg) {
		// name == "foo"
		// msgtype == FLORA_MSGTYPE_INSTANT
		int32_t iv;
		string str;
		msg->read(iv);  // read integer 1
		msg->read_string(str);  // read string "hello"
	}
	
	void recv_call(const char *name, shared_ptr<Caps> &msg, Reply &reply) {
		// name == "foo"
		// fill 'reply' content here
		// caller will receive the reply content
		reply.ret_code = 0;
		reply.data = Caps::new_instance();
		reply.data->write("world");	
	}
	
	void disconnected() {
	}
};

shared_ptr<flora::Client> floraClient;
FloraCallback floraCallback;
flora::Client::connect("unix:/var/run/flora.sock#exam-client", floraCallback, 80 * 1024, floraClient);

floraClient->subscribe("foo");
floraClient->declare_method("foo");


// 发送广播消息
shared_ptr<Caps> msg = Caps::new_instance();
msg.write(1);
msg.write("hello");
floraClient->post("foo", msg, FLORA_MSGTYPE_INSTANT);
// 调用远程方法并等待结果返回
Response resp;
int32_t r = floraClient->call("foo", msg, "exam-client", resp, 0);
if (r == FLORA_CLI_SUCCESS) {
	string str;
	// resp.ret_code == 0
	resp.data->read(str);  // str == "world"
}
// 异步调用远程方法，在callback函数中得到返回结果
floraClient->call("foo", msg, "exam-client", [](int32_t rescode, Response &resp) {
	if (rescode == FLORA_CLI_SUCCESS) {
		// resp.ret_code == 0
		string str;
		resp.data->read(str); // str == "world"
	}
}, 0);
```

## Warning!!!

**Don't destruct flora::Client instance in callback function, may cause program crash**

## Methods

### <font color=#bdbdbd>(static)</font> connect(uri, cb, bufsize, result)

连接flora服务，创建flora::Client对象

#### Parameters

name | type | default | description
--- | --- | --- | ---
uri | string | | flora服务uri
cb | [flora::ClientCallback](#ClientCallback) | |
bufsize | uint32_t | | 消息缓冲大小
result | shared_ptr\<flora::Client\> & | | 创建的flora::Client对象

---

### subscribe(name)

订阅消息

#### Parameters

name | type | default | description
--- | --- | --- | ---
name | const char* | | 消息名称

---

### unsubscribe(name)

取消订阅

#### Parameters

name | type | default | description
--- | --- | --- | ---
name | const char* | | 消息名称

---

### declare_method(name)

声明远程调用方法

#### Parameters

name | type | default | description
--- | --- | --- | ---
name | const char* | | 方法名称

---

### remove_method(name)

删除远程调用方法

#### Parameters

name | type | default | description
--- | --- | --- | ---
name | const char* | | 方法名称

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
## Definitions

### <a id="CallCallback"></a>CallCallback(int32_t, response)

回调函数：收到回复的消息

#### Parameters

name | type | description
--- | --- | ---
rescode | int32_t | 远程方法调用错误码
response | [Response](#Response)& | 远程调用返回结果

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
ret_code | int32_t | 返回码，远程方法返回，0为成功。
data | shared_ptr\<[Caps](https://github.com/Rokid/aife-mutils/blob/master/caps.md)>& | 消息内容
sender | string | 远程方法声明方的身份标识

---

# <a id="ClientCallback"></a>ClientCallback

## Methods

### recv_post(name, type, msg)

回调函数：收到订阅的消息

#### Parameters

name | type | description
--- | --- | ---
name | string | 消息名称
type | uint32_t | 消息类型<br>FLORA_MSGTYPE_INSTANT<br>FLORA_MSGTYPE_PERSIST
msg | shared_ptr\<[Caps](https://github.com/Rokid/aife-mutils/blob/master/caps.md)>& | 消息内容

### recv_call(name, msg, reply)

回调函数：收到远程方法调用

#### Parameters

name | type | description
--- | --- | ---
name | string | 消息名称
msg | shared_ptr\<[Caps](https://github.com/Rokid/aife-mutils/blob/master/caps.md)>& | 消息内容
reply | [Reply](#Reply)& | 填充reply结构体，给远程方法调用者返回数据

### disconnected

回调函数：连接断开
