#pragma once

#include <string>
#include "thr-pool.h"
#include "flora-svc.h"

class Service {
public:
  Service(const char* uri) {
    floraUri = uri;
  }

  void start() {
    disp = flora::Dispatcher::new_instance(FLORA_DISP_FLAG_MONITOR);
    poll = flora::Poll::new_instance(floraUri.c_str());
    poll->start(disp);
    disp->run(false);
  }

  void stop() {
    disp->close();
    poll->stop();
    disp.reset();
    poll.reset();
  }

public:
  static std::string floraUri;
  static ThreadPool thrPool;

private:
  std::shared_ptr<flora::Dispatcher> disp;
  std::shared_ptr<flora::Poll> poll;
};

