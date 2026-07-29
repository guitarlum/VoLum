# VoLum 1.2.1 audit — process lifetime / startup / threading / standalone audio path

**Scope read in full:** `VoLumLoader.inc.cpp`, `VoLumDiagLog.h`, `VoLumSettingsFileIO.h`, `VoLumLatencyReport.h`, the lifecycle members of `NeuralAmpModeler.{h,cpp}`, `IPlugAPP_host.{h,cpp}`, `IPlugAPP_dialog.cpp`, `IPlugAPP_main.cpp`, `IPlugAPP.cpp`, `IPlugTimer.{h,cpp}`, `IGraphicsEditorDelegate.cpp`, `IGraphics::AttachControl`/`GetControlWithTag`, `WDL/mutex.h`, and `iplug2-patches/`.

---

## PART 1 — The hang

### First, what the evidence actually pins down

Three facts in the code fix the failing thread's position precisely.

**(a) The main thread never returned from `OpenWindow`.** `ShowWindow` is the *last* statement of `WM_INITDIALOG`, after the UI is built:

```563:578:C:\dev\VoLum\iPlug2\IPlug\APP\IPlugAPP_dialog.cpp
    case WM_INITDIALOG:
    {
      gHWND = hwndDlg;
      IPlugAPP* pPlug = pAppHost->GetPlug();

      if (!pAppHost->OpenWindow(gHWND))
        DBGMSG("couldn't attach gui\n");

      width = pPlug->GetEditorWidth();
      height = pPlug->GetEditorHeight();

      ClientResize(hwndDlg, width, height);

      ShowWindow(hwndDlg, SW_SHOW);
      return 1;
    }
```

`IsWindowVisible()==FALSE` therefore means line 576 was never reached, and since `CreateDialog` (`IPlugAPP_main.cpp:77`) is called *before* the message loop (`:85`), `IsHungAppWindow()==TRUE` follows automatically — the loop never started. The failure is inside `pAppHost->OpenWindow(gHWND)`.

**(b) The "MAIN loaded" / "SUPPORT loaded" log lines were written by the ASIO callback thread, not the main thread.** `_VolumDrainLoaderResults()` has exactly one caller:

```1667:1669:C:\dev\VoLum\NeuralAmpModeler\NeuralAmpModeler.cpp
void NeuralAmpModeler::_ApplyDSPStaging()
{
  _VolumDrainLoaderResults();
```

and `_ApplyDSPStaging()` has exactly one caller, `ProcessBlock` at `NeuralAmpModeler.cpp:577`. The `VOLUM_LOG("model", "MAIN loaded " + result.path)` at `VoLumLoader.inc.cpp:185` is thus unreachable except from the audio thread. So at 19:59:24 the stream was live and callbacks were running.

**(c) The load requests were issued from `OnIdle`, which needs the main thread's message pump.** `_VolumQueueMainModelLoad` is called only from `NeuralAmpModeler.cpp:876` (inside `OnIdle`), and iPlug2's Windows timer is a *thread* timer:

```79:79:C:\dev\VoLum\iPlug2\IPlug\IPlugTimer.cpp
  ID = SetTimer(0, 0, intervalMs, TimerProc); //TODO: timer ID correct?
```

`WM_TIMER` with `hwnd == NULL` is only delivered by a `GetMessage`/`DispatchMessage` on that thread (`IPlugAPP_main.cpp:99-102`). Since the outer loop never ran, `OnIdle` fired from a **nested pump inside `OpenWindow`** (font/DirectX/COM initialisation or a driver-owned pump; a `MessageBox` is ruled out because a failed open would have logged a second `[audio] reset` line via `RestoreActiveAudioStateAfterFailure` → second `InitAudio`, and only one reset line exists).

Combining (a)–(c): **the main thread was inside `OpenWindow` building controls, and simultaneously the audio thread was inside `_ApplyDSPStaging`.** That is exactly the collision window of Finding 1.

---

### FINDING 1 — Audio thread mutates the IGraphics control tree while the main thread is building it; the settings page dereferences a child that does not exist yet

- **SEVERITY:** BLOCKER
- **WHERE:** `NeuralAmpModeler.cpp:1766` / `1772` / `1778` → `NeuralAmpModeler.cpp:2264` → `NeuralAmpModeler.cpp:2281-2285` → `NeuralAmpModelerControls.h:1215-1220` → `NeuralAmpModelerControls.h:741-751`
- **CONFIDENCE:** certain that the code path exists and is reachable exactly in the observed window; likely that it is the specific fault that killed this launch.

**MECHANISM.** Commit `68cb936` ("Fix the standalone latency readout") appended a UI write to `_UpdateLatency()`:

```2262:2265:C:\dev\VoLum\NeuralAmpModeler\NeuralAmpModeler.cpp
  // Force: our PDC just changed, and the editor may have been rebuilt with empty
  // labels, so an unchanged device-side report still has to be re-sent.
  _VolumRefreshLatencyReport(/*force=*/true);
}
```

`_UpdateLatency()` is called from `_ApplyDSPStaging()` — i.e. from `ProcessBlock`:

```1764:1780:C:\dev\VoLum\NeuralAmpModeler\NeuralAmpModeler.cpp
  if (removedMainModel || appliedMainModel)
  {
    _UpdateLatency();
    _SetInputGain();
    _SetOutputGain();
  }
  if (removedSupportModel || appliedSupportModel)
  {
    _UpdateLatency();
    _SetSupportOutputGain();
  }
```

