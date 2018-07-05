#pragma once

#include "caps.h"

#define FLORA_CLI_SUCCESS 0
// 认证过程失败(版本号不支持)
#define FLORA_CLI_EAUTH -1
// 参数不合法
#define FLORA_CLI_EINVAL -2
// 连接错误
#define FLORA_CLI_ECONN -3

typedef intptr_t flora_cli_t;
typedef void (*flora_cli_disconnected_func_t)(void* arg);
typedef void (*flora_cli_received_func_t)(const char* name, caps_t msg, void* arg);
typedef struct {
	flora_cli_received_func_t received;
	flora_cli_disconnected_func_t disconnected;
} flora_cli_callback_t;

// uri: 支持的schema:
//     tcp://$(host):$(port)/
//     unix:$(domain_name)         (unix domain socket)
// async: 异步模式开关
// result: 同步模式时返回flora_cli_t对象
//         异步模式时无用, 可传NULL
int32_t flora_cli_connect(const char* uri, /*int32_t async,*/
		flora_cli_callback_t* callback, void* arg,
		uint32_t msg_buf_size, flora_cli_t* result);

void flora_cli_delete(flora_cli_t handle);

int32_t flora_cli_subscribe(flora_cli_t handle, const char* name);

int32_t flora_cli_unsubscribe(flora_cli_t handle, const char* name);

int32_t flora_cli_post(flora_cli_t handle, const char* name, caps_t msg);
