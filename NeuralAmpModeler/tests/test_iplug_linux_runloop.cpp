#include "third_party/doctest.h"

#ifdef __linux__

  // The registry is iPlug2 code the Linux plug-in compiles; the test binary
  // does not link iPlug2, so the translation unit is included here.
  #include "../../iPlug2/IPlug/Linux/IPlugLinuxRunLoop.cpp"

  #include <chrono>
  #include <thread>
  #include <unistd.h>
  #include <vector>

using iplug::IPlugLinuxAddRunLoop;
using iplug::IPlugLinuxBindFd;
using iplug::IPlugLinuxBindTimer;
using iplug::IPlugLinuxGetRunLoop;
using iplug::IPlugLinuxPollRunLoop;
using iplug::IPlugLinuxRemoveRunLoop;
using iplug::IPlugLinuxRunLoop;
using iplug::IPlugLinuxUnbindFd;
using iplug::IPlugLinuxUnbindTimer;

namespace
{
// A host IRunLoop as the registry sees it. Fire*() plays the host calling back.
class FakeHostLoop final : public IPlugLinuxRunLoop
{
public:
  struct Timer
  {
    uint32_t id;
    TimerHandler handler;
    void* userData;
  };
  struct Fd
  {
    int fd;
    FdHandler handler;
    void* userData;
  };

  void RegisterFd(int fd, FdHandler handler, void* userData) override { fds.push_back({fd, handler, userData}); }

  void UnregisterFd(int fd) override
  {
    for (size_t i = 0; i < fds.size(); ++i)
      if (fds[i].fd == fd)
      {
        fds.erase(fds.begin() + i);
        return;
      }
  }

  uint32_t RegisterTimer(uint32_t, TimerHandler handler, void* userData) override
  {
    timers.push_back({nextId, handler, userData});
    return nextId++;
  }

  void UnregisterTimer(uint32_t id) override
  {
    for (size_t i = 0; i < timers.size(); ++i)
      if (timers[i].id == id)
      {
        timers.erase(timers.begin() + i);
        return;
      }
  }

  void FireTimers()
  {
    const std::vector<Timer> registered = timers;
    for (const auto& timer : registered)
      timer.handler(timer.userData);
  }

  void FireFds()
  {
    const std::vector<Fd> registered = fds;
    for (const auto& fd : registered)
      fd.handler(fd.fd, fd.userData);
  }

  std::vector<Timer> timers;
  std::vector<Fd> fds;
  uint32_t nextId = 1;
};

void Count(void* userData) { ++*static_cast<int*>(userData); }
void CountFd(int, void* userData) { ++*static_cast<int*>(userData); }
} // namespace

TEST_CASE("IPlugLinuxRunLoop: instance B's timers keep firing after instance A's editor closes")
{
  // Ticket 05 kept one process-wide loop; closing A's editor cleared it and
  // detached every timer, including B's.
  FakeHostLoop frameA;
  FakeHostLoop frameB;
  int idleA = 0;
  int idleB = 0;
  const uint32_t timerA = IPlugLinuxBindTimer(20, Count, &idleA); // IPlugAPIBase timers exist before any editor
  const uint32_t timerB = IPlugLinuxBindTimer(20, Count, &idleB);

  IPlugLinuxAddRunLoop(&frameA); // A's editor: attached()
  IPlugLinuxAddRunLoop(&frameB); // B's editor: attached()
  int drawB = 0;
  const uint32_t drawTimerB = IPlugLinuxBindTimer(16, Count, &drawB);
  CHECK(IPlugLinuxGetRunLoop() == &frameB);
  CHECK(frameA.timers.size() == 2);
  CHECK(frameB.timers.size() == 1);

  IPlugLinuxRemoveRunLoop(&frameA); // A's editor: removed()
  CHECK(frameA.timers.empty());
  frameB.FireTimers();
  CHECK(idleB == 1);
  CHECK(idleA == 1);
  CHECK(drawB == 1);

  IPlugLinuxUnbindTimer(drawTimerB);
  IPlugLinuxUnbindTimer(timerA);
  IPlugLinuxUnbindTimer(timerB);
  IPlugLinuxRemoveRunLoop(&frameB);
  CHECK(frameB.timers.empty());
  CHECK(IPlugLinuxGetRunLoop() == nullptr);
}

