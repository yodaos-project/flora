# Poll

flora service连接管理，需与[Dispatcher](dispatcher.md)配合使用

```
shared_ptr<Dispatcher> disp = Dispatcher::new_instance(0);
shared_ptr<Poll> unix_poll = Poll::new_instance("unix:/var/run/flora.sock");
unix_poll->start(disp);
shared_ptr<Poll> tcp_poll = Poll::new_instance("tcp://0.0.0.0:38380/");
tcp_poll->start(disp);
```

## Methods

### <font color=#bdbdbd>(static)</font> new_instance(uri)

创建Poll对象

#### Parameters

name | type | default | description
--- | --- | --- | ---
uri | const char* | | uri - 服务侦听地址<br>支持unix domain socket及tcp socket

### start(dispatcher)

启动服务地址侦听

#### Parameters

name | type | default | description
--- | --- | --- | ---
dispatcher | shared_ptr\<[Dispatcher](dispatcher.md)>& | |

#### Returns

Type: int32_t

value | description
--- | ---
FLORA_POLL_SUCCESS | 成功
FLORA_POLL_ALREADY_START | 重复start
FLORA_POLL_SYSERR | socket等系统调用失败
FLORA_POLL_INVAL | 参数非法

---

### stop()

停止服务地址侦听
