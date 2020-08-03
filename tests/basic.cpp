#include <stdio.h>
#include <unistd.h>
#include <vector>
#include "gtest/gtest.h"
#include "tests-common.h"
#include "flora-cli.h"
#include "caps.h"

using namespace std;
using namespace rokid;
using namespace flora;

TEST(BasicTest, subscribe) {
  struct {
    uint32_t recvFoo{0};
    uint32_t updateBar{0};
    uint32_t removeBar{0};
  } testVars;
  auto cli = Client::newInstance();
  cli->config(ClientOptions::URI, floraUri.c_str());
  cli->config(ClientOptions::SYNC, 1);
  cli->subscribe("foo", [&testVars](const Caps& data) {
    ++testVars.recvFoo;
    EXPECT_EQ(data.size(), 2);
    auto it = data.iterate();
    try {
      int32_t v;
      it >> v;
      EXPECT_EQ(v, 0);
      it >> v;
      EXPECT_EQ(v, 1);
    } catch (exception& e) {
      FAIL();
    }
  });
  cli->subscribe("bar", [&testVars](const Caps* data, bool remove) {
    if (remove) {
      ++testVars.removeBar;
    } else {
      ++testVars.updateBar;
      ASSERT_TRUE(data != nullptr);
      ASSERT_TRUE(data->empty());
    }
  });
  thrPool.push([&cli]() {
    sleep(2);
    cli->close();
  });
  cli->open([&cli](int32_t status) {
    if (status == 1) {
      Caps data;
      data << 0;
      data << 1;
      cli->publish("foo", &data);
      cli->updatePersist("bar", nullptr);
      cli->deletePersist("bar");
    }
  });
  thrPool.finish();
  EXPECT_EQ(testVars.recvFoo, 1);
  EXPECT_EQ(testVars.updateBar, 1);
  EXPECT_EQ(testVars.removeBar, 1);
}

TEST(BasicTest, remoteMethod) {
  struct {
    uint32_t callFoo{0};
    uint32_t callBar{0};
    uint32_t retBar{0};
    uint32_t conn{0};
  } testVars;
  auto callee = Client::newInstance();
  callee->config(ClientOptions::URI, floraUri.c_str());
  callee->config(ClientOptions::ID, "callee");
  callee->config(ClientOptions::SYNC, 1);
  callee->declareMethod("foo",
      [&testVars](const Caps& data, shared_ptr<Reply>& reply) {
    ++testVars.callFoo;
    EXPECT_EQ(data.size(), 1);
    auto it = data.iterate();
    try {
      int32_t v;
      it >> v;
      Caps ret;
      ret << v + 100;
      reply->end(&ret);
    } catch (exception& e) {
      FAIL();
    }
  });
  callee->declareMethod("bar",
      [&testVars](const Caps& data, shared_ptr<Reply>& reply) {
    ++testVars.callBar;
    EXPECT_EQ(data.size(), 1);
    auto it = data.iterate();
    try {
      int32_t v;
      it >> v;
      Caps ret;
      ret << v * 100;
      reply->end(&ret);
    } catch (exception& e) {
      FAIL();
    }
  });

  auto caller = Client::newInstance();
  caller->config(ClientOptions::URI, floraUri.c_str());
  caller->config(ClientOptions::SYNC, 1);
  thrPool.push([&callee, &testVars]() {
    callee->open([&callee, &testVars](int32_t status) {
      if (status == 1) {
        testVars.conn |= 1;
      } else {
        testVars.conn &= (~1);
      }
    });
  });
  thrPool.push([&caller, &testVars]() {
    caller->open([&caller, &testVars](int32_t status) {
      if (status == 1) {
        testVars.conn |= 2;
      } else {
        testVars.conn &= (~2);
      }
    });
  });
  thrPool.push([&caller, &testVars]() {
    uint32_t retry = 10;
    while (retry) {
      if (testVars.conn == 3)
        break;
      usleep(10000);
      --retry;
    }
    ASSERT_FALSE(retry == 0);
    Caps args;
    args << 1;
    Caps ret;
    auto r = caller->call("foo", "callee", &args, 200, &ret);
    EXPECT_TRUE(r);
    if (r) {
      EXPECT_EQ(ret.size(), 1);
      auto it = ret.iterate();
      try {
        int32_t v;
        it >> v;
        EXPECT_EQ(v, 101);
      } catch (exception& e) {
        FAIL();
      }
    }
    args.clear();
    args << 30;
    r = caller->callAsync("bar", "callee", &args, 200,
        [&testVars](int32_t code, const Caps& ret) {
          ++testVars.retBar;
          EXPECT_TRUE(code == FLORA_CLI_SUCCESS);
          if (code == FLORA_CLI_SUCCESS) {
            EXPECT_EQ(ret.size(), 1);
            auto it = ret.iterate();
            try {
              int32_t v;
              it >> v;
              EXPECT_EQ(v, 3000);
            } catch (exception& e) {
              FAIL();
            }
          }
        });
    EXPECT_TRUE(r);
  });
  sleep(4);
  callee->close();
  caller->close();
  sleep(1);
  thrPool.finish();
  EXPECT_EQ(testVars.callFoo, 1);
  EXPECT_EQ(testVars.callBar, 1);
  EXPECT_EQ(testVars.retBar, 1);
  EXPECT_EQ(testVars.conn, 0);
}

