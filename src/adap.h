#pragma once

#include <unistd.h>
#include <fcntl.h>
#include <sys/un.h>
#include <netinet/in.h>
#include <netdb.h>
#include <set>
#include "caps.h"
#include "buf-mgr.h"
#include "uri.h"
#include "rlog.h"

class Adapter {
public:
  virtual ~Adapter() = default;

  virtual bool read() = 0;

  virtual int32_t read(Caps& data) = 0;

  bool write(const Caps& data) {
    uint32_t c;
    try {
      c = data.serialize(writeBuffer, bufsize);
    } catch (exception& e) {
      ROKID_GERROR(STAG, FLORA_SVC_EINVAL,
          "adapter write failed: corrupted caps");
      return false;
    }
    return write(writeBuffer, c);
  }

  virtual bool write(const void* data, uint32_t size) = 0;

  virtual void shutdown() = 0;

  virtual void close() = 0;

  virtual bool setWriteTimeout(uint32_t timeout) = 0;

  virtual bool setReadTimeout(uint32_t timeout) = 0;

protected:
  int32_t readFromBuffer(Caps& data) {
    auto size = readDataSize - readPos;
    if (size < sizeof(uint32_t))
      return 1;
    uint32_t csz;
    beReadU32((uint8_t*)(readBuffer + readPos), csz);
    if (csz > bufsize) {
      ROKID_GERROR(STAG, FLORA_SVC_ENOBUF,
          "adapter read buffer not enough, need %u, bufsize %u",
          csz, bufsize);
      return -1;
    }
    if (csz > size) {
      if (readPos) {
        memmove(readBuffer, readBuffer + readPos, size);
        readDataSize -= readPos;
        readPos = 0;
      }
      return 1;
    }
    try {
      data.parse(readBuffer + readPos, csz);
    } catch (exception& e) {
      ROKID_GERROR(STAG, FLORA_SVC_EINVAL,
          "adapter read failed: invalid caps format");
      return -1;
    }
    readPos += csz;
    return 0;
  }

  virtual bool doRead() = 0;

protected:
  char* readBuffer{nullptr};
  uint32_t readPos{0};
  uint32_t readDataSize{0};
  char* writeBuffer{nullptr};
  uint32_t bufsize;
};

class MsgWriteThread;
class ServiceAdapter : public Adapter {
public:
  virtual ~ServiceAdapter();

  int32_t read(Caps& data) {
    auto r = readFromBuffer(data);
    if ((r == 0 && readPos == readDataSize) || r < 0) {
      bufferManager->putBuffer(readBuffer);
      readBuffer = nullptr;
      readPos = 0;
      readDataSize = 0;
    }
    return r;
  }

  bool read() {
    if (readBuffer == nullptr)
      readBuffer = bufferManager->getBuffer();
    return doRead();
  }

  void setAuthorized() {
  }

  void addStatus(const string& name) {
    allStatus.insert(name);
  }

  void eraseStatus(const string& name) {
    allStatus.erase(name);
  }

protected:
  FixedBufferManager* bufferManager;

public:
  string name;
  // if unix socket
  // |type|authorized|reserved| pid |
  // | 63 |    62    |  61~32 | 31~0|
  // if tcp socket
  // |type|authorized|reserved| port|ipv4|
  // | 63 |    62    |  61~48 |47~32|31~0|
  uint64_t tag;
  MsgWriteThread* writeThread;
  set<string> allStatus;
};

template <typename BASE,
         typename enable_if<is_base_of<Adapter, BASE>::value, BASE>::type* = nullptr>
class SocketAdapter : public BASE {
public:
  explicit SocketAdapter(uint32_t bsz) : BASE{bsz} {}

  SocketAdapter(int fd, int type) : socketfd{fd}, socketType{type} {}

  virtual ~SocketAdapter() {
    ::close(socketfd);
  }

