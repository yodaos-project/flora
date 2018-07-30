#pragma once

#include <map>
#include <memory>
#include <string>
#include <thread>
#include <mutex>
#include "flora-svc.h"
#include "tcp-adap.h"
#include "disp.h"

typedef std::map<int, std::shared_ptr<TCPAdapter> > AdapterMap;
// typedef std::list<std::shared_ptr<Session> > SessionList;
// typedef std::map<std::string, SessionList> SubscribeMap;

namespace flora {
namespace internal {

class TCPPoll : public flora::Poll {
public:
	TCPPoll(const std::string& host, int32_t port);

	int32_t start(std::shared_ptr<flora::Dispatcher>& disp);

	void stop();

private:
	void run();

	bool init_socket();

	void new_adapter(int fd);

	void delete_adapter(int fd);

	bool read_from_client(std::shared_ptr<TCPAdapter>& adap);

private:
	std::shared_ptr<Dispatcher> dispatcher;
	int listen_fd = -1;
	uint32_t max_msg_size = 0;
	std::thread run_thread;
	std::mutex start_mutex;
	AdapterMap adapters;
	std::string host;
	int32_t port;
};

} // namespace internal
} // namespace flora
