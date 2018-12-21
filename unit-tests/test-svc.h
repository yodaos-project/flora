#pragma once

#include "flora-svc.h"

using namespace std;
using namespace flora;

class TestService {
public:
  bool run(const char *uri, bool capi);

  void close();

private:
  shared_ptr<Dispatcher> dispatcher;
  shared_ptr<Poll> fpoll;

  flora_dispatcher_t c_dispatcher = 0;
  flora_poll_t c_fpoll = 0;

  bool use_c_api = false;
};