  int32_t getSocket() {
    return socketfd;
  }

protected:
  bool doRead() {
    assert(BASE::readPos == 0);
    auto size = BASE::bufsize - BASE::readDataSize;
    if (size == 0) {
      ROKID_GERROR(STAG, FLORA_SVC_ENOBUF, "adapter read buffer not enough");
      return false;
    }
    ssize_t c;
    while ((c = ::read(socketfd, BASE::readBuffer + BASE::readDataSize, size))
        == -1 && errno == EINTR);
    if (c == -1) {
      ROKID_GERROR(STAG, FLORA_SVC_ESYS, "read socket failed: %s",
          strerror(errno));
      return false;
    }
    if (c == 0) {
      ROKID_GERROR(STAG, FLORA_SVC_ESYS, "socket shutdown or remote closed");
      return false;
    }
    BASE::readDataSize += c;
    return true;
  }

public:
  bool write(const void* data, uint32_t size) {
    ssize_t c;
    while ((c = ::write(socketfd, data, size)) == -1 && errno == EINTR);
    if (c == -1) {
      ROKID_GERROR(STAG, FLORA_SVC_ESYS, "socket write failed: %s",
          strerror(errno));
      return false;
    }
    if (c == 0) {
      ROKID_GERROR(STAG, FLORA_SVC_ESYS, "socket write failed: remote closed");
      return false;
    }
    return true;
  }

  void shutdown() {
    ::shutdown(socketfd, SHUT_RD);
  }

  void close() {
    ::close(socketfd);
    socketfd = -1;
  }

  bool setWriteTimeout(uint32_t timeout) {
    return setSocketTimeout(socketfd, timeout, false);
  }

  bool setReadTimeout(uint32_t timeout) {
    return setSocketTimeout(socketfd, timeout, true);
  }

protected:
  int socketfd{-1};
  int socketType{-1};
};

class ServiceSocketAdapter : public SocketAdapter<ServiceAdapter> {
public:
  ServiceSocketAdapter(FixedBufferManager* bm, int fd, int type)
    : SocketAdapter<ServiceAdapter>{fd, type} {
    bufferManager = bm;
    bufsize = bm->getBufsize();
  }
};

class ClientAdapter : public Adapter {
public:
  explicit ClientAdapter(uint32_t bsz) {
    auto p = (char*)mmap(nullptr, bsz << 1, PROT_WRITE | PROT_READ,
        MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
    if ((intptr_t)p == -1)
      return;
    readBuffer = p;
    writeBuffer = p + bsz;
    bufsize = bsz;
  }

  virtual ~ClientAdapter() {
    munmap(readBuffer, bufsize << 1);
  }

  virtual bool connect(const Uri& urip, uint32_t timeout) = 0;

  bool read() {
    return doRead();
  }

  int32_t read(Caps& data) {
    auto r = readFromBuffer(data);
    if ((r == 0 && readPos == readDataSize) || r < 0) {
      readPos = 0;
      readDataSize = 0;
    }
    return r;
  }
};

class ClientSocketAdapter : public SocketAdapter<ClientAdapter> {
public:
  ClientSocketAdapter(uint32_t bsz) : SocketAdapter{bsz} {}

