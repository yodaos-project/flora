#include <stdlib.h>
#include <chrono>
#include "gtest/gtest.h"
#include "tests-common.h"
#include "flora-cli.h"
#include "caps.h"
#include "global-error.h"

#define NETWORK_CONNECTED 1
#define VIDEO_ACTIVE 2
#define ALARM_RUNNING 4
#define MUSIC_PLAYING 8
#define MAX_RANDOM_OPERATIONS 32

using namespace std;
using namespace std::chrono;
using namespace rokid;
using namespace flora;

class User {
public:
  User() {
    operations[0] = [this]() {
      setupNetwork();
    };
    operations[1] = [this]() {
      closeNetwork();
    };
    operations[2] = [this]() {
      playMusic();
    };
    operations[3] = [this]() {
      pauseMusic();
    };
    operations[4] = [this]() {
      stopMusic();
    };
    operations[5] = [this]() {
      setupAlarm();
    };
    operations[6] = [this]() {
      callVideo();
    };
    operations[7] = [this]() {
      cancelVideo();
    };
    operations[8] = [this]() {
      resumeMusic();
    };
    operations[9] = operations[3];
    operations[10] = operations[6];
    operations[11] = operations[5];
    operations[12] = operations[3];
    operations[13] = operations[5];
    operations[14] = operations[6];
    operations[15] = operations[7];
    operations[16] = operations[7];
    operations[17] = operations[6];
    operations[18] = operations[5];
    operations[19] = operations[1];
    operations[20] = operations[0];
    operations[21] = operations[0];
    operations[22] = operations[0];
    operations[23] = operations[8];
    operations[24] = operations[2];
    operations[25] = operations[2];
    operations[26] = operations[2];
    operations[27] = operations[4];
    operations[28] = operations[8];
    operations[29] = operations[8];
    operations[30] = operations[0];
    operations[31] = operations[4];
  }

  ~User() {
    if (cli != nullptr)
      cli->close();
  }

  void start() {
    hasExecuted = true;
    Client::Builder builder;
    cli = builder.setUri(floraUri)
      .setId("user")
      .build();
    cli->subscribe("status", "network", [this](const Caps* data, bool remove) {
      modifyFlags(data, remove, NETWORK_CONNECTED);
    });
    cli->subscribe("status", "video", [this](const Caps* data, bool remove) {
      modifyFlags(data, remove, VIDEO_ACTIVE);
    });
    cli->subscribe("status", "music", [this](const Caps* data, bool remove) {
      modifyFlags(data, remove, MUSIC_PLAYING);
    });
    cli->subscribe("sound-alarm", [this](const Caps& data) {
      alarmEvent = "sound-alarm";
    });
    cli->subscribe("floatwindow-alarm", [this](const Caps& data) {
      alarmEvent = "floatwindow-alarm";
    });
    cli->open([this](int32_t conn) {
      if (conn == 1) {
        thrPool.push([this]() {
          run();
        });
      }
    });
  }

  void waitFinish() {
    unique_lock<mutex> locker(userMutex);
    if (hasExecuted)
      userTiered.wait(locker);
  }

private:
  void run() {
    uint32_t repeat = 100;
    uint32_t i;
    int32_t idx;
    for (i = 0; i < repeat; ++i) {
      sleep(1);
      idx = rand() % MAX_RANDOM_OPERATIONS;
      operations[idx]();
    }
    lock_guard<mutex> locker(userMutex);
    hasExecuted = false;
    userTiered.notify_one();
  }

  void setupNetwork() {
    Caps args;
    args << "connect";
    Caps ret;
    auto r = cli->call("config", "network", &args, 2000, &ret);
    EXPECT_TRUE(r);
    if (r) {
      EXPECT_EQ(ret.size(), 1);
      EXPECT_EQ((int32_t)ret[0], 0);
    } else {
      KLOGW(TAG, "call failed: %s", ROKID_GERROR_STRING);
    }
  }

  void closeNetwork() {
    Caps args;
    args << "disconnect";
    auto r = cli->callAsync("config", "network", &args, 200, [](int32_t code, const Caps& data) {
      EXPECT_EQ(code, FLORA_CLI_SUCCESS);
      if (code == FLORA_CLI_SUCCESS) {
        EXPECT_EQ(data.size(), 1);
        EXPECT_EQ((int32_t)data[0], 0);
      }
    });
    EXPECT_TRUE(r);
  }

