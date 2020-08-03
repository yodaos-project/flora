#include <signal.h>
#include "svc.h"

namespace flora {

shared_ptr<Service> Service::newInstance() {
  signal(SIGPIPE, SIG_IGN);
  return make_shared<ServiceImpl>();
}

} // namespace flora

ServiceAdapter::~ServiceAdapter() {
  writeThread->unbindAdapter();
}
