#pragma once

#include <atomic>
#include <thread>

namespace volum
{

// A host restores state (setState / UnserializeState) on a thread of its choosing,
// while the editor draws and handles clicks on the UI thread. The restore has to
// apply the rig synchronously - the next audio block and an immediate offline
// render must play the restored sound - but the rig appliers also write IGraphics
// controls whenever an editor exists, and IGraphics is not thread-safe.
//
// The gate names the one thread currently inside a restore. The plug-in's GetUI()
// returns no editor to that thread only, so every applier takes its headless path
// there while the UI thread keeps its editor. The restore then flags a UI resync,
// which OnIdle/OnUIOpen run on the UI thread by reading the live state.
class HostRestoreGate
{
public:
  // True only on the thread that is inside a Scope of this gate.
  bool HidesUiFromThisThread() const { return mOwner.load(std::memory_order_acquire) == std::this_thread::get_id(); }

  class Scope
  {
  public:
    explicit Scope(HostRestoreGate& gate)
    : mGate(gate)
    , mPrevious(gate.mOwner.exchange(std::this_thread::get_id(), std::memory_order_acq_rel))
    {
    }
    ~Scope() { mGate.mOwner.store(mPrevious, std::memory_order_release); }
    Scope(const Scope&) = delete;
    Scope& operator=(const Scope&) = delete;

  private:
    HostRestoreGate& mGate;
    std::thread::id mPrevious;
  };

private:
  // A default-constructed id compares unequal to every real thread.
  std::atomic<std::thread::id> mOwner{};
};

} // namespace volum