TEST(BasicTest, subscribeStatus) {
  struct {
    uint32_t updateFoo{0};
    uint32_t removeFoo{0};
    uint32_t updateBar{0};
    uint32_t removeBar{0};
    bool holderConnected{false};
    bool observerConnected{false};
    vector<Caps> fooStatus;
    vector<Caps> barStatus;
  } testVars;
  auto holder = Client::newInstance();
  auto observer = Client::newInstance();
  holder->config(ClientOptions::URI, floraUri.c_str());
  holder->config(ClientOptions::ID, "holder");
  holder->config(ClientOptions::SYNC, 1);
  observer->config(ClientOptions::URI, floraUri.c_str());
  observer->config(ClientOptions::SYNC, 1);
  observer->subscribe("foo", "holder", [&testVars](const Caps* args, bool remove) {
    if (remove)
      ++testVars.removeFoo;
    else {
      ++testVars.updateFoo;
      testVars.fooStatus.push_back(*args);
    }
  });
  observer->subscribe("bar", "holder", [&testVars](const Caps* args, bool remove) {
    if (remove)
      ++testVars.removeBar;
    else {
      ++testVars.updateBar;
      testVars.barStatus.push_back(*args);
    }
  });
  thrPool.push([&holder, &testVars]() {
    holder->open([&testVars](int32_t status) {
      testVars.holderConnected = (status == 1);
    });
  });
  thrPool.push([&observer, &testVars]() {
    observer->open([&testVars](int32_t status) {
      testVars.observerConnected = (status == 1);
    });
  });
  uint32_t waitConnected{10};
  while (waitConnected) {
    usleep(10000);
    if (testVars.holderConnected && testVars.observerConnected)
      break;
    --waitConnected;
  }
  ASSERT_TRUE(testVars.holderConnected);
  ASSERT_TRUE(testVars.observerConnected);
  Caps args;
  args << 77;
  holder->updateStatus("foo", &args);
  holder->updateStatus("foo", nullptr);
  holder->updateStatus("bar", &args);
  args.clear();
  holder->updateStatus("bar", &args);
  args << 66;
  holder->updateStatus("bar", &args);
  holder->deleteStatus("foo");
  sleep(1);
  EXPECT_EQ(testVars.updateFoo, 2);
  EXPECT_EQ(testVars.removeFoo, 1);
  EXPECT_EQ(testVars.updateBar, 3);
  EXPECT_EQ(testVars.removeBar, 0);
  if (testVars.fooStatus.size() == 2) {
    EXPECT_EQ((int32_t)testVars.fooStatus[0][0], 77);
    EXPECT_TRUE(testVars.fooStatus[1].empty());
  }
  if (testVars.barStatus.size() == 3) {
    EXPECT_EQ((int32_t)testVars.barStatus[0][0], 77);
    EXPECT_TRUE(testVars.barStatus[1].empty());
    EXPECT_EQ((int32_t)testVars.barStatus[2][0], 66);
  }
  holder->close();
  sleep(1);
  EXPECT_EQ(testVars.removeBar, 1);
  observer->close();
  sleep(1);
  EXPECT_FALSE(testVars.holderConnected);
  EXPECT_FALSE(testVars.observerConnected);
}
