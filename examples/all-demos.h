#pragma once

#include "flora-agent.h"
#include "flora-svc.h"

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

  void test_errors();

  void test_reply_after_close();

  void test_invoke_in_callback();

private:
  std::shared_ptr<flora::Poll> poll;
  std::shared_ptr<flora::Dispatcher> dispatcher;
  flora::Agent agents[AGENT_NUMBER];
  flora_agent_t cagents[AGENT_NUMBER];
};
