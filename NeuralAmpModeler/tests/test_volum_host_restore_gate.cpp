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
