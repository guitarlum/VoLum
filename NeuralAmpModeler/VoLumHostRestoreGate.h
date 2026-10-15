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
    ~Scope()
    {
      mGate.mOwner.store(mPrevious, std::memory_order_release);
      // The API wrappers (VST3 setState, AU, AAX, VST2) call OnRestoreState() on this
      // same thread right after UnserializeState() returns. Leave a one-shot mark for
      // that call so it is deferred like the rest of the restore's UI work.
      if (mPrevious == std::thread::id{})
        mGate.mTail.store(std::this_thread::get_id(), std::memory_order_release);
    }
    Scope(const Scope&) = delete;
    Scope& operator=(const Scope&) = delete;

  private:
    HostRestoreGate& mGate;
    std::thread::id mPrevious;
  };

  // True once, on the thread whose restore just ended, until it is taken.
  bool TakeRestoreTail()
  {
    std::thread::id me = std::this_thread::get_id();
    return mTail.compare_exchange_strong(me, std::thread::id{}, std::memory_order_acq_rel);
  }

private:
  // A default-constructed id compares unequal to every real thread.
  std::atomic<std::thread::id> mOwner{};
  std::atomic<std::thread::id> mTail{};
};

// The base layer between the iPlug plug-in class and VoLum. GetUI() is not virtual,
// and iPlug's own editor-delegate helpers reach the editor through mGraphics
// directly, so hiding GetUI() alone does not stop them: SendParameterValueFromDelegate
// (every restored parameter goes through it) walks the whole control tree. This layer
// closes both doors for the thread inside a host restore, and only for that thread.
//
// Base is the plug-in class (iplug::Plugin); DelegateBase is the graphics-free
// IEditorDelegate, whose helpers are what a plug-in without an editor would run:
// the parameter helper only notifies OnParamChangeUI, the message helper only calls
// OnMessage, and the control-value helper does nothing. The controls those calls
// skipped are re-derived by the UI resync the restore requests.
template <class Base, class DelegateBase>
class HostRestoreDelegate : public Base
{
public:
  using Base::Base;

  HostRestoreGate& RestoreGate() { return mRestoreGate; }

  // Hides the base GetUI() for every call made from inside the plug-in class.
  auto* GetUI() { return mRestoreGate.HidesUiFromThisThread() ? nullptr : Base::GetUI(); }

  // Raised instead of walking the controls when a restore's own OnRestoreState() runs
  // on the host's thread. The plug-in schedules its UI resync here.
  virtual void OnRestoreStateDeferred() {}

  // The default OnRestoreState() sends every parameter to the control tree. The API
  // wrappers call it on the host's restore thread right after UnserializeState(),
  // so that call is deferred; any other caller (a preset recall from the UI) runs it.
  void OnRestoreState() override
  {
    if (mRestoreGate.TakeRestoreTail())
      OnRestoreStateDeferred();
    else
      Base::OnRestoreState();
  }

  void SendControlValueFromDelegate(int ctrlTag, double normalizedValue) override
  {
    if (mRestoreGate.HidesUiFromThisThread())
      return;
    Base::SendControlValueFromDelegate(ctrlTag, normalizedValue);
  }

  void SendControlMsgFromDelegate(int ctrlTag, int msgTag, int dataSize = 0, const void* pData = nullptr) override
  {
    if (mRestoreGate.HidesUiFromThisThread())
      this->DelegateBase::SendControlMsgFromDelegate(ctrlTag, msgTag, dataSize, pData);
    else
      Base::SendControlMsgFromDelegate(ctrlTag, msgTag, dataSize, pData);
  }

  void SendParameterValueFromDelegate(int paramIdx, double value, bool normalized) override
  {
    if (mRestoreGate.HidesUiFromThisThread())
      this->DelegateBase::SendParameterValueFromDelegate(paramIdx, value, normalized);
    else
      Base::SendParameterValueFromDelegate(paramIdx, value, normalized);
  }

private:
  HostRestoreGate mRestoreGate;
};

} // namespace volum
