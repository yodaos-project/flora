# flora2

## 进程间消息广播协议实现

### 功能

* (INSTANT)即时消息广播/订阅

* (PERSIST)持久消息广播/订阅/删除

* (STATUS)状态消息广播/订阅/删除

* (METHOD)远程方法同步调用/异步调用

```
状态消息类似于持久消息，状态消息有命名空间，持久消息是全局唯一，状态消息是每个客户端唯一。
客户端如果断开连接，该客户端定义的状态消息自动删除并通知所有订阅者。

目前仅支持linux，未支持macos与windows。
```

### 依赖库

* https://github.com/rokid/cmake-modules -- cmake脚本

* https://github.com/mingzc0508/mutils -- log工具, 线程池等 (分支dev/1.1.x)

* https://github.com/mingzc0508/caps -- 序列化工具caps 2.0, 比1.0更紧凑的数据结构, 更易用的接口。 (分支dev/2.0.x)
