# Reply

## Methods

### write_code

设置远程函数返回码

#### Parameters

name | type | default | description
--- | --- | --- | ---
code | int32_t | 0 |

### write_data

设置远程函数返回值

#### Parameters

name | type | default | description
--- | --- | --- | ---
data | shared_ptr\<[Caps](https://github.com/Rokid/aife-mutils/blob/master/caps.md)>& | | 返回值

### end

销毁Reply对象并将返回码与返回值发送至flora服务，flora服务将发送给远程函数调用者

#### No Parameter
