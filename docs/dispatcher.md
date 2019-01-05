# Dispatcher

flora service消息分发模块，需与[Poll](poll.md)配合使用

## Methods

### <font color=#bdbdbd>(static)</font> new_instance(bufsize)

创建Dispatcher对象

#### Parameters

name | type | default | description
--- | --- | --- | ---
bufsize | uint32_t | 0 | 消息缓冲区大小（决定了一个消息最大大小），最小值32K，小于32K的值会被改为32K。