#pragma once

#define FLORA_VERSION 1

// client --> server
#define CMD_AUTH_REQ 0
#define CMD_SUBSCRIBE_REQ 1
#define CMD_UNSUBSCRIBE_REQ 2
#define CMD_POST_REQ 3
#define CMD_REPLY_REQ 4
// server --> client
#define CMD_AUTH_RESP 101
#define CMD_POST_RESP 102
#define CMD_REPLY_RESP 103

#define MSG_HANDLER_COUNT 5

#define DEFAULT_MSG_BUF_SIZE 4096
