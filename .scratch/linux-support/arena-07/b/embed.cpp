// Throwaway X11 host for ticket 07 screenshots. Not product code.
// Pumps IRunLoop, embeds VoLum, and holds the window until /tmp/volum-embed-done exists.

#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <dlfcn.h>
#include <poll.h>
#include <time.h>
#include <unistd.h>
#include <vector>

#include <X11/Xlib.h>

#include "pluginterfaces/base/funknown.h"
#include "pluginterfaces/base/ipluginbase.h"
#include "pluginterfaces/base/smartpointer.h"
#include "pluginterfaces/gui/iplugview.h"
#include "pluginterfaces/gui/iplugviewcontentscalesupport.h"
#include "pluginterfaces/vst/ivstaudioprocessor.h"
#include "pluginterfaces/vst/ivstcomponent.h"
#include "pluginterfaces/vst/ivsteditcontroller.h"
#include "pluginterfaces/vst/ivsthostapplication.h"

namespace
{

uint64_t NowMs()
{
  timespec ts{};
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return static_cast<uint64_t>(ts.tv_sec) * 1000u + static_cast<uint64_t>(ts.tv_nsec / 1000000);
}

class Host final : public Steinberg::Linux::IRunLoop
                 , public Steinberg::IPlugFrame
                 , public Steinberg::Vst::IHostApplication
{
public:
  Display* display = nullptr;
  Window window = 0;
  std::atomic<Steinberg::uint32> refs{1};

  struct Fd
  {
    Steinberg::Linux::IEventHandler* handler = nullptr;
    int fd = -1;
  };
  struct Timer
  {
    Steinberg::Linux::ITimerHandler* handler = nullptr;
    uint64_t intervalMs = 16;
    uint64_t nextMs = 0;
  };
  std::vector<Fd> fds;
  std::vector<Timer> timers;

  Steinberg::tresult PLUGIN_API queryInterface(const Steinberg::TUID iid, void** obj) override
  {
    if (!obj)
      return Steinberg::kInvalidArgument;
    if (Steinberg::FUnknownPrivate::iidEqual(iid, Steinberg::Linux::IRunLoop::iid) ||
        Steinberg::FUnknownPrivate::iidEqual(iid, Steinberg::FUnknown::iid))
    {
      addRef();
      *obj = static_cast<Steinberg::Linux::IRunLoop*>(this);
      return Steinberg::kResultOk;
    }
    if (Steinberg::FUnknownPrivate::iidEqual(iid, Steinberg::IPlugFrame::iid))
    {
      addRef();
      *obj = static_cast<Steinberg::IPlugFrame*>(this);
      return Steinberg::kResultOk;
    }
    if (Steinberg::FUnknownPrivate::iidEqual(iid, Steinberg::Vst::IHostApplication::iid))
    {
      addRef();
      *obj = static_cast<Steinberg::Vst::IHostApplication*>(this);
      return Steinberg::kResultOk;
    }
    *obj = nullptr;
    return Steinberg::kNoInterface;
  }
  Steinberg::uint32 PLUGIN_API addRef() override { return ++refs; }
  Steinberg::uint32 PLUGIN_API release() override { return --refs; }

  Steinberg::tresult PLUGIN_API getName(Steinberg::Vst::String128 name) override
  {
    name[0] = 'V';
    name[1] = 'o';
    name[2] = 'L';
    name[3] = 'u';
    name[4] = 'm';
    name[5] = 0;
    return Steinberg::kResultOk;
  }
  Steinberg::tresult PLUGIN_API createInstance(Steinberg::TUID, Steinberg::TUID, void** obj) override
  {
    if (obj)
      *obj = nullptr;
    return Steinberg::kNoInterface;
  }

  Steinberg::tresult PLUGIN_API registerEventHandler(Steinberg::Linux::IEventHandler* handler, Steinberg::Linux::FileDescriptor fd) override
  {
    if (!handler || fd < 0)
      return Steinberg::kInvalidArgument;
    fds.push_back(Fd{handler, fd});
    return Steinberg::kResultOk;
  }
  Steinberg::tresult PLUGIN_API unregisterEventHandler(Steinberg::Linux::IEventHandler* handler) override
  {
    for (auto it = fds.begin(); it != fds.end(); ++it)
    {
      if (it->handler != handler)
        continue;
      fds.erase(it);
      return Steinberg::kResultOk;
    }
    return Steinberg::kResultFalse;
  }
  Steinberg::tresult PLUGIN_API registerTimer(Steinberg::Linux::ITimerHandler* handler, Steinberg::Linux::TimerInterval ms) override
  {
    if (!handler || ms == 0)
      return Steinberg::kInvalidArgument;
    timers.push_back(Timer{handler, ms, NowMs() + ms});
    return Steinberg::kResultOk;
  }
  Steinberg::tresult PLUGIN_API unregisterTimer(Steinberg::Linux::ITimerHandler* handler) override
  {
    for (auto it = timers.begin(); it != timers.end(); ++it)
    {
      if (it->handler != handler)
        continue;
      timers.erase(it);
      return Steinberg::kResultOk;
    }
    return Steinberg::kResultFalse;
  }

  Steinberg::tresult PLUGIN_API resizeView(Steinberg::IPlugView* view, Steinberg::ViewRect* size) override
  {
    if (!view || !size || !display || !window)
      return Steinberg::kInvalidArgument;
    const unsigned w = static_cast<unsigned>(size->getWidth() > 0 ? size->getWidth() : 1);
    const unsigned h = static_cast<unsigned>(size->getHeight() > 0 ? size->getHeight() : 1);
    XResizeWindow(display, window, w, h);
    XFlush(display);
    view->onSize(size);
    return Steinberg::kResultOk;
  }

  void Pump(int timeoutMs)
  {
    std::vector<pollfd> pfds;
    pfds.reserve(fds.size() + 1);
    const int xfd = display ? ConnectionNumber(display) : -1;
    if (xfd >= 0)
      pfds.push_back(pollfd{xfd, POLLIN, 0});
    const size_t pluginBegin = pfds.size();
    for (const auto& fd : fds)
      pfds.push_back(pollfd{fd.fd, POLLIN, 0});

    poll(pfds.data(), static_cast<nfds_t>(pfds.size()), timeoutMs);
    if (display)
    {
      while (XPending(display))
      {
        XEvent event;
        XNextEvent(display, &event);
      }
    }

    std::vector<Fd> fdCopy = fds;
    for (size_t i = 0; i < fdCopy.size(); ++i)
    {
      const size_t index = pluginBegin + i;
      if (index >= pfds.size() || !(pfds[index].revents & (POLLIN | POLLERR | POLLHUP)))
        continue;
      bool still = false;
      for (const auto& live : fds)
        still = still || live.handler == fdCopy[i].handler;
      if (still && fdCopy[i].handler)
        fdCopy[i].handler->onFDIsSet(fdCopy[i].fd);
    }

    const uint64_t now = NowMs();
    std::vector<Timer> due;
    for (auto& timer : timers)
    {
      if (now >= timer.nextMs)
      {
        due.push_back(timer);
        timer.nextMs = now + timer.intervalMs;
      }
    }
    for (const auto& timer : due)
    {
      bool still = false;
      for (const auto& live : timers)
        still = still || live.handler == timer.handler;
      if (still && timer.handler)
        timer.handler->onTimer();
    }
  }
};

int Fail(const char* text)
{
  std::fprintf(stderr, "embed: %s\n", text);
  return 1;
}

} // namespace

