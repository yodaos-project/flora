#pragma once

#define FLORA_SVC_SUCCESS 0
/// 非法的参数/配置
#define FLORA_SVC_EINVAL -1
/// 系统调用失败
#define FLORA_SVC_ESYS -2
/// client name 已被使用
/// method name 已存在
#define FLORA_SVC_EDUP -3
/// 操作不允许
#define FLORA_SVC_EPERMIT -4
/// 对象方法不存在
#define FLORA_SVC_ENEXISTS -5
/// 读/写buffer不足
#define FLORA_SVC_ENOBUF -6

#define FLORA_CLI_SUCCESS FLORA_SVC_SUCCESS
#define FLORA_CLI_EINVAL FLORA_SVC_EINVAL
#define FLORA_CLI_ESYS FLORA_SVC_ESYS
#define FLORA_CLI_EDUP FLORA_SVC_EDUP
#define FLORA_CLI_EPERMIT FLORA_SVC_EPERMIT
#define FLORA_CLI_ETIMEOUT -7
/// flora服务未连接，可稍候重试
#define FLORA_CLI_ENOT_READY -8
/// client端未声明该method
#define FLORA_CLI_ENOTFOUND -9