So on the block where a model goes live — 19:59:24.211 for MAIN, 19:59:24.429 for SUPPORT — the **ASIO callback thread** executes:

```2281:2285:C:\dev\VoLum\NeuralAmpModeler\NeuralAmpModeler.cpp
  if (auto* pGraphics = GetUI())
  {
    if (auto* settings = pGraphics->GetControlWithTag(kCtrlTagSettingsBox))
      settings->As<NAMSettingsPageControl>()->SetCurrentLatency(report);
  }
```

`GetUI()` is already non-null at that moment, because iPlug2 assigns `mGraphics` *before* the layout runs:

```27:40:C:\dev\VoLum\iPlug2\IGraphics\IGraphicsEditorDelegate.cpp
void* IGEditorDelegate::OpenWindow(void* pParent)
{
  if(!mGraphics)
  {
    mGraphics = std::unique_ptr<IGraphics>(CreateGraphics());
...
  if(mGraphics)
    return mGraphics->OpenWindow(pParent);
```

And `AttachControl` publishes the tag **before** running `OnAttached()`, which is where the settings page creates its children:

```299:316:C:\dev\VoLum\iPlug2\IGraphics\IGraphics.cpp
IControl* IGraphics::AttachControl(IControl* pControl, int ctrlTag, const char* group)
{
  if(ctrlTag > kNoTag)
  {
    auto result = mCtrlTags.insert(std::make_pair(ctrlTag, pControl));
...
  mControls.Add(pControl);
    
  pControl->OnAttached();
  return pControl;
}
```

`NAMSettingsPageControl::OnAttached()` starts at `NeuralAmpModelerControls.h:1030` and only adds the `modelInfo` child at line 1197 — near the very end of a long function that attaches dozens of controls and loads SVGs. During that whole span the tag resolves but the child does not:

```1215:1220:C:\dev\VoLum\NeuralAmpModeler\NeuralAmpModelerControls.h
  void SetCurrentLatency(const volum::LatencyReport& report)
  {
    auto* modelInfoControl = static_cast<ModelInfoControl*>(GetNamedChild(mControlNames.modelInfo));
    assert(modelInfoControl != nullptr);
    modelInfoControl->SetCurrentLatency(report);
  }
```

`assert` is a no-op in Release, so this calls through a null `this` into:

```748:750:C:\dev\VoLum\NeuralAmpModeler\NeuralAmpModelerControls.h
    const volum::LatencyLines lines = volum::FormatLatencyLines(report, kStandalone);
    static_cast<IVLabelControl*>(GetNamedChild(mControlNames.currentLatency))->SetStr(lines.headline.c_str());
    static_cast<IVLabelControl*>(GetNamedChild(mControlNames.latencyDetail))->SetStr(lines.detail.c_str());
```

Two independent faults live here: the null-`this` dereference above, and — even once the children exist — an **unsynchronised `std::unordered_map` insert/find race**. `mCtrlTags` is a plain `std::unordered_map<int, IControl*>` (`IGraphics.h:1781`); the main thread is `insert`ing into it (rehash) while the audio thread `find`s in it. That is UB with a real chance of returning a stale or garbage `IControl*`, after which `SetCurrentLatency` writes through arbitrary pointers.

Note also that `GetControlWithTag`'s guard is a tautology and never fires, even in Debug:

```446:459:C:\dev\VoLum\iPlug2\IGraphics\IGraphics.cpp
IControl* IGraphics::GetControlWithTag(int ctrlTag) const
{
  const auto it = mCtrlTags.find(ctrlTag);
...
    assert("There is no control attached with this tag");
    return nullptr;
```

`assert("string literal")` is always true. The same missing-null-check pattern appears twice more on paths reachable from the same nested `OnIdle`: `NeuralAmpModeler.cpp:1005` (`->ClearModelInfo()`) and `NeuralAmpModeler.cpp:2203`/`2207` (`->SetModelInfo(...)`, `GetControlWithTag(kCtrlTagCalibrateInput)->SetDisabled(...)`), both unguarded.

**TRIGGER.** Launch the standalone with a device whose stream opens fast enough that the first audio callbacks land while the editor is still being constructed. On this machine that is the normal case: FlexASIO at 48 kHz started at ~19:59:23.2, the loader finished MAIN at ~19:59:24.2, and the UI build straddles that. A VST3 instance already live on the hardware in another process makes it *more* likely by slowing the standalone's own open/probe relative to the loader. Before 1.2.1 the audio thread never touched IGraphics, which is why this is new.

**IMPACT.** The dialog is created but never shown, the message loop never starts, and the process becomes an unkillable-looking zombie (see Finding 2 for why it survives instead of crashing visibly, and why the *next* launch silently does nothing).

**FIX SKETCH.** Do not touch the UI from `_ApplyDSPStaging`. Split `_UpdateLatency()` into an audio-safe part (compute PDC, `SetLatency`) and a UI part, and drop the `_VolumRefreshLatencyReport` call from it — `OnIdle` already polls it every idle tick (`NeuralAmpModeler.cpp:793`), so the readout stays correct with no loss of behaviour. Additionally add real null checks in `SetCurrentLatency`/`SetModelInfo`/`ClearModelInfo` instead of `assert`. **No audio DSP touched; the audio voicing is unaffected.**

**WOULD A TEST HAVE CAUGHT IT? No — the suite actively pins the defect.** `test_volum_ui_regressions.cpp` is a source-text grep harness, and it *requires* the offending call to be present:

