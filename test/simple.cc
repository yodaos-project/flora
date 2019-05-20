#include <unistd.h>
#include "gtest/gtest.h"
#include "flora-agent.h"
#include "svc.h"

using namespace std;
using namespace flora;

TEST(SimpleTest, postInstant) {
  Agent rcv;
  Agent snd1;
  Agent snd2;
  string uri = Service::floraUri;
  rcv.config(FLORA_AGENT_CONFIG_URI, uri.c_str());
  snd1.config(FLORA_AGENT_CONFIG_URI, (uri + "#test1").c_str());
  snd2.config(FLORA_AGENT_CONFIG_URI, (uri + "#test2").c_str());
  string msgName = "foo";
  struct {
    uint32_t num[2]{0, 0};
    string tags[2];
    uint32_t postCompleted{0};

    int32_t tag2Idx(const string& tag) {
      int32_t i;
      for (i = 0; i < 2; ++i) {
        if (tags[i].empty()) {
          tags[i] = tag;
          return i;
        }
        if (tags[i] == tag)
          return i;
      }
      return -1;
    }
  } inst;
  rcv.subscribe(msgName.c_str(),
      [&inst](const char* name, shared_ptr<Caps>& msg, uint32_t type) {
        string tag;
        MsgSender::to_string(tag);
        auto idx = inst.tag2Idx(tag);
        ASSERT_GE(idx, 0);
        ASSERT_LT(idx, 2);
        int32_t v{-1};
        EXPECT_EQ(msg->read(v), CAPS_SUCCESS);
        EXPECT_EQ(v, inst.num[idx]);
        ++inst.num[idx];
      });
  rcv.start();
  snd1.start();
  snd2.start();

  Service::thrPool.push([&snd1, &inst, &msgName]() {
    uint32_t i;
    for (i = 0; i < 78; ++i) {
      auto msg = Caps::new_instance();
      msg->write(i);
      snd1.post(msgName.c_str(), msg);
    }
    ++inst.postCompleted;
  });

  Service::thrPool.push([&snd2, &inst, &msgName]() {
    uint32_t i;
    for (i = 0; i < 99; ++i) {
      auto msg = Caps::new_instance();
      msg->write(i);
      snd2.post(msgName.c_str(), msg);
    }
    ++inst.postCompleted;
  });

  sleep(5);
  EXPECT_EQ(inst.postCompleted, 2);
  EXPECT_EQ(inst.num[0] + inst.num[1], 78 + 99);
  EXPECT_EQ(inst.num[0] == 78 || inst.num[0] == 99, true);
}
