#include <signal.h>
#include "svc.h"

namespace flora {

shared_ptr<Service> Service::Builder::build() {
  signal(SIGPIPE, SIG_IGN);
  return make_shared<ServiceImpl>(uris, bufsize, readThreadNum, writeThreadNum);
}

} // namespace flora

ServiceAdapter::~ServiceAdapter() {
  writeThread->unbindAdapter();
}