```943:945:C:\dev\VoLum\NeuralAmpModeler\tests\test_volum_ui_regressions.cpp
  RequireContains(source, "void NeuralAmpModeler::_VolumRefreshLatencyReport(bool force)");
  RequireContains(source, "_VolumRefreshLatencyReport();");
  RequireContains(source, "_VolumRefreshLatencyReport(/*force=*/true);");
```

`test_volum_latency_report.cpp` only tests the pure string formatting in `VoLumLatencyReport.h`. There is no test that instantiates a plugin, no test that runs `ProcessBlock` concurrently with UI construction, and no thread-sanitiser build on the Windows path.

---

### FINDING 2 — `WinMain`'s exception path leaks the single-instance mutex and never destroys the window, converting any startup fault into a permanent zombie that blocks all future launches

- **SEVERITY:** BLOCKER
- **WHERE:** `C:\dev\VoLum\iPlug2\IPlug\APP\IPlugAPP_main.cpp:36-143`
- **CONFIDENCE:** certain (this is what makes the reported symptom *permanent* rather than a crash dialog)

**MECHANISM.** The cleanup that matters is inside the `try`, after the message loop:

```128:142:C:\dev\VoLum\iPlug2\IPlug\APP\IPlugAPP_main.cpp
    }
    
    // in case gHWND didnt get destroyed -- this corresponds to SWELLAPP_DESTROY roughly
    if (gHWND)
      DestroyWindow(gHWND);
    
#ifndef APP_ALLOW_MULTIPLE_INSTANCES
    ReleaseMutex(hMutex);
#endif
  }
  catch(std::exception e)
  {
    DBGMSG("Exception: %s", e.what());
    return 1;
  }
```

Any exception escaping `CreateDialog`'s `WM_INITDIALOG` (and `std::runtime_error` is thrown liberally in this codebase — e.g. `NeuralAmpModeler.cpp:1656`, `1661`; `RtAudioError` also derives from `std::runtime_error`) skips **both** `DestroyWindow(gHWND)` and `ReleaseMutex(hMutex)`. Result, matching the evidence item for item: a top-level `#32770` titled "VoLum" that still exists, was never shown, and whose thread never pumped (`IsHungAppWindow()==TRUE`).

The process then does not exit, because teardown blocks. `IPlugAPPHost::sInstance` is a namespace-scope static (`IPlugAPP_host.cpp:29`), so it is destroyed during CRT exit, and `~IPlugAPPHost` (`:37-48`) calls `CloseAudio()`:

```621:651:C:\dev\VoLum\iPlug2\IPlug\APP\IPlugAPP_host.cpp
void IPlugAPPHost::CloseAudio()
{
  if (mDAC && mDAC->isStreamOpen())
  {
    if (mDAC->isStreamRunning())
    {
      mAudioEnding = true;
    
      int waitCount = 0;
      while (!mAudioDone && waitCount < 200)
      {
        Sleep(10);
        ++waitCount;
      }
...
      try
      {
        mDAC->abortStream();
...
    mDAC->closeStream();
```

`mAudioDone` is only ever set by the audio callback (`IPlugAPP_host.cpp:934`). If that callback thread is wedged or was unwound by the fault, the 2 s bounded wait expires and we go straight into `abortStream()` (`ASIOStop`) and `closeStream()` (`ASIODisposeBuffers`/`ASIOExit`) against a driver whose callback thread never returned — those calls are not bounded and are the classic place an ASIO teardown parks forever. Six threads, all `Wait`, zero CPU delta, ~30 s of CPU burned earlier: consistent.

And then the killer for the user: the next launch hits

```39:48:C:\dev\VoLum\iPlug2\IPlug\APP\IPlugAPP_main.cpp
    HANDLE hMutex = OpenMutex(MUTEX_ALL_ACCESS, 0, BUNDLE_NAME);
    
    if (!hMutex)
      hMutex = CreateMutex(0, 0, BUNDLE_NAME);
    else
    {
      HWND hWnd = FindWindow(0, BUNDLE_NAME);
      SetForegroundWindow(hWnd);
      return 0;
    }
```

The mutex still exists, `FindWindow` returns the invisible hung window, `SetForegroundWindow` on a hung window does nothing, and `WinMain` returns 0 with **no window, no error, no message**. This is exactly "the user could not start VoLum until they manually killed the dead background process."

**TRIGGER.** Any exception or fatal fault during standalone startup, once. It is self-perpetuating from then on.

**IMPACT.** VoLum becomes unusable until the user opens Task Manager. Most users will not know to do that; they will report "VoLum won't open any more".

**FIX SKETCH.** Two small, independent changes, both in the iPlug2 mirror:
1. Make the mutex and window cleanup exception-safe — RAII handle for `hMutex`, and destroy `gHWND` in the catch block (or restructure so cleanup is in a scope guard).
2. In the single-instance branch, verify liveness before bailing: if `FindWindow` returns null, or `IsHungAppWindow(hWnd)`, or `SetForegroundWindow` fails, either proceed to launch or tell the user which process to close. Silently `return 0` is the worst option.

Neither touches DSP.

**WOULD A TEST HAVE CAUGHT IT?** No. `e2e-standalone-win.ps1` and the CI standalone smoke exercise the happy launch. Nothing injects a startup fault, nothing asserts the mutex is released on an abnormal exit, and nothing checks for a leftover invisible window.

---

### FINDING 3 — `_VolumDrainLoaderResults` performs file I/O, two blocking mutex acquisitions, multi-megabyte deallocation and (conditionally) a full model `Reset()` on the audio thread; the new diagnostic log is written from `ProcessBlock` in direct violation of its own documented contract

