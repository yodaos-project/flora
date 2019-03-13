#include "beep-sock-poll.h"
#include "flora-svc.h"
#include "uri.h"

using namespace std;
using namespace rokid;

shared_ptr<flora::Poll> flora::Poll::new_instance(const char *uri) {
  Uri urip;
  if (!urip.parse(uri))
    return nullptr;
  if (urip.scheme == "unix") {
    return static_pointer_cast<flora::Poll>(
        make_shared<flora::internal::SocketPoll>(urip.path));
  } else if (urip.scheme == "tcp") {
    return static_pointer_cast<flora::Poll>(
        make_shared<flora::internal::BeepSocketPoll>(urip.host, urip.port));
  }
  return nullptr;
}

int32_t flora_poll_new(const char *uri, flora_poll_t *result) {
  if (result == nullptr)
    return FLORA_POLL_INVAL;
  Uri urip;
  if (!urip.parse(uri))
    return FLORA_POLL_INVAL;
  if (urip.scheme == "unix") {
    *result = reinterpret_cast<flora_poll_t>(
        new flora::internal::SocketPoll(urip.path));
    return FLORA_POLL_SUCCESS;
  } else if (urip.scheme == "tcp") {
    *result = reinterpret_cast<flora_poll_t>(
        new flora::internal::SocketPoll(urip.host, urip.port));
    return FLORA_POLL_SUCCESS;
  }
  return FLORA_POLL_UNSUPP;
}

void flora_poll_delete(flora_poll_t handle) {
  if (handle)
    delete reinterpret_cast<flora::Poll *>(handle);
}

int32_t flora_poll_start(flora_poll_t handle, flora_dispatcher_t dispatcher) {
  if (handle == 0 || dispatcher == 0)
    return FLORA_POLL_INVAL;
  flora::Poll *fpoll = reinterpret_cast<flora::Poll *>(handle);
  flora::Dispatcher *disp = reinterpret_cast<flora::Dispatcher *>(dispatcher);
  shared_ptr<flora::Dispatcher> pdisp(disp, [](flora::Dispatcher *) {});
  return fpoll->start(pdisp);
}

void flora_poll_stop(flora_poll_t handle) {
  if (handle)
    reinterpret_cast<flora::Poll *>(handle)->stop();
}
