#pragma once

#include "flora-svc.h"
#include "flora-agent.h"

#define TAG "flora.demo"
#define AGENT_NUMBER 4

class DemoAllInOne {
public:
  void run();

private:
  void start_service();

  void stop_service();

  void run_start_stop_service();

  void run_post_msg();

  void run_recv_msg();

private:
  std::shared_ptr<flora::Poll> poll;
  std::shared_ptr<flora::Dispatcher> dispatcher;
  flora::Agent agents[AGENT_NUMBER];
  flora_agent_t cagents[AGENT_NUMBER];
};
