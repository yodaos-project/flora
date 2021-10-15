#include <unistd.h>
#include "gtest/gtest.h"
#include "tests-common.h"
#include "flora-svc.h"
#include "xmopt.h"

using namespace std;

int main(int argc, char** argv) {
  testing::InitGoogleTest(&argc, argv);
  XMOptions optp;
  optp.option(nullptr, "flora-uri", "specify flora service uri");
  shared_ptr<flora::Service> service;
  if (!optp.parse(argc - 1, argv + 1)) {
    string msg;
    optp.errorMsg(msg);
    printf("%s\n", msg.c_str());
    return 1;
  } else {
    auto r = optp.find("flora-uri");
    XMOptions::Option* opt;
    opt = r.next();
    if (opt == nullptr) {
      printf("must specify flora service uri\n");
      return 1;
    }
    floraUri = opt->value();
    flora::Service::Builder builder;
    service = builder.addUri(floraUri).build();
    service->start();
  }
  auto r = RUN_ALL_TESTS();
  if (service) {
    service->stop();
    sleep(2);
  }
  return r;
}