- **SEVERITY:** BLOCKER
- **WHERE:** `VoLumLoader.inc.cpp:135-242`, reached from `NeuralAmpModeler.cpp:577`
- **CONFIDENCE:** certain

**MECHANISM.** `VoLumDiagLog.h` states its own rule unambiguously:

```13:16:C:\dev\VoLum\NeuralAmpModeler\VoLumDiagLog.h
//   - NEVER call it from the audio thread. Every entry opens a file. The audio
//     thread must stay lock-free and allocation-free, so anything it wants logged
//     has to be latched and emitted from the main thread.
```

and again at the macro:

```161:163:C:\dev\VoLum\NeuralAmpModeler\VoLumDiagLog.h
// Main-thread only. The name is deliberately loud: if you find yourself reaching for
// this inside ProcessBlock, latch the fact and log it from OnIdle instead.
#define VOLUM_LOG(category, message) ::volum::diag::Write(category, message)
```

The drain breaks it five times: `VoLumLoader.inc.cpp:175`, `185`, `203`, `212`, `232`. Each call takes a process-wide `std::mutex` and then does `std::filesystem::file_size`, possibly `remove` + `rename`, and an `std::ofstream` append (`VoLumDiagLog.h:118-145`) — inside the ASIO buffer switch. The user's own log is therefore proof of the violation: `[model] MAIN loaded …` at 19:59:24.211 was written by the audio thread.

The same function also does, on the audio thread:

- **Blocking lock acquisition** on `mVolumLoaderMutex` at lines 158, 192, 222 — note the entry at line 139 correctly uses `std::try_to_lock` and bails, and then the body abandons that discipline. These contend with the loader thread (`:380`) and with the main thread's queue calls (`:51`, `:74`, `:101`, `:124`).
- **Blocking lock acquisition** on `mStagingMutex` at lines 181, 209, 238.
- **A full model reset**, when the request's captured rate/block no longer match:

```147:152:C:\dev\VoLum\NeuralAmpModeler\VoLumLoader.inc.cpp
    if (result.model != nullptr && (result.sampleRate != GetSampleRate() || result.blockSize != GetBlockSize()))
    {
      result.model->Reset(GetSampleRate(), GetBlockSize());
      result.sampleRate = GetSampleRate();
      result.blockSize = GetBlockSize();
    }
```

`request.sampleRate`/`blockSize` are latched at *queue* time (`:47-48`, `:97-98`, `:120-121`). On standalone startup the constructor sets `mVolumNeedsLoad` (`NeuralAmpModeler.cpp:521`) and `OnIdle` can run before `InitAudio` calls `SetSampleRate`/`SetBlockSize` (`IPlugAPP_host.cpp:755-757`), in which case the request carries the 44.1 kHz default and the very first ASIO callback performs a full 48 kHz `ResamplingNAM::Reset` — allocation and prewarm — inside the buffer switch. Also note `APP_N_VECTOR_WAIT` is `0` (`config.h:54`), so processing starts on callback #1 with no warm-up margin.

Finally, `mModel = std::move(mStagedModel)` at `NeuralAmpModeler.cpp:1729` frees the previous model — tens of MB of heap, under `mStagingMutex`, on the audio thread.

**TRIGGER.** Every model, IR, PRE-capture or SUPPORT load, i.e. every amp/channel/cab change and every startup. The `Reset` variant triggers on any startup where the device rate differs from 44.1 kHz.

**IMPACT.** Audible dropouts and glitches on every rig change; a multi-hundred-millisecond stall on the first block after startup; and an unbounded priority-inversion window where a background thread's file I/O or a main-thread queue push can stall the audio callback. It is also the ingredient that turns Finding 1 from "a race that usually wins" into one that reliably lands inside the UI-build window, because it stretches the audio thread's stay inside `_ApplyDSPStaging` from microseconds to milliseconds.

**FIX SKETCH.** Move the drain to `OnIdle` (main thread) and leave `_ApplyDSPStaging` doing only the lock-free pointer swaps it was designed for. Latch load outcomes into atomics/an SPSC queue and emit the log lines from `OnIdle`, exactly as `VoLumDiagLog.h` instructs. Move the rate/block re-`Reset` to the drain's new main-thread home. **No change to the DSP graph or to any coefficient — the audio output is bit-identical; only *when* the swap is published changes.**

**WOULD A TEST HAVE CAUGHT IT?** No. `test_volum_diag_log.cpp` is entirely single-threaded (writes, checks rotation, checks it stays silent when disabled). `test_volum_dsp_staging.cpp` tests the pure path-pair helpers. `test_volum_ui_regressions.cpp:842-849` pins the *presence* of `mStagingMutex` and `_VolumDrainLoaderResults();` by text match, which is a spelling check, not a thread-safety check. Nothing asserts "no file I/O, no locks, no allocation on the audio thread" — that would need either a doctest harness that runs `ProcessBlock` under an allocation/syscall hook, or the macOS sanitiser job extended to a threaded scenario.

---

### FINDING 4 — `ENTER_PARAMS_MUTEX` / `LEAVE_PARAMS_MUTEX` are not RAII: any exception or SEH fault escaping `ProcessBlock` abandons a critical section for the life of the process

- **SEVERITY:** BLOCKER (as the amplifier that turns Findings 1/3 into a hard deadlock rather than a recoverable glitch)
- **WHERE:** `C:\dev\VoLum\iPlug2\IPlug\APP\IPlugAPP.cpp:164-166`, macro at `C:\dev\VoLum\iPlug2\IPlug\IPlugPlatform.h:48-49`
- **CONFIDENCE:** certain about the defect; likely about its role here

