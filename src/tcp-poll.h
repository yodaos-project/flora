#pragma once

#include <map>
#include <memory>
#include <string>
#include <thread>
#include <mutex>
#include "poll.h"
#include "tcp-adap.h"

typedef std::map<int, std::shared_ptr<TCPAdapter> > AdapterMap;
// typedef std::list<std::shared_ptr<Session> > SessionList;
// typedef std::map<std::string, SessionList> SubscribeMap;

class TCPPoll : public Poll {
public:
	TCPPoll(const std::string& host, int32_t port);

	int32_t start(Dispatcher* disp);

	void close();

private:
	void run();

	bool init_socket();

	void new_adapter(int fd);

	void delete_adapter(int fd);

	bool read_from_client(std::shared_ptr<TCPAdapter>& adap);

private:
	Dispatcher* dispatcher = nullptr;
	int listen_fd = -1;
	uint32_t max_msg_size = 0;
	std::thread run_thread;
	std::mutex start_mutex;
	AdapterMap adapters;
	std::string host;
	int32_t port;
};