  void playMusic() {
    Caps args;
    args << "play";
    Caps ret;
    auto r = cli->call("control", "music", &args, 500, &ret);
    EXPECT_TRUE(r);
    if (r) {
      EXPECT_EQ(ret.size(), 1);
      int32_t v = ret[0];
      if ((flags & NETWORK_CONNECTED) == 0)
        EXPECT_TRUE(v == -2 || v == 0);
      else if (flags & VIDEO_ACTIVE)
        EXPECT_TRUE(v == -3 || v == 0);
      else
        EXPECT_EQ(v, 0);
    } else {
      KLOGW(TAG, "call failed: %s", ROKID_GERROR_STRING);
    }
  }

  void pauseMusic() {
    Caps args;
    args << "pause";
    Caps ret;
    auto r = cli->call("control", "music", &args, 200, &ret);
    EXPECT_TRUE(r);
    if (r) {
      EXPECT_EQ(ret.size(), 1);
      EXPECT_EQ((int32_t)ret[0], 0);
    } else {
      KLOGW(TAG, "call failed: %s", ROKID_GERROR_STRING);
    }
  }

  void resumeMusic() {
    Caps args;
    args << "resume";
    Caps ret;
    auto r = cli->call("control", "music", &args, 500, &ret);
    EXPECT_TRUE(r);
    if (r) {
      EXPECT_EQ(ret.size(), 1);
      int32_t v = ret[0];
      if ((flags & NETWORK_CONNECTED) == 0)
        EXPECT_TRUE(v == -2 || v == 10);
      else if (flags & VIDEO_ACTIVE)
        EXPECT_TRUE(v == -3 || v == 10);
      else
        EXPECT_TRUE(v == 0 || v == 10);
    } else {
      KLOGW(TAG, "call failed: %s", ROKID_GERROR_STRING);
    }
  }

  void stopMusic() {
    Caps args;
    args << "stop";
    Caps ret;
    auto r = cli->call("control", "music", &args, 200, &ret);
    EXPECT_TRUE(r);
    if (r) {
      EXPECT_EQ(ret.size(), 1);
      EXPECT_EQ((int32_t)ret[0], 0);
    } else {
      KLOGW(TAG, "call failed: %s", ROKID_GERROR_STRING);
    }
  }

  void setupAlarm() {
    alarmEvent.clear();
    if (flags & VIDEO_ACTIVE)
      expectAlarmEvent = "floatwindow-alarm";
    else
      expectAlarmEvent = "sound-alarm";
    Caps args;
    args << "alarm";
    Caps ret;
    auto r = cli->call("launch", "launcher", &args, 200, &ret);
    EXPECT_TRUE(r);
    if (r) {
      EXPECT_EQ(ret.size(), 1);
      EXPECT_EQ((int32_t)ret[0], 0);
      thrPool.push([this]() {
        usleep(200000);
        EXPECT_EQ(expectAlarmEvent, alarmEvent);
      });
    } else {
      KLOGW(TAG, "call failed: %s", ROKID_GERROR_STRING);
    }
  }

  void callVideo() {
    Caps args;
    args << "video";
    Caps ret;
    auto r = cli->call("launch", "launcher", &args, 200, &ret);
    EXPECT_TRUE(r);
    if (r) {
      EXPECT_EQ(ret.size(), 1);
      EXPECT_EQ((int32_t)ret[0], 0);
      thrPool.push([this]() {
        usleep(200000);
        EXPECT_TRUE(flags & VIDEO_ACTIVE);
      });
    } else {
      KLOGW(TAG, "call failed: %s", ROKID_GERROR_STRING);
    }
  }

  void cancelVideo() {
    Caps ret;
    bool videoActive = (flags & VIDEO_ACTIVE);
    auto r = cli->call("stop", "video", nullptr, 200, &ret);
    if (videoActive) {
      EXPECT_TRUE(r);
      if (r) {
        EXPECT_EQ(ret.size(), 1);
        EXPECT_EQ((int32_t)ret[0], 0);
        thrPool.push([this]() {
          usleep(200000);
          EXPECT_FALSE(flags & VIDEO_ACTIVE);
        });
      } else {
        KLOGW(TAG, "call failed: %s", ROKID_GERROR_STRING);
      }
    } else {
      EXPECT_FALSE(r);
      EXPECT_EQ(GlobalError::code(), FLORA_SVC_ENEXISTS);
    }
  }