  bool connect(const Uri& urip, uint32_t timeout) {
    int fd;
    int sockType;
    if (urip.scheme == "unix") {
      fd = createUnixSocket();
      sockType = 0;
    } else if(urip.scheme == "tcp") {
      fd = createTcpSocket();
      sockType = 1;
    } else {
      ROKID_GERROR(CTAG, FLORA_CLI_EINVAL, "uri's scheme %s is invalid",
          urip.scheme.c_str());
      return false;
    }
    if (fd < 0)
      return false;
    auto fl = fcntl(fd, F_GETFL);
    fl |= O_NONBLOCK;
    fcntl(fd, F_SETFL, fl);
    bool r = false;
    if (sockType == 0)
      r = connectUnix(fd, urip);
    else if (sockType == 1)
      r = connectTcp(fd, urip);
    if (!r) {
      if (errno != EINPROGRESS) {
        ROKID_GERROR(CTAG, FLORA_CLI_ESYS, "connect failed: %s",
            strerror(errno));
        return false;
      }
      auto conntp = steady_clock::now();
      uint32_t elapsed{0};
      fd_set fdset;
      int sr;
      do {
        FD_ZERO(&fdset);
        FD_SET(fd, &fdset);
        if (timeout) {
          struct timeval tv;
          tv.tv_sec = (timeout - elapsed) / 1000;
          tv.tv_usec = (timeout - elapsed) % 1000 * 1000;
          sr = select(fd + 1, nullptr, &fdset, nullptr, &tv);
        } else {
          sr = select(fd + 1, nullptr, &fdset, nullptr, nullptr);
        }
        if (sr >= 0)
          break;
        if (errno != EINTR)
          break;
        auto nowtp = steady_clock::now();
        elapsed = duration_cast<milliseconds>(nowtp - conntp).count();
      } while (elapsed < timeout);
      if (sr <= 0) {
        ::close(fd);
        if (sr < 0)
          ROKID_GERROR(CTAG, FLORA_CLI_ESYS, "wait connected failed: %s",
              strerror(errno));
        else
          ROKID_GERROR(CTAG, FLORA_CLI_ETIMEOUT, "wait connected timeout");
        return false;
      }
    }
    fl &= (~O_NONBLOCK);
    fcntl(fd, F_SETFL, fl);
    socketfd = fd;
    socketType = sockType ? SOCKET_TYPE_TCP : 0;
    return true;
  }

private:
  int createUnixSocket() {
#ifdef __APPLE__
    auto fd = socket(AF_UNIX, SOCK_STREAM, 0);
#else
    auto fd = socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0);
#endif
    if (fd < 0) {
      ROKID_GERROR(CTAG, FLORA_CLI_ESYS, "create socket failed: %s",
          strerror(errno));
      return -1;
    }
#ifdef __APPLE__
    auto f = fcntl(fd, F_GETFD);
    f |= FD_CLOEXEC;
    fcntl(fd, F_SETFD, f);
#endif
    return fd;
  }

  int createTcpSocket() {
#ifdef __APPLE__
    auto fd = socket(AF_INET, SOCK_STREAM, 0);
#else
    auto fd = socket(AF_INET, SOCK_STREAM | SOCK_CLOEXEC, 0);
#endif
    if (fd < 0) {
      ROKID_GERROR(CTAG, FLORA_CLI_ESYS, "create socket failed: %s",
          strerror(errno));
      return -1;
    }
#ifdef __APPLE__
    auto f = fcntl(fd, F_GETFD);
    f |= FD_CLOEXEC;
    fcntl(fd, F_SETFD, f);
#endif
    return fd;
  }

  bool connectUnix(int fd, const Uri& urip) {
    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    uint32_t abslen = urip.path.length() + 1;
    if (abslen > sizeof(addr.sun_path))
      abslen = sizeof(addr.sun_path);
#ifdef __APPLE__
    strncpy(addr.sun_path, urip.path.c_str(), abslen);
#else
    addr.sun_path[0] = '\0';
    memcpy(addr.sun_path + 1, urip.path.data(), abslen - 1);
#endif
    abslen += offsetof(sockaddr_un, sun_path);
    return ::connect(fd, (sockaddr *)&addr, abslen) == 0;
  }

  bool connectTcp(int fd, const Uri& urip) {
    struct sockaddr_in addr;
    struct hostent *hp;
    hp = gethostbyname(urip.host.c_str());
    if (hp == nullptr) {
      ROKID_GERROR(CTAG, FLORA_CLI_ESYS, "dns failed for host %s: %s",
          urip.host.c_str(), strerror(errno));
      return false;
    }
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    memcpy(&addr.sin_addr, hp->h_addr_list[0], sizeof(addr.sin_addr));
    addr.sin_port = htons(urip.port);
    return ::connect(fd, (sockaddr *)&addr, sizeof(addr)) == 0;
  }
};
