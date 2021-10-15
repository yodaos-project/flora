#include "gtest/gtest.h"
#include "tests-common.h"
#include "flora-cli.h"
#include "caps.h"
#include "global-error.h"

using namespace std;
using namespace rokid;
using namespace flora;

TEST(FailedTest, callTimeout) {
  struct {
    uint32_t callFoo{0};
    uint32_t callBar{0};
    uint32_t retFoo{0};
    uint32_t retBar{0};
    uint32_t conn{0};
  } testVars;

  Client::Builder builder;
  auto callee = builder.setUri(floraUri)
    .setId("callee")
    .build();
  callee->declareMethod("foo",
      [&testVars](const Caps& data, shared_ptr<Reply>& reply) {
        ++testVars.callFoo;
      });
  callee->declareMethod("bar",
      [&testVars](const Caps& data, shared_ptr<Reply>& reply) {
        ++testVars.callBar;
      });
  auto caller = builder.setId("").build();
  thrPool.push([&callee, &testVars]() {
    callee->open([&callee, &testVars](bool conn) {
      if (conn) {
        testVars.conn |= 1;
      } else {
        testVars.conn &= (~1);
      }
    });
  });
  thrPool.push([&caller, &testVars]() {
    caller->open([&caller, &testVars](bool conn) {
      if (conn) {
        testVars.conn |= 2;
      } else {
        testVars.conn &= (~2);
      }
    });
  });

  uint32_t retry = 10;
  while (retry) {
    if (testVars.conn == 3)
      break;
    usleep(10000);
    --retry;
  }
  ASSERT_FALSE(retry == 0);
  EXPECT_FALSE(caller->call("foo", "callee", nullptr, 100, nullptr));
  EXPECT_EQ(rokid::GlobalError::code(), FLORA_CLI_ETIMEOUT);
  EXPECT_FALSE(caller->call("foo", "callee", nullptr, 100, nullptr));
  EXPECT_EQ(rokid::GlobalError::code(), FLORA_CLI_ETIMEOUT);
  EXPECT_FALSE(caller->call("bar", "callee", nullptr, 100, nullptr));
  EXPECT_EQ(rokid::GlobalError::code(), FLORA_CLI_ETIMEOUT);
  caller->callAsync("foo", "callee", nullptr, 100, [&testVars](int32_t code, const Caps& ret) {
    ++testVars.retFoo;
    EXPECT_EQ(code, FLORA_CLI_ETIMEOUT);
    EXPECT_TRUE(ret.empty());
  });
  caller->callAsync("bar", "callee", nullptr, 100, [&testVars](int32_t code, const Caps& ret) {
    ++testVars.retBar;
    EXPECT_EQ(code, FLORA_CLI_ETIMEOUT);
    EXPECT_TRUE(ret.empty());
  });
  sleep(2);
  caller->close();
  callee->close();
  sleep(1);
  EXPECT_EQ(testVars.callFoo, 3);
  EXPECT_EQ(testVars.callBar, 2);
  EXPECT_EQ(testVars.retFoo, 1);
  EXPECT_EQ(testVars.retBar, 1);
  EXPECT_EQ(testVars.conn, 0);
}