**MECHANISM.**

```164:166:C:\dev\VoLum\iPlug2\IPlug\APP\IPlugAPP.cpp
  ENTER_PARAMS_MUTEX
  ProcessBuffers(0.0, GetBlockSize());
  LEAVE_PARAMS_MUTEX
```

The macros are bare calls, not a scoped lock:

```48:49:C:\dev\VoLum\iPlug2\IPlug\IPlugPlatform.h
  #define ENTER_PARAMS_MUTEX mParams_mutex.Enter(); Trace(TRACELOC, "%s", "ENTER_PARAMS_MUTEX");
  #define LEAVE_PARAMS_MUTEX mParams_mutex.Leave(); Trace(TRACELOC, "%s", "LEAVE_PARAMS_MUTEX");
```

`WDL_Mutex` is a `CRITICAL_SECTION` on Windows (`WDL/mutex.h:63-64`). If anything unwinds out of `ProcessBuffers` — a C++ exception, or an access violation swallowed by an SEH filter inside the ASIO driver's `bufferSwitch` — `Leave()` never runs and the critical section stays owned by a thread that no longer holds it. Being a `CRITICAL_SECTION` it is recursive, so the *audio* thread keeps working; but any other thread that later enters it (`IPluginBase::UnserializeParams`, `IPlugPluginBase.cpp:123`; preset/FXP paths at `:891`, `:1025`) blocks forever with zero CPU. That is precisely the "all threads Wait, no crash, no exit" signature.

The same non-RAII hazard exists for the `std::lock_guard`s in `_ApplyDSPStaging` and `_VolumDrainLoaderResults` under an SEH fault (a structured exception does not run C++ destructors), which would leave `mStagingMutex` permanently locked and hang the *main* thread the next time `_StageIR`/`_StageModel`/`_ResetModelAndIR` runs.

**FIX SKETCH.** Replace the macro pair with a `WDL_MutexLock` (already in `WDL/mutex.h:141-148`) in `IPlugAPP::AppProcess`, and wrap `ProcessBuffers` in a `try/catch(...)` at the APP boundary so a throwing `ProcessBlock` degrades to a silent block rather than corrupting host state. Mirror-branch change; no DSP impact.

**WOULD A TEST HAVE CAUGHT IT?** No — there is no test that provokes an exception from `ProcessBlock` and then asserts the plugin is still usable.

---

### Hypotheses explicitly refuted

**H1 — mismatched ASIO `indev`/`outdev` causes a blocking/deadlocking open: REFUTED as the hang cause, but it *is* a real bug (see Finding 6).** On Windows ASIO the host ignores `indev` entirely and uses `outdev` for both directions:

```466:476:C:\dev\VoLum\iPlug2\IPlug\APP\IPlugAPP_host.cpp
#if defined OS_WIN
  if(mState.mAudioDriverType == kDeviceASIO)
    inputID = GetAudioDeviceIdx(mState.mAudioOutDev.Get());
  else
    inputID = GetAudioDeviceIdx(mState.mAudioInDev.Get());
```

So the stream was FlexASIO duplex, `inId == outId`, and `InitAudio` even special-cases that to avoid a double `getDeviceInfo` (`:684-692`). The multiple ASIO DLLs in the process are simply `ProbeAudioIO()` (`:264-332`) enumerating and initialising every registered driver — normal, and it completed (device names were resolved).

**H2 — a second process owning the ASIO device makes the open block forever: REFUTED.** The log proves the opposite: `[audio] reset` was followed by audio callbacks that drained and logged two model loads. A blocked `openStream` would have produced no further log lines at all, and a *failed* open would have produced a second `[audio] reset` from `RestoreActiveAudioStateAfterFailure` → `InitAudio` (`:394-422`). Only one reset line exists.

**H3 — a lock cycle between `mStagingMutex` / `mVolumLoaderMutex` / `mPrePitchMutex`: REFUTED as a cycle.** I traced every acquisition. `mStagingMutex` and `mVolumLoaderMutex` are never nested (the scopes in `_VolumDrainLoaderResults` at 157-163 / 181-184, 191-195 / 209-211, 221-225 / 238-240 are strictly sequential), `mPrePitchMutex` is `try_to_lock` on the audio side (`VoLumProcessBlock.inc.cpp:28`), and the loader thread releases `mVolumLoaderMutex` before doing any file work (`VoLumLoader.inc.cpp:292`). There is no lock-order inversion. What *is* wrong is that the audio thread blocks on these locks at all (Finding 3) and that they can be abandoned by an SEH unwind (Finding 4).

**H4 — the new latency readout queries the stream before it is safe: PARTIALLY CONFIRMED, but not via the stream query.** `GetStreamLatencyFrames()` guards on `isStreamOpen()` (`IPlugAPP_host.h:241-247`) and RtAudio's ASIO latency is cached at open, so the query itself is benign. The 1.2.1 latency code *is* the culprit, but through the **UI write** it added (Finding 1), not the device read. Secondary defect: `mVolumLastLatencyReport` (`NeuralAmpModeler.cpp:2279`) is a non-atomic struct written from both the audio thread (via `_UpdateLatency`) and the main thread (via `OnIdle`) — a data race.

