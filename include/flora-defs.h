#pragma once

/// \file flora-defs.h
/// flora错误码定义
/// \author chen.zhang@rokid.com
/// \version 2.0.0
/// \date 2020-12-29

#define FLORA_SVC_SUCCESS 0
/// 非法的参数/配置
#define FLORA_SVC_EINVAL -1
/// 系统调用失败
#define FLORA_SVC_ESYS -2
/// 客户端id已被使用 或 远程方法名已存在
#define FLORA_SVC_EDUP -3
/// 操作不允许
#define FLORA_SVC_EPERMIT -4
/// 对象方法不存在
#define FLORA_SVC_ENEXISTS -5
/// 读/写buffer不足
#define FLORA_SVC_ENOBUF -6

#define FLORA_CLI_SUCCESS FLORA_SVC_SUCCESS
/// 非法参数
#define FLORA_CLI_EINVAL FLORA_SVC_EINVAL
/// 系统调用失败
#define FLORA_CLI_ESYS FLORA_SVC_ESYS
/// 客户端id重复
#define FLORA_CLI_EDUP FLORA_SVC_EDUP
/// 操作不允许
#define FLORA_CLI_EPERMIT FLORA_SVC_EPERMIT
/// 远程方法调用超时
#define FLORA_CLI_ETIMEOUT -7
/// flora服务未连接，可稍候重试
#define FLORA_CLI_ENOT_READY -8
/// 未找到指定远程方法
#define FLORA_CLI_ENOTFOUND -9
