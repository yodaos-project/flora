#pragma once

#define FLORA_VERSION 2

// client --> server
#define CMD_AUTH_REQ 0
#define CMD_SUBSCRIBE_REQ 1
#define CMD_UNSUBSCRIBE_REQ 2
#define CMD_POST_REQ 3
#define CMD_REPLY_REQ 4
#define CMD_DECLARE_METHOD_REQ 5
#define CMD_REMOVE_METHOD_REQ 6
#define CMD_CALL_REQ 7
// server --> client
#define CMD_AUTH_RESP 101
#define CMD_POST_RESP 102
#define CMD_REPLY_RESP 103
#define CMD_CALL_RESP 104

#define MSG_HANDLER_COUNT 8

#define DEFAULT_MSG_BUF_SIZE 32768

#ifdef __APPLE__
#define SELECT_BLOCK_IF_FD_CLOSED
#endif