int main(int argc, char** argv)
{
  const char* modulePath = argc > 1 ? argv[1] : "build-linux/VoLum.vst3/Contents/x86_64-linux/VoLum.so";
  const float scale = argc > 2 ? static_cast<float>(std::atof(argv[2])) : 1.f;
  std::remove("/tmp/volum-embed-ready");
  std::remove("/tmp/volum-embed-done");

  void* module = dlopen(modulePath, RTLD_NOW | RTLD_LOCAL);
  if (!module)
    return Fail(dlerror());

  auto entry = reinterpret_cast<bool (*)(void*)>(dlsym(module, "ModuleEntry"));
  auto factoryFn = reinterpret_cast<Steinberg::IPluginFactory* (*)()>(dlsym(module, "GetPluginFactory"));
  if (!entry || !factoryFn || !entry(nullptr))
    return Fail("module entry");

  Steinberg::IPluginFactory* factory = factoryFn();
  if (!factory)
    return Fail("factory");

  Host host;
  host.display = XOpenDisplay(nullptr);
  if (!host.display)
    return Fail("XOpenDisplay");

  Steinberg::FUnknownPtr<Steinberg::IPluginFactory3> factory3(factory);
  if (factory3)
    factory3->setHostContext(static_cast<Steinberg::Linux::IRunLoop*>(&host));

  Steinberg::PClassInfo info{};
  if (factory->getClassInfo(0, &info) != Steinberg::kResultOk)
    return Fail("class info");

  void* raw = nullptr;
  if (factory->createInstance(info.cid, Steinberg::Vst::IComponent::iid, &raw) != Steinberg::kResultOk || !raw)
    return Fail("createInstance");

  auto* component = static_cast<Steinberg::Vst::IComponent*>(raw);
  if (component->initialize(static_cast<Steinberg::Linux::IRunLoop*>(&host)) != Steinberg::kResultOk)
    return Fail("initialize");

  Steinberg::Vst::IEditController* controller = nullptr;
  if (component->queryInterface(Steinberg::Vst::IEditController::iid, reinterpret_cast<void**>(&controller)) != Steinberg::kResultOk || !controller)
    return Fail("controller");

  Steinberg::IPlugView* view = controller->createView("editor");
  if (!view)
    return Fail("createView");

  Steinberg::ViewRect size{};
  if (view->getSize(&size) != Steinberg::kResultOk)
    return Fail("getSize");
  const int logicalW = size.getWidth() > 0 ? size.getWidth() : 1;
  const int logicalH = size.getHeight() > 0 ? size.getHeight() : 1;
  const int pixelW = static_cast<int>(logicalW * (scale > 0.f ? scale : 1.f));
  const int pixelH = static_cast<int>(logicalH * (scale > 0.f ? scale : 1.f));
  std::printf("embed: logical %dx%d scale %.2f pixel %dx%d\n", logicalW, logicalH, scale, pixelW, pixelH);
  std::fflush(stdout);

  const int screen = DefaultScreen(host.display);
  host.window = XCreateSimpleWindow(host.display, RootWindow(host.display, screen), 0, 0,
                                    static_cast<unsigned>(pixelW), static_cast<unsigned>(pixelH), 0, 0, 0x101010);
  XMapWindow(host.display, host.window);
  XFlush(host.display);

  view->setFrame(static_cast<Steinberg::IPlugFrame*>(&host));
  const void* parent = reinterpret_cast<void*>(static_cast<uintptr_t>(host.window));
  if (view->attached(const_cast<void*>(parent), Steinberg::kPlatformTypeX11EmbedWindowID) != Steinberg::kResultOk)
    return Fail("attached");

  Steinberg::IPlugViewContentScaleSupport* scaling = nullptr;
  if (view->queryInterface(Steinberg::IPlugViewContentScaleSupport::iid, reinterpret_cast<void**>(&scaling)) == Steinberg::kResultOk && scaling)
  {
    scaling->setContentScaleFactor(scale > 0.f ? scale : 1.f);
    scaling->release();
  }

  const uint64_t warmupEnd = NowMs() + 800;
  while (NowMs() < warmupEnd)
    host.Pump(16);

  FILE* ready = std::fopen("/tmp/volum-embed-ready", "w");
  if (ready)
  {
    std::fputs("ready\n", ready);
    std::fclose(ready);
  }

  while (access("/tmp/volum-embed-done", F_OK) != 0)
    host.Pump(16);

  view->removed();
  view->release();
  controller->release();
  component->terminate();
  component->release();
  auto exitFn = reinterpret_cast<bool (*)()>(dlsym(module, "ModuleExit"));
  if (exitFn)
    exitFn();
  dlclose(module);
  XDestroyWindow(host.display, host.window);
  XCloseDisplay(host.display);
  return 0;
}
