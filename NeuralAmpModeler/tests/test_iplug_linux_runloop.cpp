#include "third_party/doctest.h"

// Linux-only. On Windows and macOS this translation unit compiles to nothing.
#if defined(__linux__)

#include "Linux/IPlugLinuxRunLoop.h"

#include <cstdint>
#include <unistd.h>
#include <vector>

namespace
{

struct FakeLoop : iplug::IPlugLinuxRunLoop
{
  struct Timer
  {
    uint32_t id = 0;
    TimerHandler handler = nullptr;
    void* user = nullptr;
  };

  std::vector<Timer> timers;
  int registers = 0;
  int unregisters = 0;
  uint32_t next = 1;

  void RegisterFd(int, FdHandler, void*) override {}
  void UnregisterFd(int) override {}

  uint32_t RegisterTimer(uint32_t, TimerHandler handler, void* userData) override
  {
    ++registers;
    const uint32_t id = next++;
    timers.push_back(Timer{id, handler, userData});
    return id;
  }

  void UnregisterTimer(uint32_t timerId) override
  {
    ++unregisters;
    for (auto it = timers.begin(); it != timers.end(); ++it)
    {
      if (it->id != timerId)
        continue;
      timers.erase(it);
      return;
    }
  }

  void Fire()
  {
    const std::vector<Timer> snapshot = timers;
    for (const auto& timer : snapshot)
    {
      if (timer.handler)
        timer.handler(timer.user);
    }
  }
};

void Count(void* user) { ++*static_cast<int*>(user); }

} // namespace

TEST_CASE("linux run loop: editor B timers survive editor A closing")
{
  // Two editors, two loops. B's timer is bound while B is the top loop.
  // Releasing A must not unregister B. Reverting IPlugLinuxReleaseRunLoop so
  // it drops every binding makes CHECK(bFires == 1) fail with bFires == 0.
  FakeLoop editorA;
  FakeLoop editorB;
  int aFires = 0;
  int bFires = 0;

  iplug::IPlugLinuxRetainRunLoop(&editorA);
  const uint32_t idA = iplug::IPlugLinuxBindTimer(16, &Count, &aFires);
  iplug::IPlugLinuxRetainRunLoop(&editorB);
  const uint32_t idB = iplug::IPlugLinuxBindTimer(16, &Count, &bFires);

  const bool releasedA = iplug::IPlugLinuxReleaseRunLoop(&editorA);
  editorB.Fire();

  CHECK(releasedA == true);
  CHECK(bFires == 1);
  CHECK(editorA.unregisters >= 1);

  iplug::IPlugLinuxUnbindTimer(idA);
  iplug::IPlugLinuxUnbindTimer(idB);
  iplug::IPlugLinuxReleaseRunLoop(&editorB);
}

TEST_CASE("linux run loop: bindings stay on the calling thread")
{
  FakeLoop loop;
  iplug::IPlugLinuxRetainRunLoop(&loop);
  const uint32_t id = iplug::IPlugLinuxBindTimer(16, &Count, nullptr);
  iplug::IPlugLinuxAssertUiThread();
  CHECK(iplug::IPlugLinuxBindingsAreOnThisThread());
  iplug::IPlugLinuxUnbindTimer(id);
  iplug::IPlugLinuxReleaseRunLoop(&loop);
}

TEST_CASE("linux poll run loop: timer fires and can unregister itself")
{
  iplug::IPlugLinuxPollRunLoop loop;
  struct State
  {
    iplug::IPlugLinuxPollRunLoop* loop = nullptr;
    uint32_t id = 0;
    int fires = 0;
  } state;
  state.loop = &loop;

  state.id = loop.RegisterTimer(10, [](void* user) {
    auto* self = static_cast<State*>(user);
    ++self->fires;
    self->loop->UnregisterTimer(self->id);
  }, &state);

  REQUIRE(state.id != 0);
  const int first = loop.Poll(80);
  CHECK(first >= 1);
  CHECK(state.fires == 1);
  const int second = loop.Poll(40);
  CHECK(second == 0);
  CHECK(state.fires == 1);
}

TEST_CASE("linux poll run loop: a readable fd is dispatched")
{
  int fds[2] = {-1, -1};
  REQUIRE(pipe(fds) == 0);

  iplug::IPlugLinuxPollRunLoop loop;
  int reads = 0;
  loop.RegisterFd(fds[0], [](int fd, void* user) {
    char byte = 0;
    const ssize_t n = ::read(fd, &byte, 1);
    if (n == 1)
      ++*static_cast<int*>(user);
  }, &reads);

  const char byte = 'x';
  REQUIRE(::write(fds[1], &byte, 1) == 1);
  const int fired = loop.Poll(50);
  CHECK(fired == 1);
  CHECK(reads == 1);

  loop.UnregisterFd(fds[0]);
  ::close(fds[0]);
  ::close(fds[1]);
}

#endif