**H5 — a modal dialog shown from a non-UI thread or before the window is visible: NOT the hang, but a real latent bug.** See Finding 8: `RestoreActiveAudioStateAfterFailure` calls `MessageBox(gHWND, …)` with `gHWND == NULL`. It did not fire on this launch (single `[audio] reset` line), and a modal box pumps messages, so it cannot produce `IsHungAppWindow()==TRUE`.

**H6 — reentrancy through `OnReset`/`OnActivate` during device init: REFUTED.** `OnReset` is called exactly once per `InitAudio`, at `IPlugAPP_host.cpp:757`, from the main thread before `openStream`; `OnActivate(true)` once at `:77`. `NeuralAmpModeler` does not override `OnActivate`. No reentrancy path exists.

---

## PART 2 — General audit of the subsystem

### FINDING 5 — `~NeuralAmpModeler` can throw: `json.dump()` on non-UTF-8 content terminates the process on exit

- **SEVERITY:** MAJOR
- **WHERE:** `NeuralAmpModeler.cpp:540-556` (esp. `:545`) → `VoLumSettingsScene.inc.cpp:334-377` → `VoLumSettingsFileIO.h:98`
- **CONFIDENCE:** likely

**MECHANISM.** The destructor writes settings:

```540:546:C:\dev\VoLum\NeuralAmpModeler\NeuralAmpModeler.cpp
NeuralAmpModeler::~NeuralAmpModeler()
{
  _VolumStopLoader();
  _VolumSaveCurrentToSettings();
#ifdef APP_API
  _VolumSaveSettingsToFile();
```

which reaches `WriteJsonAtomically`:

```98:98:C:\dev\VoLum\NeuralAmpModeler\VoLumSettingsFileIO.h
    out << json.dump(2);
```

`nlohmann::json::dump` throws `type_error.316` when any string holds invalid UTF-8. The settings JSON carries user-supplied names and file paths (custom amps, IRs, imported NAM filenames), and this repo has a commit specifically titled "Fix remaining UTF-8 runtime paths" — so mixed-encoding path bytes are a live concern here, not a theoretical one. There is no `try/catch` in `WriteJsonAtomically`, none in `_VolumSaveSettingsToFile` (it only inspects `std::error_code`), and none in the destructor. An exception leaving a destructor during stack unwinding or `atexit` is `std::terminate`.

The same throw can also escape `SerializeState` via `volum::content::GlobalContentStore().Save()` (`NeuralAmpModeler.cpp:1032`) and `PutChunkIdTail`'s `IdTailToJson(t).dump()` (`VoLumChunkIdTail.h:415`) — in a DAW that is a host crash during project save.

**FIX SKETCH.** Wrap the `dump()` in `WriteJsonAtomically` in `try/catch`, returning `std::errc::illegal_byte_sequence`; and wrap the destructor's save block in `try { … } catch (...) {}`. Optionally use `json::dump(2, ' ', false, error_handler_t::replace)` so a bad byte degrades to U+FFFD instead of losing the whole file. No DSP impact.

**WOULD A TEST HAVE CAUGHT IT?** No. `test_volum_settings_atomic_write.cpp` covers the golden write, an invalid target directory, and a two-writer race — all with clean ASCII payloads. No case feeds invalid UTF-8, and none asserts the destructor is noexcept in practice.

---

### FINDING 6 — A mismatched ASIO `indev`/`outdev` in `settings.ini` is silently ignored rather than repaired, so the file keeps lying and the Preferences dialog keeps showing the wrong input device

- **SEVERITY:** MAJOR
- **WHERE:** `IPlugAPP_host.cpp:466-476`, `:410-415`, `IPlugAPP_dialog.cpp:155-160`
- **CONFIDENCE:** certain

**MECHANISM.** `TryToChangeAudio` resolves the input from `mAudioOutDev` when the driver is ASIO, but never writes that correction back — `mState.mAudioInDev` keeps the value `ASIO4ALL v2` forever, and `UpdateINI()` (`:172-208`) faithfully re-persists it. The dialog papers over it visually by selecting the *output* index in the input combo:

```155:160:C:\dev\VoLum\iPlug2\IPlug\APP\IPlugAPP_dialog.cpp
#ifdef OS_WIN
  if(driverType == kDeviceASIO)
    SendDlgItemMessage(hwndDlg,IDC_COMBO_AUDIO_IN_DEV,CB_SETCURSEL, outdevidx, 0);
  else
#endif
```

so the UI looks right while the file stays wrong. The changelog entry for 05/20/2026 claims "Older settings with mismatched input/output devices migrate silently to the previous output device when available" — the *runtime* honours that, but the *migration* (rewriting the ini) never happens. Any code path that reads `mAudioInDev` directly is therefore wrong: `AudioSettingsInStateAreEqual` (`:372`) and `AppState::operator==` (`:158`) compare a field that no longer describes reality, so an ASIO settings change can compare "equal" and skip a needed reopen, or compare "unequal" and force a spurious one.

**FIX SKETCH.** In `TryToChangeAudio`, when `mAudioDriverType == kDeviceASIO`, set `mState.mAudioInDev` to `mState.mAudioOutDev` and call `UpdateINI()`. Two lines, no DSP impact.

**WOULD A TEST HAVE CAUGHT IT?** The Windows standalone smoke test asserts a non-empty input dropdown (changelog 05/27/2026) — which passes, because the dropdown is populated from the output device. Nothing asserts on the ini contents after an ASIO launch.

---

### FINDING 7 — `mAudioEnding` / `mAudioDone` / `mExiting` are non-atomic `bool`s shared across threads, and `CloseAudio`'s bounded wait falls through into an unbounded `abortStream()`

