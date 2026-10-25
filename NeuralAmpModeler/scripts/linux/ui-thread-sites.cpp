// Debug harness for the four IPlugLinuxCheckUIThread sites in
// IGraphicsLinux.cpp. Links the built VoLum.so (the sites live there) and
// calls each one from a second thread. A zeroed object is enough: the check
// is the first statement and does not read members, and the call is
// non-virtual so a null vptr is not loaded.
//
// PASS: stderr contains one "called off the UI thread" line for OpenWindow,
// CloseWindow, ProcessX11Events and OnDisplayTimer, and the registry count
// rose. OpenWindow continues into X11 after the check and may SIGSEGV; that is
// caught.

#include "IGraphicsLinux.h"
#include "Linux/IPlugLinuxRunLoop.h"

#include <csetjmp>
#include <csignal>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <sstream>
#include <string>
#include <thread>

namespace
{
thread_local sigjmp_buf gJmp;

void OnSignal(int sig)
{
  siglongjmp(gJmp, sig);
}

template <typename Fn>
int CallOffThread(Fn&& fn)
{
  int sig = 0;
  std::thread([&]() {
    struct sigaction sa
    {
    };
    sa.sa_handler = OnSignal;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGABRT, &sa, nullptr);
    sigaction(SIGSEGV, &sa, nullptr);
    sigaction(SIGBUS, &sa, nullptr);
    sig = sigsetjmp(gJmp, 1);
    if (sig == 0)
      fn();
  }).join();
  return sig;
}
} // namespace

int main(int argc, char** argv)
{
  if (argc != 2)
  {
    std::fprintf(stderr, "usage: ui-thread-sites LOG\n");
    return 2;
  }
  if (!std::freopen(argv[1], "w", stderr))
  {
    std::fprintf(stdout, "ui-thread sites: cannot write %s\n", argv[1]);
    return 2;
  }

  using namespace iplug;
  using namespace igraphics;

  // The first registry call latches this thread as the UI thread. The four
  // sites then run on a different thread, so they count as off-thread.
  IPlugLinuxCheckUIThread("harness-latch");
  const int latched = IPlugLinuxOffThreadCalls();
  if (latched != 0)
  {
    std::printf("ui-thread sites: latch counted %d\n", latched);
    return 1;
  }

  alignas(IGraphicsLinux) static unsigned char storage[sizeof(IGraphicsLinux)];
  auto* graphics = reinterpret_cast<IGraphicsLinux*>(storage);
  const int before = IPlugLinuxOffThreadCalls();

  std::memset(storage, 0, sizeof(storage));
  CallOffThread([&]() { graphics->IGraphicsLinux::CloseWindow(); });
  std::memset(storage, 0, sizeof(storage));
  CallOffThread([&]() { graphics->ProcessX11Events(); });
  std::memset(storage, 0, sizeof(storage));
  CallOffThread([&]() { graphics->OnDisplayTimer(); });
  std::memset(storage, 0, sizeof(storage));
  const int openSig = CallOffThread([&]() { graphics->IGraphicsLinux::OpenWindow(nullptr); });

  std::fflush(stderr);
  const int after = IPlugLinuxOffThreadCalls();
  std::ifstream in(argv[1]);
  std::stringstream buffer;
  buffer << in.rdbuf();
  const std::string log = buffer.str();

  const char* sites[] = {
    "IGraphicsLinux::OpenWindow called off the UI thread",
    "IGraphicsLinux::CloseWindow called off the UI thread",
    "IGraphicsLinux::ProcessX11Events called off the UI thread",
    "IGraphicsLinux::OnDisplayTimer called off the UI thread",
  };
  int missing = 0;
  for (const char* site : sites)
  {
    if (log.find(site) == std::string::npos)
    {
      std::printf("ui-thread sites: missing %s\n", site);
      ++missing;
    }
  }
  std::printf("ui-thread sites: count %d -> %d, OpenWindow signal %d\n", before, after, openSig);
  if (missing != 0 || after <= before)
  {
    std::printf("ui-thread sites: FAIL\n");
    return 1;
  }
  std::printf("ui-thread sites: PASS\n");
  return 0;
}