TEST_CASE("IPlugLinuxRunLoop: the editor's X fd moves when its loop is popped, and loops are reference counted")
{
  FakeHostLoop hostContext;
  FakeHostLoop frame;
  IPlugLinuxAddRunLoop(&hostContext);
  IPlugLinuxAddRunLoop(&frame);
  IPlugLinuxAddRunLoop(&frame); // a second reference to the same loop

  int reads = 0;
  const uint32_t fdBinding = IPlugLinuxBindFd(7, CountFd, &reads);
  REQUIRE(frame.fds.size() == 1);
  CHECK(hostContext.fds.empty());

  IPlugLinuxRemoveRunLoop(&frame);
  CHECK(frame.fds.size() == 1); // one reference left
  IPlugLinuxRemoveRunLoop(&frame);
  CHECK(frame.fds.empty());
  REQUIRE(hostContext.fds.size() == 1);
  hostContext.FireFds();
  CHECK(reads == 1);

  IPlugLinuxUnbindFd(fdBinding);
  CHECK(hostContext.fds.empty());
  IPlugLinuxRemoveRunLoop(&hostContext);
  CHECK(IPlugLinuxGetRunLoop() == nullptr);
}

TEST_CASE("IPlugLinuxRunLoop: a timer made before any loop attaches later and can unbind from its own callback")
{
  struct SelfStop
  {
    uint32_t binding = 0;
    int calls = 0;
  } self;
  self.binding = IPlugLinuxBindTimer(
    10,
    [](void* userData) {
      auto* s = static_cast<SelfStop*>(userData);
      ++s->calls;
      IPlugLinuxUnbindTimer(s->binding);
    },
    &self);

  FakeHostLoop loop;
  IPlugLinuxAddRunLoop(&loop);
  REQUIRE(loop.timers.size() == 1);
  loop.FireTimers();
  loop.FireTimers();
  CHECK(self.calls == 1);
  CHECK(loop.timers.empty());
  IPlugLinuxRemoveRunLoop(&loop);
}

TEST_CASE("IPlugLinuxRunLoop: binding is UI-thread only")
{
  int ticks = 0;
  const uint32_t binding = IPlugLinuxBindTimer(10, Count, &ticks); // latches this thread as the UI thread
  IPlugLinuxUnbindTimer(binding);
  CHECK(iplug::IPlugLinuxIsUIThread());

  bool otherIsUIThread = true;
  std::thread([&otherIsUIThread] { otherIsUIThread = iplug::IPlugLinuxIsUIThread(); }).join();
  CHECK_FALSE(otherIsUIThread);
  CHECK(iplug::IPlugLinuxOffThreadCalls() == 0);
}

TEST_CASE("IPlugLinuxPollRunLoop dispatches a readable fd and timerfd timers")
{
  IPlugLinuxPollRunLoop loop;
  int pipeFds[2] = {-1, -1};
  REQUIRE(pipe(pipeFds) == 0);

  int reads = 0;
  loop.RegisterFd(
    pipeFds[0],
    [](int fd, void* userData) {
      char c = 0;
      CHECK(read(fd, &c, 1) == 1);
      ++*static_cast<int*>(userData);
    },
    &reads);
  CHECK(loop.RunOnce(0) == 0);
  REQUIRE(write(pipeFds[1], "x", 1) == 1);
  CHECK(loop.RunOnce(1000) == 1);
  CHECK(reads == 1);

  int ticks = 0;
  const uint32_t timer = loop.RegisterTimer(5, Count, &ticks);
  REQUIRE(timer != 0);
  const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
  while (ticks < 3 && std::chrono::steady_clock::now() < deadline)
    loop.RunOnce(100);
  CHECK(ticks >= 3);

  struct SelfStop
  {
    IPlugLinuxPollRunLoop* loop = nullptr;
    uint32_t id = 0;
    int calls = 0;
  } self;
  self.loop = &loop;
  self.id = loop.RegisterTimer(
    1,
    [](void* userData) {
      auto* s = static_cast<SelfStop*>(userData);
      ++s->calls;
      s->loop->UnregisterTimer(s->id);
    },
    &self);
  REQUIRE(self.id != 0);
  for (int i = 0; i < 20; ++i)
    loop.RunOnce(5);
  CHECK(self.calls == 1);

  loop.UnregisterTimer(timer);
  loop.UnregisterFd(pipeFds[0]);
  REQUIRE(write(pipeFds[1], "y", 1) == 1);
  CHECK(loop.RunOnce(0) == 0);
  close(pipeFds[0]);
  close(pipeFds[1]);
}

#endif