  void modifyFlags(const Caps* data, bool remove, int32_t flag) {
    if (remove || data == nullptr) {
      flags &= (~flag);
    } else {
      auto it = data->iterate();
      int32_t st;
      it >> st;
      if (st)
        flags |= flag;
      else
        flags &= (~flag);
    }
  }

public:
  bool hasExecuted = false;

private:
  shared_ptr<Client> cli;
  function<void()> operations[MAX_RANDOM_OPERATIONS];
  uint32_t flags = 0;
  string alarmEvent;
  string expectAlarmEvent;
  mutex userMutex;
  condition_variable userTiered;
};

class Network {
public:
  ~Network() {
    if (cli != nullptr)
      cli->close();
  }

  void start() {
    Client::Builder builder;
    cli = builder.setUri(floraUri)
      .setId("network")
      .build();
    cli->declareMethod("config", [this](const Caps& args, shared_ptr<Reply>& reply) {
      config(args, reply);
    });
    cli->declareMethod("getStatus", [this](const Caps& args, shared_ptr<Reply>& reply) {
      Caps data;
      data << netStatus;
      reply->end(&data);
    });
    cli->open(nullptr);
  }

private:
  void config(const Caps& args, shared_ptr<Reply>& reply) {
    auto it = args.iterate();
    string cmd;
    it >> cmd;
    if (cmd == "connect") {
      thrPool.push([this, reply]() {
        auto t = rand() % 1900;
        usleep(t * 1000);
        netStatus = 1;
        Caps data;
        data << 0;
        reply->end(&data);
        data.clear();
        data << 1;
        cli->updateStatus("status", &data);
      });
    } else if (cmd == "disconnect") {
      netStatus = 0;
      Caps data;
      data << 0;
      reply->end(&data);
      cli->updateStatus("status", &data);
    } else {
      Caps data;
      data << -1;
      reply->end(&data);
    }
  }

private:
  shared_ptr<Client> cli;
  int32_t netStatus = 0;
};

class Video {
public:
  ~Video() {
    if (cli != nullptr)
      cli->close();
  }

  void start() {
    if (videoStatus)
      return;
    videoStatus = 1;
    Client::Builder builder;
    cli = builder.setUri(floraUri)
      .setId("video")
      .build();
    cli->declareMethod("stop", [this](const Caps& args, shared_ptr<Reply>& reply) {
      cli->deleteStatus("status");
      cli->close();
      cli.reset();
      Caps data;
      data << 0;
      reply->end(&data);
    });
    cli->declareMethod("getStatus", [this](const Caps& args, shared_ptr<Reply>& reply) {
      Caps data;
      data << videoStatus;
      reply->end(&data);
    });
    cli->open([this](int32_t conn) {
      if (conn == 1) {
        Caps data;
        data << 1;
        cli->updateStatus("status", &data);
      } else
        videoStatus = 0;
    });
  }

private:
  shared_ptr<Client> cli;
  int32_t videoStatus = 0;
};

class Alarm {
public:
  ~Alarm() {
    if (cli != nullptr)
      cli->close();
  }

  void start() {
    Client::Builder builder;
    cli = builder.setUri(floraUri)
      .setId("alarm")
      .build();
    cli->open([this](int32_t conn) {
      if (conn == 1) {
        Caps data;
        data << 1;
        cli->updateStatus("status", &data);
        auto r = cli->callAsync("getStatus", "video", nullptr, 200, [this](int32_t code, const Caps& data) {
          EXPECT_TRUE(code == FLORA_CLI_SUCCESS || code == FLORA_SVC_ENEXISTS);
          int32_t st = 0;
          if (code == FLORA_CLI_SUCCESS) {
            auto it = data.iterate();
            it >> st;
          }
          if (st == 1) {
            cli->publish("floatwindow-alarm", nullptr);
          } else {
            cli->publish("sound-alarm", nullptr);
          }
        });
        EXPECT_TRUE(r);
        thrPool.push([this]() {
          usleep(500000);
          cli->close();
          cli.reset();
        });
      }
    });
  }

private:
  shared_ptr<Client> cli;
};

enum class MusicStatus {
  STOP = 0,
  PLAY,
  PAUSE
};