- **SEVERITY:** MAJOR
- **WHERE:** `IPlugAPP_host.h:269-271`; `IPlugAPP_host.cpp:621-651`, `:627`, `:872`, `:933-934`, `:951`
- **CONFIDENCE:** certain

**MECHANISM.**

```269:271:C:\dev\VoLum\iPlug2\IPlug\APP\IPlugAPP_host.h
  bool mExiting = false;
  bool mAudioEnding = false;
  bool mAudioDone = false;
```

`mAudioEnding` is written by the main thread (`:627`) and read by the audio callback (`:872`, `:933`); `mAudioDone` is written by the audio callback (`:934`) and spun on by the main thread (`:630`); `mExiting` is written by the main thread (`:39`) and read on the RtMidi callback thread (`:951`). All three are plain `bool` with no atomics and no fences — a data race by the letter of the standard, and on the fade-out handshake specifically it means the main thread may never observe the flag it is waiting for.

Worse, the timeout is not actually a safety net. After 2 s it logs and then does the dangerous thing anyway:

```636:646:C:\dev\VoLum\iPlug2\IPlug\APP\IPlugAPP_host.cpp
      if (!mAudioDone)
        DBGMSG("VoLum: CloseAudio timed out waiting for callback fade; aborting stream\n");
      
      try
      {
        mDAC->abortStream();
```

If the callback thread is wedged (Findings 3/4), `abortStream()`/`closeStream()` into a live ASIO driver is exactly where teardown parks. This is the most likely proximate reason the observed process was still alive with zero CPU.

**FIX SKETCH.** Make the three flags `std::atomic<bool>`. That is the correctness fix. The unbounded `abortStream` is harder to make safe without an ASIO-level timeout; at minimum log to the diag log (not `DBGMSG`, which vanishes in Release) so a future hang is visible in `volum.log`. No DSP impact.

**WOULD A TEST HAVE CAUGHT IT?** No. There is no test of the shutdown handshake at all; `test_iplug_app_vector_accumulator.cpp` covers only the buffer-carry arithmetic.

---

### FINDING 8 — `TryToChangeAudio()` runs before any window exists, so a device failure shows an ownerless modal box and pumps `OnIdle` before the UI is constructed

- **SEVERITY:** MAJOR
- **WHERE:** `IPlugAPP_main.cpp:55-57` vs `:77`; `IPlugAPP_host.cpp:394-397`
- **CONFIDENCE:** certain

**MECHANISM.** The startup order is `Create()` → `Init()` → `TryToChangeAudio()` → *then* `CreateDialog`. On failure:

```394:397:C:\dev\VoLum\iPlug2\IPlug\APP\IPlugAPP_host.cpp
bool IPlugAPPHost::RestoreActiveAudioStateAfterFailure(const char* message)
{
  if (message && message[0])
    MessageBox(gHWND, message, "Audio Error", MB_OK);
```

`gHWND` is still `NULL` (it is only assigned at `IPlugAPP_dialog.cpp:565`), so the "Audio Error" box has no owner — it can appear behind other windows with no taskbar anchor, and the user sees a VoLum that started and did nothing. More consequentially, `MessageBox` runs a modal message loop, which dispatches the `WM_TIMER` from `SetTimer(0, 0, …)` and therefore runs `IPlugAPIBase::OnTimer` → `NeuralAmpModeler::OnIdle` **before the editor exists** — queuing model loads, writing `volum-settings.json` (`NeuralAmpModeler.cpp:935-939`), and touching controls through paths that assume a built UI (`:1005`, `:2203`, `:2207`, all unguarded).

**FIX SKETCH.** Move `TryToChangeAudio()` into `WM_INITDIALOG` after `ShowWindow`, or defer the message box via `PostMessage` until `gHWND` is valid. Guard `OnIdle`'s UI writes with real null checks regardless.

**WOULD A TEST HAVE CAUGHT IT?** No — the smoke tests only launch with a working device.

---

### FINDING 9 — Device names longer than 99 characters are silently truncated on read, so the device is never found again

- **SEVERITY:** MINOR
- **WHERE:** `IPlugAPP_host.cpp:27` (`#define STRBUFSZ 100`), `:110`, `:118-119`
- **MECHANISM.** `GetPrivateProfileString(... buf, STRBUFSZ, ...)` truncates into a 100-byte buffer, and `GetAudioDeviceIdx` uses `strcmp` (`:215-224`). Long ASIO/WASAPI names (multi-word interface names with a channel-mode suffix are routinely >100 chars) fail to match, so `TryToChangeAudio` silently resets to the default device on every launch and the user's selection never sticks. **Fix:** size the buffer from `MAX_PATH_LEN` or read into a `WDL_String`. No DSP impact. No test covers long device names.

### FINDING 10 — The diag log's `block` field reports the plugin's internal vector size, never the device buffer, which actively misleads diagnosis

- **SEVERITY:** MINOR
- **WHERE:** `NeuralAmpModeler.cpp:781-784`; `config.h:57` (`APP_SIGNAL_VECTOR_SIZE 64`); `IPlugAPP_host.cpp:755`
- **MECHANISM.** `OnReset` logs `GetBlockSize()`, which `InitAudio` has just pinned to the constant 64. The user's requested 128 was never renegotiated — the log simply never mentions the device buffer, and the mismatch reads as a driver override that did not happen. This cost real diagnostic time on this very bug report. **Fix:** log `IPlugAPPHost::GetIOBufferSize()` alongside, and add a `[audio] stream opened: <device>, buffer N, driver latency M` line after `startStream()`. Also log the selected driver/device names and the resolved channel offsets — none of that is currently recorded, which is why "which device did it actually open?" had to be inferred from loaded DLLs. No DSP impact.

