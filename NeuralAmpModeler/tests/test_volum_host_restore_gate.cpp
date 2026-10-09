#include "third_party/doctest.h"

#include <atomic>
#include <thread>

#include "../VoLumHostRestoreGate.h"

// A host restores state on its own thread while the UI thread keeps drawing. The
// plug-in's GetUI() consults this gate, so the restore must hide the editor from
// ITS thread only, for exactly as long as it runs.

TEST_CASE("HostRestoreGate hides the editor only from the thread inside the restore")
{
  volum::HostRestoreGate gate;
  CHECK_FALSE(gate.HidesUiFromThisThread());

  {
    const volum::HostRestoreGate::Scope restoring(gate);
    CHECK(gate.HidesUiFromThisThread());

    // The UI thread is a different thread: it must keep its editor, or its own
    // controls would stop updating for as long as the host restore runs.
    std::atomic<bool> otherThreadHidden{true};
    std::thread ui([&] { otherThreadHidden = gate.HidesUiFromThisThread(); });
    ui.join();
    CHECK_FALSE(otherThreadHidden.load());
  }

  CHECK_FALSE(gate.HidesUiFromThisThread());
}

TEST_CASE("HostRestoreGate is released when the restore throws")
{
  volum::HostRestoreGate gate;
  try
  {
    const volum::HostRestoreGate::Scope restoring(gate);
    throw 1;
  }
  catch (int)
  {
  }
  CHECK_FALSE(gate.HidesUiFromThisThread());
}

TEST_CASE("HostRestoreGate nests: an inner restore does not unhide an outer one")
{
  volum::HostRestoreGate gate;
  const volum::HostRestoreGate::Scope outer(gate);
  {
    const volum::HostRestoreGate::Scope inner(gate);
    CHECK(gate.HidesUiFromThisThread());
  }
  CHECK(gate.HidesUiFromThisThread());
}

TEST_CASE("HostRestoreGate: a restore on another thread leaves this thread's editor visible")
{
  volum::HostRestoreGate gate;
  std::atomic<bool> inRestore{false};
  std::atomic<bool> release{false};
  std::atomic<bool> hiddenOnHost{false};
  std::thread host([&] {
    const volum::HostRestoreGate::Scope restoring(gate);
    hiddenOnHost = gate.HidesUiFromThisThread();
    inRestore = true;
    while (!release.load())
      std::this_thread::yield();
  });
  while (!inRestore.load())
    std::this_thread::yield();

  CHECK(hiddenOnHost.load());
  CHECK_FALSE(gate.HidesUiFromThisThread());

  release = true;
  host.join();
}

namespace
{
struct FakeGfx
{
};

// Stands for iplug::IEditorDelegate: the graphics-free helpers.
struct FakeEditorDelegate
{
  virtual ~FakeEditorDelegate() = default;
  virtual void SendControlValueFromDelegate(int, double) {}
  virtual void SendControlMsgFromDelegate(int, int, int = 0, const void* = nullptr) { ++messages; }
  virtual void SendParameterValueFromDelegate(int, double, bool) { ++paramNotifications; }
  int messages = 0;
  int paramNotifications = 0;
};

// Stands for iplug::Plugin with IGEditorDelegate underneath: every helper walks the
// editor's controls through mGraphics, and GetUI() is not virtual.
struct FakeGraphicsPlugin : FakeEditorDelegate
{
  FakeGraphicsPlugin() = default;
  FakeGfx* GetUI() { return &gfx; }
  void SendControlValueFromDelegate(int, double) override { ++controlWalks; }
  void SendControlMsgFromDelegate(int c, int m, int s = 0, const void* d = nullptr) override
  {
    ++controlWalks;
    FakeEditorDelegate::SendControlMsgFromDelegate(c, m, s, d);
  }
  void SendParameterValueFromDelegate(int p, double v, bool n) override
  {
    ++controlWalks;
    FakeEditorDelegate::SendParameterValueFromDelegate(p, v, n);
  }
  FakeGfx gfx;
  int controlWalks = 0;
};

using GuardedPlugin = volum::HostRestoreDelegate<FakeGraphicsPlugin, FakeEditorDelegate>;
} // namespace

TEST_CASE("HostRestoreDelegate: the delegate helpers reach the controls outside a restore")
{
  GuardedPlugin plugin;
  CHECK(plugin.GetUI() != nullptr);
  plugin.SendControlValueFromDelegate(1, 0.5);
  plugin.SendControlMsgFromDelegate(1, 2);
  plugin.SendParameterValueFromDelegate(3, 0.5, true);
  CHECK(plugin.controlWalks == 3);
  CHECK(plugin.messages == 1);
  CHECK(plugin.paramNotifications == 1);
}

TEST_CASE("HostRestoreDelegate: no helper reaches the controls on the restoring thread")
{
  GuardedPlugin plugin;
  {
    const volum::HostRestoreGate::Scope restoring(plugin.RestoreGate());
    CHECK(plugin.GetUI() == nullptr);
    plugin.SendControlValueFromDelegate(1, 0.5);
    plugin.SendControlMsgFromDelegate(1, 2);
    plugin.SendParameterValueFromDelegate(3, 0.5, true);

    // iPlug's IEditorDelegate behaviour is kept: the plug-in is still told.
    CHECK(plugin.controlWalks == 0);
    CHECK(plugin.messages == 1);
    CHECK(plugin.paramNotifications == 1);
  }

  // The UI resync that follows pushes everything through the normal path again.
  plugin.SendParameterValueFromDelegate(3, 0.5, true);
  CHECK(plugin.controlWalks == 1);
  CHECK(plugin.GetUI() != nullptr);
}

TEST_CASE("HostRestoreDelegate: the UI thread keeps its controls while another thread restores")
{
  GuardedPlugin plugin;
  std::atomic<bool> inRestore{false};
  std::atomic<bool> release{false};
  std::thread host([&] {
    const volum::HostRestoreGate::Scope restoring(plugin.RestoreGate());
    inRestore = true;
    while (!release.load())
      std::this_thread::yield();
  });
  while (!inRestore.load())
    std::this_thread::yield();

  CHECK(plugin.GetUI() != nullptr);
  plugin.SendParameterValueFromDelegate(3, 0.5, true);
  CHECK(plugin.controlWalks == 1);

  release = true;
  host.join();
}