class Music {
public:
  ~Music() {
    if (cli != nullptr)
      cli->close();
  }

  void start() {
    Client::Builder builder;
    cli = builder.setUri(floraUri)
      .setId("music")
      .build();
    cli->declareMethod("control", [this](const Caps& args, shared_ptr<Reply>& reply) {
      control(args, reply);
    });
    cli->open(nullptr);
  }

private:
  void control(const Caps& args, shared_ptr<Reply>& reply) {
    auto it = args.iterate();
    string cmd;
    it >> cmd;
    Caps data;
    Caps ret;
    if (cmd == "play") {
      if (status != MusicStatus::PLAY) {
        resumePlay(reply);
      } else {
        ret << 0;
        reply->end(&ret);
      }
    } else if (cmd == "stop") {
      if (status != MusicStatus::STOP) {
        status = MusicStatus::STOP;
        data << 0;
        cli->updateStatus("status", &data);
      }
      ret << 0;
      reply->end(&ret);
    } else if (cmd == "pause") {
      if (status == MusicStatus::PLAY) {
        status = MusicStatus::PAUSE;
        data << 0;
        cli->updateStatus("status", &data);
      }
      ret << 0;
      reply->end(&ret);
    } else if (cmd == "resume") {
      if (status == MusicStatus::PAUSE) {
        resumePlay(reply);
      } else {
        ret << 10;
        reply->end(&ret);
      }
    } else {
      ret << -1;
      reply->end(&ret);
    }
  }

  void resumePlay(shared_ptr<Reply>& reply) {
    uint32_t* nvstatus = new uint32_t;
    *nvstatus = 0;
    auto r = cli->callAsync("getStatus", "network", nullptr, 200, [this, nvstatus](int32_t code, const Caps& data) {
      EXPECT_EQ(code, FLORA_CLI_SUCCESS);
      if (code == FLORA_CLI_SUCCESS) {
        EXPECT_EQ(data.size(), 1);
        if ((int32_t)data[0] == 0)
          *nvstatus |= 1;
      }
    });
    EXPECT_TRUE(r);
    r = cli->callAsync("getStatus", "video", nullptr, 200, [this, nvstatus](int32_t code, const Caps& data) {
      EXPECT_TRUE(code == FLORA_CLI_SUCCESS || code == FLORA_SVC_ENEXISTS);
      if (code == FLORA_CLI_SUCCESS) {
        EXPECT_EQ(data.size(), 1);
        if ((int32_t)data[0] == 1)
          *nvstatus |= 2;
      }
    });
    EXPECT_TRUE(r);
    thrPool.push([this, nvstatus, reply]() {
      usleep(300000);
      Caps ret;
      if (*nvstatus & 1) {
        ret << -2;
      } else if (*nvstatus & 2) {
        ret << -3;
      } else {
        status = MusicStatus::PLAY;
        ret << 1;
        cli->updateStatus("status", &ret);
        ret.clear();
        ret << 0;
      }
      reply->end(&ret);
      delete nvstatus;
    });
  }

private:
  MusicStatus status;
  shared_ptr<Client> cli;
};

class Launcher {
public:
  ~Launcher() {
    if (cli != nullptr)
      cli->close();
  }

  void start() {
    Client::Builder builder;
    cli = builder.setUri(floraUri)
      .setId("launcher")
      .build();
    cli->declareMethod("launch", [this](const Caps& args, shared_ptr<Reply>& reply) {
      launch(args, reply);
    });
    cli->open(nullptr);
  }

private:
  void launch(const Caps& args, shared_ptr<Reply>& reply) {
    auto it = args.iterate();
    string app;
    it >> app;
    Caps data;
    if (app == "video") {
      video.start();
      data << 0;
    } else if (app == "alarm") {
      alarm.start();
      data << 0;
    } else {
      data << -1;
    }
    reply->end(&data);
  }

private:
  shared_ptr<Client> cli;
  Video video;
  Alarm alarm;
};

TEST(MusicBoxTest, MusicBox) {
  auto tp = steady_clock::now();
  auto dur = tp.time_since_epoch();
  srand(duration_cast<microseconds>(dur).count());

  User user;
  user.start();
  Network network;
  network.start();
  Music music;
  music.start();
  Launcher launcher;
  launcher.start();
  user.waitFinish();
}
