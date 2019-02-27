#include "test-svc.h"

bool TestService::run(const char *uri, bool capi) {
  use_c_api = capi;
  if (capi) {
    c_dispatcher = flora_dispatcher_new(0, 0);
    if (c_dispatcher == 0)
      return false;
    if (flora_poll_new(uri, &c_fpoll) != FLORA_POLL_SUCCESS) {
      flora_dispatcher_delete(c_dispatcher);
      c_dispatcher = 0;
      return false;
    }
    if (flora_poll_start(c_fpoll, c_dispatcher) != FLORA_POLL_SUCCESS) {
      flora_poll_delete(c_fpoll);
      c_fpoll = 0;
      flora_dispatcher_delete(c_dispatcher);
      c_dispatcher = 0;
      return false;
    }
    flora_dispatcher_run(c_dispatcher, 0);
  } else {
    dispatcher = Dispatcher::new_instance(0, 0);
    fpoll = Poll::new_instance(uri);
    if (fpoll.get() == nullptr)
      return false;
    if (fpoll->start(dispatcher) != FLORA_POLL_SUCCESS)
      return false;
    dispatcher->run(false);
  }
  return true;
}

void TestService::close() {
  if (use_c_api) {
    flora_poll_stop(c_fpoll);
    flora_poll_delete(c_fpoll);
    c_fpoll = 0;
    flora_dispatcher_delete(c_dispatcher);
    c_dispatcher = 0;
  } else {
    fpoll->stop();
    fpoll.reset();
    dispatcher.reset();
  }
}