### FINDING 11 — `MainDlgProc` dereferences `pAppHost` after `WM_DESTROY` has nulled `sInstance`

- **SEVERITY:** MINOR
- **WHERE:** `IPlugAPP_dialog.cpp:556`, `:582`, `:788-790`
- **MECHANISM.** `WM_DESTROY` sets `IPlugAPPHost::sInstance = nullptr`. Any subsequent message re-enters the proc with `pAppHost == nullptr`; `WM_GETMINMAXINFO` checks for that (`:726-727`), `WM_SIZE` does not (`:790`), nor do the `WM_COMMAND` cases. A resize or command arriving during teardown is a null dereference. **Fix:** early-out on `!pAppHost` at the top of the proc. Also note that destroying `sInstance` *inside* `WM_DESTROY` runs `_VolumStopLoader()`'s `join()` and two JSON file writes on the UI thread while the window is being torn down — the app appears to hang on close for as long as an in-flight `nam::get_dsp` takes.

### FINDING 12 — `_VolumStopLoader` cannot cancel an in-flight model load

- **SEVERITY:** MINOR
- **WHERE:** `VoLumLoader.inc.cpp:20-35`; loader body `:301-361`
- **MECHANISM.** `_VolumStopLoader` clears the queue and notifies, but the worker only observes `mVolumLoaderStop` between requests (`:287`) and inside the prefetch directory scan (`:322`, `:344`). A single `nam::get_dsp` on a large `.nam` is not interruptible, so `join()` blocks for its full duration. Bounded by one model load, but on a slow disk that is a visible close delay. **Fix:** acceptable as-is; if it matters, detach on shutdown with a shared-state guard rather than joining.

### FINDING 13 — Static destruction order between `volum::diag::Log` and `IPlugAPPHost::sInstance` is unspecified

- **SEVERITY:** MINOR
- **WHERE:** `VoLumDiagLog.h:77-81`; `IPlugAPP_host.cpp:29`
- **MECHANISM.** `Log::Instance()`'s function-local static is constructed during the plugin constructor, i.e. *after* the namespace-scope `sInstance` object, so it is destroyed *before* it. On the abnormal-exit path (Finding 2) `~IPlugAPPHost` runs at CRT exit; if any audio callback is still in flight and reaches `VOLUM_LOG` (which it can, per Finding 3), it touches a destroyed mutex and `std::filesystem::path`. `Log::Close()` is defined (`:98-103`) but never called anywhere. **Fix:** call `Log::Close()` at the start of `~NeuralAmpModeler`, and/or leak the singleton deliberately (`new`, never deleted) so it outlives every consumer.

### NITs (one line each)

- `IGraphics::GetControlWithTag`'s guard is `assert("string literal")` (`IGraphics.cpp:456`) — always true, so it never fires even in Debug; the same tautology pattern would hide any future missing-tag bug.
- `IPlugAPPHost::ErrorCallback` (`IPlugAPP_host.cpp:979-982`) is an empty `//TODO:` and is not even passed to `openStream` (commented out at `:761`), so RtAudio errors during a running stream are discarded entirely instead of reaching `volum.log`.
- `GetAudioDeviceName` uses `mAudioIDDevNames.at(idx)` (`:210-213`) and is called with unvalidated indices from `DBGMSG` at `:741-742`; an out-of-range index throws `std::out_of_range` from inside `InitAudio`.
- `UpdateINI` writes `mAudioInChanL` into both `in1` and `in2` (`:183-186`) while `InitState` reads only `in1` — `in2` is dead weight in the file and will confuse the next person to read it.
- `ProbeMidiIO` (`:334-367`) appends to `mMidiInputDevNames`/`mMidiOutputDevNames` without clearing them first, so a second call duplicates every port (harmless today because it is called once, fragile if device re-probing is ever added).
- `MakeAtomicJsonTempPath` (`VoLumSettingsFileIO.h:32-43`) keys on `steady_clock` ticks plus a thread-id hash; two processes can collide, and stale `.tmp.*` files are never swept from `%LOCALAPPDATA%\VoLum`.
- `_VolumRefreshLatencyReport`'s `mVolumLastLatencyReport` (`NeuralAmpModeler.cpp:2279`) is written from two threads without synchronisation — subsumed by Finding 1's fix, but worth naming as its own race.

---

## Suggested fix order for 1.2.1

1. **Finding 1** — remove `_VolumRefreshLatencyReport` from `_UpdateLatency`, add real null checks. This is the shipping blocker and the smallest change on the list.
2. **Finding 2** — exception-safe `hMutex`/`gHWND` cleanup plus a liveness check in the single-instance branch. Without this, *any* future startup fault reproduces the same un-startable state.
3. **Finding 3** — move the loader drain and its logging to `OnIdle`.
4. **Findings 4, 5, 7** — RAII for the params mutex, non-throwing destructor, atomic shutdown flags.
5. **Finding 10** — log the device, driver and real I/O buffer, so the next report diagnoses itself.

Nothing in the above alters a filter coefficient, a gain, a block boundary or the order of DSP stages. Findings 1 and 3 change *when* a staged model is published to the audio thread (from "inside the callback that also does file I/O" to "one idle tick earlier, published lock-free"), which is a timing change only; the rendered audio is unchanged. **The frozen 1.2.1 voicing is safe.**