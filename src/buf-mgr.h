#pragma once

#include <sys/mman.h>
#include <mutex>
#include <chrono>
#include "common.h"

class FixedBufferManager {
public:
  void init(uint32_t size, bool mthr) {
    bufsize = size;
    multiThread = mthr;
  }

  inline uint32_t getBufsize() const { return bufsize; }

  char* getBuffer() {
    unique_lock<mutex> locker(bmMutex, defer_lock);
    if (multiThread)
      locker.lock();
    if (idleBuffers.empty())
      return allocBuffer();
    auto res = idleBuffers.front();
    idleBuffers.erase(idleBuffers.begin());
    return res;
  }

  void putBuffer(char* buf) {
    if (multiThread)
      bmMutex.lock();
    idleBuffers.push_back(buf);
    if (multiThread)
      bmMutex.unlock();
  }

private:
  char* allocBuffer() {
    auto ptr = mmap(nullptr, bufsize, PROT_READ | PROT_WRITE,
        MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
    if (ptr == (void*)-1)
      return nullptr;
    return (char*)ptr;
  }

private:
  typedef list<char*> BufferList;

  uint32_t bufsize;
  BufferList idleBuffers;
  mutex bmMutex;
  bool multiThread;
};
