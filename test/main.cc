#include "gtest/gtest.h"
#include "svc.h"

using namespace std;
using namespace flora;

string Service::floraUri;
ThreadPool Service::thrPool{4};

int main(int argc, char** argv) {
  if (argc < 2) {
    printf("USAGE: %s ${floraUri}\n", argv[0]);
    return 1;
  }
  Service svc{argv[1]};
  svc.start();

  --argc;
  ++argv;
  testing::InitGoogleTest(&argc, argv);
  auto r = RUN_ALL_TESTS();
  svc.stop();
  Service::thrPool.clear();
  return r;
}
