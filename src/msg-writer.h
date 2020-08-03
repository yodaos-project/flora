#pragma once

#include <atomic>
#include "data-tracer.h"

#define WT_STATUS_STOPPED 0
#define WT_STATUS_RUNNING 1

class MsgWriteThread {
public:
  void put(shared_ptr<ServiceAdapter>& adap, Caps& data, bool shutdown) {
    taskMutex.lock();
    pendingTasks.emplace(pendingTasks.end(), adap, move(data), shutdown);
    taskAvail.notify_one();
    taskMutex.unlock();
  }

  void start(FixedBufferManager* bm) {
    buffer = bm->getBuffer();
    bufsize = bm->getBufsize();
    status = WT_STATUS_RUNNING;
    runThread = thread([this]() {
      run();
    });
  }

  void stop() {
    unique_lock<mutex> locker(taskMutex);
    status = WT_STATUS_STOPPED;
    taskAvail.notify_one();
    locker.unlock();
    runThread.join();
  }

  void bindAdapter() {
    ++numAdapters;
  }

  void unbindAdapter() {
    --numAdapters;
  }

  inline uint32_t getAdapterNum() const { return numAdapters.load(); }

private:
  void run() {
    unique_lock<mutex> locker(taskMutex);
    while (status == WT_STATUS_RUNNING) {
      if (pendingTasks.empty())
        taskAvail.wait(locker);
      else {
        TaskList tasks = move(pendingTasks);
        locker.unlock();
        TaskList::iterator it = tasks.begin();
        while (it != tasks.end()) {
          auto adap = it->adapter.lock();
          if (adap != nullptr) {
            TRACE_RESP_CMD(adap, it->data);
            try {
              auto c = it->data.serialize(buffer, bufsize);
              if (!adap->write(buffer, c))
                printGlobalError();
            } catch (exception& e) {
              KLOGW(STAG, "caps serialize failed: %s", e.what());
            }
            if (it->shutdownAfterWrite)
              adap->shutdown();
          }
          ++it;
        }
        locker.lock();
      }
    }
  }

private:
  class Task {
  public:
    Task(shared_ptr<ServiceAdapter>& adap, Caps&& d, bool shut)
      : adapter{adap}, data{d}, shutdownAfterWrite{shut} {}

    weak_ptr<ServiceAdapter> adapter;
    Caps data;
    bool shutdownAfterWrite;
  };
  typedef list<Task> TaskList;

  mutex taskMutex;
  condition_variable taskAvail;
  TaskList pendingTasks;
  thread runThread;
  char* buffer;
  uint32_t bufsize;
  uint32_t status{WT_STATUS_STOPPED};
  atomic<uint32_t> numAdapters{0};
}; // class MsgWriteThread

class MsgWriter {
public:

public:
  void init(uint32_t wnum, FixedBufferManager* bm,
      AdapterManager* am) {
    writeThreads = new MsgWriteThread[wnum];
    writeThreadNum = wnum;
    uint32_t i;
    for (i = 0; i < wnum; ++i)
      writeThreads[i].start(bm);
    adapterManager = am;
  }

  void put(shared_ptr<ServiceAdapter>& adap, Caps& data, bool shutdown) {
    adap->writeThread->put(adap, data, shutdown);
  }

  void bind(shared_ptr<ServiceAdapter>& adap) {
    // 根据线程绑定的adapters平均数做负载均衡
    // adapter析构时自动释放
    auto avgNum = adapterManager->getAdapterNum() / writeThreadNum + 1;
    for (uint32_t i = 0; i < writeThreadNum; ++i) {
      if (writeThreads[i].getAdapterNum() < avgNum) {
        adap->writeThread = writeThreads + i;
        writeThreads[i].bindAdapter();
        return;
      }
    }
    KLOGE(STAG, "adapter bind write thread failed: average %u", avgNum);
    assert(false);
  }

  void finish() {
    for (uint32_t i = 0; i < writeThreadNum; ++i)
      writeThreads[i].stop();
    delete[] writeThreads;
    writeThreads = nullptr;
    adapterManager = nullptr;
  }

private:
  MsgWriteThread* writeThreads;
  AdapterManager* adapterManager;
  uint32_t writeThreadNum;
}; // class MsgWriter
