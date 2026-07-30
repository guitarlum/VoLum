# Self-review: custom-content library on disk + standalone app host

Range: `ebb0215..HEAD` on `release/1.2.1`, plus `iPlug2` submodule commits
`3492e6e90`, `2352c01d3` and the working-tree patch to `IPlug/APP/IPlugAPP_host.h`.
Scope: `VoLumContentStore.h`, `VoLumCustomContentApi.h`, `VoLumCustomNamImport.h`,
`VoLumSettingsFileIO.h`, the overlay import/delete paths that drive them, and the
iPlug2 APP host (audio/MIDI lifecycle, shutdown, Preferences).

Formatting-only commit `dba9ed0` ignored.

## Verdict

Not safe to ship as-is, but the blocking set is small and localized. The
deferred-delete rewrite is the problem: `ContentStore::Load()` resets the registry
and the `mRegistryUnreadable` flag but does **not** clear `mPendingFileDeletes`, so
a delete whose `Save()` failed stays queued across a registry reload and is then
flushed by the next *successful* save — against a registry that has meanwhile been
re-read from disk and still names the file. That is the exact "registry points at a
deleted payload" state the deferral was written to prevent, it is data loss of the
user's own `.wav`/`.nam`, and it is reachable in any DAW where a second plugin
instance is created (each instance's constructor calls `Load()` on the *process-
global* store). One line in `Load()` closes it; a second, cheap belt (skip a queued
path that the current registry still references) makes the whole class unreachable.
The other must-fix is the other half of the same design: `Save()` now returns a
meaningful `false`, and exactly three of ~a dozen mutators check it. Every rename,
delete, IR-shaping edit, preset save/overwrite/delete and pedal import still
reports success in the UI while writing nothing, and because `mRegistryUnreadable`
is latched for the process lifetime and never re-tested, a user whose library was
locked at startup (antivirus, OneDrive, another VoLum) loses a whole session of
library work with no message anywhere — and the test that appears to pin recovery
actually constructs a fresh store, so it passes with the stuck flag present. The
iPlug2 side is in better shape: `CloseAudio()`'s new policy is sound and the
watchdog cannot eat the settings write (`OnUIClose` flushes before the arm point),
but the watchdog's 5 s budget covers considerably more than its own commit message
claims, the ASIO `mAudioInDev` writeback destroys the user's saved DirectSound
input device, and the MIDI "fall back to off and persist it" turns a transient
boot-time port conflict into permanent config loss. `WriteJsonAtomically`,
`IsSafeStoredRelPath`, the `AddIR`/`AddCustomAmp`/`UpdateCustomAmp` rollbacks and
the `mDAC` null-guards are correct and complete.

## Findings

---

### F1 — A queued payload delete survives `Load()` and is later flushed against a registry that still references it

**Severity:** High (silent data loss: the user's imported `.wav` / `.nam` is deleted while the library still lists it)
**Confidence:** CONFIRMED (traced end to end; requires one failed `Save()` followed by a registry reload, both ordinary)
**Introduced by:** `79e57ca` — *Make the custom content library survive its own failure modes*, hunk adding `QueueStoredFileDelete` / `FlushPendingFileDeletes` / `mPendingFileDeletes` to `VoLumContentStore.h`

**Evidence**

The queue is a plain member with exactly two mutators: push on removal, clear on flush.

```981:1002:NeuralAmpModeler/VoLumContentStore.h
private:
  // Queue a payload the committed registry still references. It is deleted by the
  // next successful Save(), never before: see the comment there.
  void QueueStoredFileDelete(const std::string& relPath)
  {
    if (relPath.empty())
      return;
    mPendingFileDeletes.push_back(relPath);
  }

  void FlushPendingFileDeletes()
  {
    for (const auto& relPath : mPendingFileDeletes)
    {
      const auto resolved = ResolveStored(relPath);
      if (resolved.empty())
        continue; // outside the content directory; not ours to delete
      std::error_code ec;
      std::filesystem::remove(resolved, ec);
    }
    mPendingFileDeletes.clear();
  }
```

`Load()` resets both the registry and the unreadable flag, and nothing else:

```746:753:NeuralAmpModeler/VoLumContentStore.h
  bool Load()
  {
    mReg = Registry{};
    mRegistryUnreadable = false;
    const auto path = RegistryPath();
    std::error_code ec;
    if (mBase.empty() || !std::filesystem::exists(path, ec))
      return true;
```

`Save()` flushes unconditionally once the write lands, with no cross-check against
what the registry it just wrote actually contains:

```810:829:NeuralAmpModeler/VoLumContentStore.h
  bool Save()
  {
    if (mBase.empty())
      return true; // intentionally in-memory (unit tests / unconfigured store)
    if (mRegistryUnreadable)
      return false; // see Load(): never overwrite a library we could not read
    std::error_code ec;
    std::filesystem::create_directories(mBase, ec);
    if (!WriteJsonAtomically(RegistryPath(), RegistryToJson(mReg), ec))
      return false;
    ...
    FlushPendingFileDeletes();
    return true;
  }
```

The store is process-global and **every plugin instance constructor re-loads it**:

```499:506:NeuralAmpModeler/NeuralAmpModeler.cpp
      if (!contentDir.empty())
      {
        volum::content::GlobalContentStore().SetBaseDir(contentDir);
        volum::content::GlobalContentStore().Load();
```

```1028:1032:NeuralAmpModeler/VoLumContentStore.h
inline ContentStore& GlobalContentStore()
{
  static ContentStore store;
  return store;
}
```

**Repro**

1. DAW project with one VoLum instance. Library contains IR `A` → `ir/ir_abc__A.wav`.
2. Manage → delete `A`. `ContentStore::RemoveIR` erases the entry and queues the
   payload (`VoLumContentStore.h:931`), then `DeleteIR` calls `Save()`
   (`VoLumCustomContentApi.h:298-306`).
3. The atomic write fails — sync agent holding `volum-content.json`, read-only
   library, full disk. `Save()` returns `false` at line 819. The queue keeps
   `ir/ir_abc__A.wav`. The UI reports nothing (see F2).
4. The user adds a **second** VoLum instance (or reopens the project). The new
   constructor calls `Load()`: `mReg` is re-read from disk and still contains `A`;
   `mRegistryUnreadable` is cleared; **the queue is untouched**.
5. Any later successful save — a new import, a rename, a preset save, or
   `_VolumSaveSettingsToFile()` → `GlobalContentStore().Save()` on quit
   (`VoLumSettingsScene.inc.cpp:382`) — writes a registry that lists `A`, then
   flushes the queue and deletes `ir/ir_abc__A.wav`.
6. Library now names a payload that does not exist. The IR can never load again and
   the user's source-derived copy is gone.

A weaker variant needs no second instance: if the library is unreadable for the
whole session (F2), `Save()` never succeeds, so every queued delete is orphaned
forever and the payload files accumulate under `ir/`, `amps/`, `pedals/` with no
registry entry and no cleanup path.

**Cleared sub-questions** (asked, checked, not problems): a queued path being
re-added under the same name cannot happen — both import prefixes are freshly
minted (`VoLumCustomOverlay.h:651`, `VoLumCustomNamImport.h:50`), so a re-import
never reuses a queued `storedPath`; the same path queued twice is harmless
(`std::filesystem::remove` with `error_code` on a missing file); and the "queue not
cleared after a failed save → a later unrelated save deletes files the user did not
ask to delete" case is *safe as long as the registry is not reloaded*, because the
in-memory registry has already dropped those entries, so the later write is
consistent with the deletion. It is specifically `Load()` that breaks the
invariant.

**Suggested fix**

```cpp
bool Load()
{
  mReg = Registry{};
  mRegistryUnreadable = false;
  mPendingFileDeletes.clear();   // the queue described a registry we just discarded
  ...
```

and, as a second barrier that makes the whole class unreachable, refuse to flush a
path the registry being written still names:

```cpp
void FlushPendingFileDeletes()
{
  for (const auto& relPath : mPendingFileDeletes)
  {
    if (RegistryReferences(relPath))   // scan irs.file / pedals.file / amps[].files[].storedPath
      continue;
    ...
```

Add a regression test that (a) queues a delete, (b) fails the save, (c) calls
`Load()`, (d) succeeds a save, and asserts the payload still exists.

---

### F2 — `mRegistryUnreadable` is latched for the process lifetime, and almost every mutator ignores the refusal it causes

**Severity:** High (a whole session of library edits silently discarded; user is told nothing and is shown an empty library)
**Confidence:** CONFIRMED
**Introduced by:** `79e57ca` (flag + `Save()` refusal) and `cc277ac` (the `is_regular_file` arm)

**Evidence**

Set in two places, cleared in exactly one — `Load()` (line 749) — and `Load()` is
only ever called from the plugin constructor (`NeuralAmpModeler.cpp:502`; the only
other call sites are tests).

```761:778:NeuralAmpModeler/VoLumContentStore.h
    if (!std::filesystem::is_regular_file(path, ec))
    {
      mRegistryUnreadable = true;
      return false;
    }

    std::ifstream in(path, std::ios::binary);
    if (!in.good())
    {
      ...
      mRegistryUnreadable = true;
      return false;
    }
```

There is no consumer of the accessor anywhere in the product:

```806:808:NeuralAmpModeler/VoLumContentStore.h
  // True when a library file exists on disk that we were unable to read, so what
  // is in memory is not what is on disk and must not be written over it.
  bool RegistryUnreadable() const { return mRegistryUnreadable; }
```

(`rg RegistryUnreadable` matches only `VoLumContentStore.h` and
`tests/test_volum_content_store.cpp`.)

Three mutators were taught to check `Save()`; the rest were not:

```288:306:NeuralAmpModeler/VoLumCustomContentApi.h
inline void RenameIR(int idx, const std::string& name)
{
  auto& irs = Store().reg().irs;
  if (idx >= 0 && idx < (int)irs.size() && !name.empty())
  {
    irs[(size_t)idx].name = name;
    Store().Save();
  }
}

inline void DeleteIR(int idx)
{
  auto& irs = Store().reg().irs;
  if (idx >= 0 && idx < (int)irs.size())
  {
    Store().RemoveIR(irs[(size_t)idx].id);
    Store().Save();
  }
}
```

Same unchecked `Store().Save()` at `VoLumCustomContentApi.h:219` (`RemoveCustomAmp`),
`:343` (`SetIRShaping`), `:433` (`AddPedal`), `:443` (`RenamePedal`), `:453`
(`DeletePedal`), `:548` (`AddPreset`), `:561` (`OverwritePreset`), `:609`
(`RenamePreset`), `:625` (`DeletePreset`).

The overlay's delete/rename wrappers have no failure channel at all:

```557:566:NeuralAmpModeler/VoLumCustomOverlay.h
  void ApplyDelete(int idx)
  {
    using namespace volum::custom;
    switch (mManageKind)
    {
      case ManageKind::IR: DeleteIR(idx); break;
      case ManageKind::Pedals: DeletePedal(idx); break;
      default: DeletePreset(mAmpIdx, idx); break;
    }
  }
```

and the confirm-dialog callback unconditionally treats it as done — reloads the
list, clears the selection, fires `NotifyChanged()` (`VoLumCustomOverlay.h:864-895`).

**Repro**

1. A sync agent, backup tool or a second VoLum has `volum-content.json` open at
   VoLum launch, so `std::ifstream::good()` is false.
2. `Load()` sets `mRegistryUnreadable`, returns `false` — ignored at
   `NeuralAmpModeler.cpp:502` — and leaves an **empty** registry.
3. The user sees no custom amps, no IRs, no pedals, no preset banks, and no
   message. Plausible reaction: re-import everything.
4. Every import, rename, delete and preset save for the rest of the session
   succeeds in the UI (list updates, dialog closes, `(unsaved)` clears) and
   persists nothing. `AddIR` and `AddCustomAmp` are the only operations that
   report a failure, so the behaviour is not even self-consistent.
5. The hold is released two minutes later. `Save()` still refuses, for the whole
   process lifetime, because nothing re-reads the file.

**Note on the pin.** `test_volum_content_store.cpp:956-962` looks like it covers
recovery but does not:

```956:962:NeuralAmpModeler/tests/test_volum_content_store.cpp
  // Once the library can be read again, writing is allowed again.
  std::filesystem::remove_all(base / "volum-content.json", ec);
  REQUIRE_FALSE(ec);
  ContentStore recovered(base);
  CHECK(recovered.Load());
  CHECK_FALSE(recovered.RegistryUnreadable());
  CHECK(recovered.Save());
```

It constructs a **new** store, i.e. it tests a restart. Calling `store.Load()` /
`store.Save()` on the original object would still fail. This pin passes with the
stuck-flag bug fully present.

**Suggested fix**

- Retry the load instead of latching: in `Save()`, if `mRegistryUnreadable`, call
  `Load()` once (or a lighter `TryReload()`); if it now succeeds the in-memory
  edits are stale rather than authoritative, so the honest move is to reload and
  tell the user the session's library edits were discarded — which needs (b).
- Give the refusal a UI. Route `Save()`'s bool out of the `volum::custom::*`
  mutators (they can stay `void` and set a module-level "last write failed" flag
  that the overlay reads into `mError`, the same channel `StartImport` already
  uses at `VoLumCustomOverlay.h:694-704`).
- At minimum, surface the unreadable state once at startup: `Load()` already
  returns `false`, and `NeuralAmpModeler.cpp:502` throws that away.

**Backup questions, answered.** The `Save()` refusal does not block `.bak` — nothing
writes `.bak` on save. `volum-content.json.bak` is written only by `BackupCorrupt()`
on a JSON parse failure (`VoLumContentStore.h:1004-1013`) and
`volum-content.json.pre-1.2.1.bak` only once, by `BackupBeforeMigration`, which
explicitly never overwrites (`:846`). So yes: both backups can be arbitrarily stale
and neither is refreshed by a good run. That is by design, not a regression, but it
means the "we never lose data" story rests entirely on the live file.

---

### F3 — `AddPedal` did not get the `AddIR` treatment: a pedal import into an unwritable library reports success

**Severity:** Medium (silent non-persistence + orphaned payload; also mislabels the failure)
**Confidence:** CONFIRMED
**Introduced by:** `79e57ca` — the commit that added the `Save()` check to `AddIR` and left `AddPedal` alone

**Evidence**

```420:435:NeuralAmpModeler/VoLumCustomContentApi.h
inline int AddPedal(const std::string& name, const std::string& file = "", const std::string& group = "")
{
  auto& reg = Store().reg();
  if (reg.nextPedalIndex > content::kCustomPedalIndexMax)
    return -1;
  content::PedalItem it;
  ...
  it.legacyIndex = reg.nextPedalIndex;
  reg.nextPedalIndex = it.legacyIndex + 1;
  reg.pedals.push_back(std::move(it));
  Store().Save();
  return (int)reg.pedals.size() - 1;
}
```

Compare the sibling three lines up in the same header:

```272:286:NeuralAmpModeler/VoLumCustomContentApi.h
inline int AddIR(const std::string& name, const std::string& file = "")
{
  ...
  reg.irs.push_back(std::move(it));
  if (!Store().Save())
  {
    reg.irs.pop_back();
    return -1;
  }
  return (int)reg.irs.size() - 1;
}
```

The overlay is written as if `AddPedal`'s `-1` were symmetric with `AddIR`'s, and
attributes it to the wrong cause:

```676:684:NeuralAmpModeler/VoLumCustomOverlay.h
      else
      {
        if (volum::custom::AddPedal(base, rel) < 0)
        {
          store.RemoveStoredFile(rel);
          mError = "Custom pedal slots are full - delete a pedal first.";
          continue;
        }
      }
```

**Repro**

Library locked or read-only (F2 conditions, or simply a full disk). Manage →
Pedals → Add, pick a `.nam`. `ImportFileCopy` succeeds (payload copied),
`AddPedal` returns a valid index, `added` increments, the row appears selected in
the list, `mError` stays empty. Restart: the pedal is gone and
`pedals/pedal_xxxx__foo.nam` is an orphan. `nextPedalIndex` was also advanced and
is not rolled back, so the finite 64-slot custom-pedal pool leaks an index per
failed import.

**Suggested fix**

Mirror `AddIR` exactly, and roll `nextPedalIndex` back too:

```cpp
  const int prevNext = reg.nextPedalIndex;
  reg.nextPedalIndex = it.legacyIndex + 1;
  reg.pedals.push_back(std::move(it));
  if (!Store().Save())
  {
    reg.pedals.pop_back();
    reg.nextPedalIndex = prevNext;
    return -1;
  }
```

Then split the overlay's message so "slots are full" is only said when
`reg.nextPedalIndex > kCustomPedalIndexMax` was the actual cause (check
`PedalSlotsFull()` before calling, or return a distinct sentinel).

---

### F4 — The ASIO `mAudioInDev` canonicalization silently destroys the user's saved DirectSound input device

**Severity:** Medium (settings loss on every ASIO↔DirectSound switch; Windows only)
**Confidence:** CONFIRMED
**Introduced by:** iPlug2 `2352c01d3` — *Stop a busy MIDI port and a mismatched ASIO pair from misreporting the audio state*, hunk in `TryToChangeAudio`

**Evidence**

```526:556:iPlug2/IPlug/APP/IPlugAPP_host.cpp
#if defined OS_WIN
  ...
  if (mState.mAudioDriverType == kDeviceASIO && inputID != -1 && mAudioInputDevs.size())
  {
    const std::string openedInputName = GetAudioDeviceName(inputID);

    if (strcmp(mState.mAudioInDev.Get(), openedInputName.c_str()) != 0)
    {
      mState.mAudioInDev.Set(openedInputName.c_str());
      correctedMismatchedAsioPair = true;
    }
  }
#endif
  ...
  else if (correctedMismatchedAsioPair)
  {
    UpdateINI();
  }
```

`inputID` under ASIO is resolved from `mAudioOutDev`, three dozen lines above:

```480:490:iPlug2/IPlug/APP/IPlugAPP_host.cpp
#if defined OS_WIN
  if(mState.mAudioDriverType == kDeviceASIO)
    inputID = GetAudioDeviceIdx(mState.mAudioOutDev.Get());
```

so the effect is `indev := outdev` in `settings.ini`. On the next DirectSound
session that name is not in the DS device list, so the existing fallback wipes it:

```495:507:iPlug2/IPlug/APP/IPlugAPP_host.cpp
  if (inputID == -1)
  {
    if (mDefaultInputDev > -1)
    {
      resetToDefault = true;
      inputID = mDefaultInputDev;

      if (mAudioInputDevs.size())
        mState.mAudioInDev.Set(GetAudioDeviceName(inputID).c_str());
    }
```

**Repro**

1. DirectSound, input = "Microphone (Focusrite USB)". Quit. `settings.ini`:
   `indev=Microphone (Focusrite USB)`.
2. Preferences → ASIO → OK. `TryToChangeAudio` rewrites `indev=FlexASIO` and calls
   `UpdateINI()` immediately (line 555, *before* `InitAudio`).
3. Switch back to DirectSound. `GetAudioDeviceIdx("FlexASIO")` → -1 → reset to the
   OS default input. The user's chosen DS input is gone and has to be re-picked.

Two smaller consequences of the same hunk: `UpdateINI()` at line 555 runs *before*
`InitAudio`, so a subsequent open failure leaves the corrected name persisted even
though `RestoreActiveAudioStateAfterFailure` reverts `mState` — it does call
`UpdateINI()` again at `:434`, so this self-heals, but only because of that second
write. And because the Cancel path re-runs `TryToChangeAudio`
(`IPlugAPP_dialog.cpp:366`), a Cancel can now write to `settings.ini`, which the
"INI file won't be changed" comment at `:358` says it will not.

**Why this writeback is not needed.** The stated justification is that Preferences
believed the stale `indev`. It no longer does — the dialog fix in the same commit
resolves the input selection from the *output device id*, never from
`mState.mAudioInDev`:

```162:172:iPlug2/IPlug/APP/IPlugAPP_dialog.cpp
  if (driverType == kDeviceASIO && mAudioOutputDevs.size())
  {
    for (int i = 0; i < mAudioInputDevs.size(); i++)
    {
      if (mAudioInputDevs[i] == mAudioOutputDevs[outdevidx])
      {
        indevidx = i;
        break;
      }
    }
  }
```

So the dialog is already honest without touching persisted state.

**Suggested fix**

Drop the `mState.mAudioInDev.Set(...)` + `UpdateINI()` writeback and keep only the
dialog-side id resolution. If a canonical value is genuinely wanted at runtime,
write it to `mActiveState` only, or store the ASIO input under a separate INI key
so the DirectSound choice survives.

---

### F5 — The shutdown watchdog covers far more than audio teardown, and its budget is partly consumed before that work starts

**Severity:** Medium (process force-kill during a legitimately slow shutdown; user-visible loss is limited, but the justification in the commit does not match the code's reach)
**Confidence:** CONFIRMED for the reach and the ordering; LIKELY for a real-world 5 s overrun
**Introduced by:** iPlug2 `3492e6e90` — `VoLumArmShutdownWatchdog()` in `~IPlugAPPHost`

**Evidence**

Armed first thing in the destructor body:

```44:62:iPlug2/IPlug/APP/IPlugAPP_host.cpp
IPlugAPPHost::~IPlugAPPHost()
{
  mExiting = true;

  // The window is already destroyed by the time this runs and VoLum persists its
  // settings during the session, so nothing is lost by forcing the process out if
  // driver teardown wedges. ...
  VOLUM_LOG("shutdown", "audio teardown begin");
  VoLumArmShutdownWatchdog();

  CloseAudio();
  ...
  VOLUM_LOG("shutdown", "audio teardown complete");
}
```

```101:106:iPlug2/IPlug/APP/VoLumAppShutdown.h
/** Grace period for the whole teardown once shutdown has begun. Audio teardown
 * is milliseconds of work; anything approaching this is a wedged driver. */
inline constexpr int kVoLumShutdownWatchdogMs = 5000;
```

But `CloseAudio()` alone can spend 2 000 ms of that budget before the driver is
even touched:

```92:95:iPlug2/IPlug/APP/VoLumAppShutdown.h
inline constexpr int kVoLumFadeWaitMs = 10;
inline constexpr int kVoLumMaxFadeWaits = 200; // 2 s
```

and, decisively, `mIPlug` is declared **before** `mDAC`, so it is destroyed **last**
— i.e. the entire plugin teardown runs inside the watchdog window, after the
"audio teardown complete" line:

```249:253:iPlug2/IPlug/APP/IPlugAPP_host.h
private:
  std::unique_ptr<IPlugAPP> mIPlug = nullptr;
  std::unique_ptr<RtAudio> mDAC = nullptr;
```

```542:566:NeuralAmpModeler/NeuralAmpModeler.cpp
NeuralAmpModeler::~NeuralAmpModeler()
{
  _VolumStopLoader();
  ...
  _VolumSaveCurrentToSettings();
#ifdef APP_API
  _VolumSaveSettingsToFile();
#else
  ...
```

`_VolumStopLoader()` is a blocking `join()` on the async model loader, and the stop
flag is only observable between requests:

```20:35:NeuralAmpModeler/VoLumLoader.inc.cpp
void NeuralAmpModeler::_VolumStopLoader()
{
  ...
  if (mVolumLoaderThread.joinable())
    mVolumLoaderThread.join();
}
```

**What saves this from being High.** `OnUIClose()` runs *before* the arm point and
already writes everything: WM_DESTROY calls `pAppHost->CloseWindow()`
(`IPlugAPP_dialog.cpp:609`) → `IGEditorDelegate::CloseWindow()` →
`IEditorDelegate::CloseWindow() { OnUIClose(); }` (`IPlugEditorDelegate.h:92`), and

```1238:1252:NeuralAmpModeler/NeuralAmpModeler.cpp
void NeuralAmpModeler::OnUIClose()
{
  ...
  _VolumSaveCurrentToSettings();
#ifdef APP_API
  _VolumSaveSettingsToFile();
#endif
}
```

with `_VolumSaveSettingsToFile()` also flushing the content library
(`VoLumSettingsScene.inc.cpp:382`). Only after that does `sInstance = nullptr`
(`IPlugAPP_dialog.cpp:611`) run the destructor. So a watchdog kill loses the
destructor's *duplicate* save, not the first one, and `WriteJsonAtomically`'s
temp-then-rename means a kill mid-write cannot corrupt either file.

**Residual problems**

- The commit's own reasoning — "audio teardown is milliseconds of work" — is about
  `CloseAudio()`, but the timer also has to cover a loader-thread join that can be
  in the middle of a multi-second WaveNet `.nam` load, plus a second full settings
  + content-registry write. Worst case that is a 3 000 ms budget for work with no
  bound, on a slow disk, on a machine that is paging at quit.
- `std::_Exit(0)` (`VoLumAppShutdown.h:114`) reports **success**. A wedged shutdown
  is indistinguishable from a clean one to any script or installer that checks the
  exit code, including `scripts/e2e-standalone-win.ps1`.
- A kill during `WriteJsonAtomically` leaves `volum-settings.json.tmp.<ticks>.<hash>`
  behind (`VoLumSettingsFileIO.h:32-43`), permanently, with nothing that reaps it.
- `VoLumArmShutdownWatchdog` constructs a `std::thread` inside an implicitly
  `noexcept` destructor. `std::thread`'s constructor throws `std::system_error`
  when a thread cannot be created; that becomes `std::terminate` — a crash on quit
  in the one situation (resource exhaustion) where you least want one. Low
  probability, one `try/catch(...)` to remove.

**Suggested fix**

Arm the watchdog *inside* `CloseAudio()` (or immediately before it and disarm right
after), scoped to the driver calls it exists for, and let the plugin teardown run
untimed. If a whole-process timer is wanted, arm it after `mIPlug.reset()`. Either
way: `std::_Exit(1)` or a distinct nonzero code, wrap the `std::thread` construction
in `try/catch(...)`, and add a `CHECK` that the watchdog value exceeds the *sum* of
the fade budget and a documented teardown allowance rather than just the fade
budget (`test_iplug_app_shutdown.cpp:142` only checks the latter).

---

### F6 — The MIDI fallback turns a transient boot-time port conflict into permanent config loss, with no notification

**Severity:** Medium (silent settings loss; self-inflicted on a device that is merely busy for a few seconds)
**Confidence:** CONFIRMED
**Introduced by:** iPlug2 `2352c01d3` — the `openOrTurnOff` lambda in `SelectMIDIDevice`

**Evidence**

```584:601:iPlug2/IPlug/APP/IPlugAPP_host.cpp
  auto openOrTurnOff = [&](auto&& open) {
    try
    {
      open();
      return true;
    }
    catch (RtMidiError& e)
    {
      e.printMessage();
      VOLUM_LOG("midi", "could not open the saved MIDI port; turning MIDI off for this direction");
      if (direction == ERoute::kInput)
        mState.mMidiInDev.Set(OFF_TEXT);
      else
        mState.mMidiOutDev.Set(OFF_TEXT);
      UpdateINI();
      return false;
    }
  };
```

**Repro**

The interface's MIDI port is enumerated (so `GetMIDIPortNumber` succeeds) but is
briefly held by another application — a DAW that has not finished quitting, a
vendor control panel, a second VoLum — at the moment VoLum launches. `openPort`
throws once. `settings.ini` is rewritten to `indev=off`. The other application
releases the port five seconds later. VoLum stays MIDI-off for that session **and
every session after**, because the saved name is now `off`. The user gets no
message: `Init()` runs before the window exists, and the only trace is a line in
`volum.log`.

Consistency of the fallback itself is fine — `PopulateMidiDialogs`
(`IPlugAPP_dialog.cpp:244-254`) re-reads `mState` when Preferences next opens, so
the combo shows "off", and `mMidiIn`/`mMidiOut` are left closed, matching the
state. It is the *persistence* that is wrong.

**Suggested fix**

Keep the catch (it fixes a real silent-exit), but do not persist on the first
failure. Fall back to off **in memory only** and leave `settings.ini` naming the
device, so the next launch retries. Persist off only after the same port fails on
N consecutive launches, or only when the failure comes from a user action in
Preferences (where a dialog can say so) rather than from `Init()`. At minimum, show
a message once the window exists — the app already has `_ShowMessageBox` and the
diagnostic log is not a user-facing channel.

---

### F7 — `IsSafeStoredRelPath` accepts `"."` and bare directory names, so a resolved delete can target the library directory itself

**Severity:** Low (needs a hand-edited or sync-mangled registry; `remove()` only succeeds on an empty directory)
**Confidence:** CONFIRMED for the validator gap, SPECULATIVE for real-world impact
**Introduced by:** `79e57ca` — new `IsSafeStoredRelPath` in `VoLumContentStore.h`

**Evidence**

```81:99:NeuralAmpModeler/VoLumContentStore.h
inline bool IsSafeStoredRelPath(const std::string& relPath)
{
  if (relPath.empty())
    return false;

  const auto path = PathFromUtf8(relPath);
  ...
  if (path.is_absolute() || path.has_root_name() || path.has_root_directory())
    return false;

  for (const auto& part : path)
  {
    if (part == "..")
      return false;
  }
  return true;
}
```

`"."` and `"ir"` both pass. `ResolveStored(".")` yields `mBase / "."`, which
`FlushPendingFileDeletes` and `RemoveStoredFile` hand to
`std::filesystem::remove`. `remove` on a directory succeeds when it is empty, so a
registry entry with `"path": "ir"` deletes the `ir/` directory once the last IR is
gone; `"path": "."` targets the content directory itself.

**The rest of the validator is correct** and I checked every case asked about:
`..` rejected including nested (`"ir/./../../x"` iterates a `".."` component);
absolute `/x` rejected via `has_root_directory`; `C:\x`, `C:x` and `\\server\share`
rejected via `has_root_name`; empty rejected. The Windows/POSIX separator asymmetry
is benign in both directions — `/` is a separator on Windows so `ir/x.wav` works and
`../x` is caught, while on POSIX a backslash string like `"..\\..\\etc\\passwd"` is
a single (weird) filename inside the library, not an escape. Symlinks and NTFS
junctions are *not* rejected, and cannot be from the string alone: a `ir/` that has
been replaced by a junction still resolves outside. That is an accepted limitation
worth one line in the header comment rather than a code change.

**Caller audit for the empty return** — all handled, one with a behaviour change
worth knowing about:

- `VoLumCustomNamImport.h:68-71` — `is_regular_file("")` is false → import fails
  with a user-facing reason. Correct.
- `VoLumSceneRig.inc.cpp:829-833` — explicit `!abs.empty()` guard. Correct.
- `VoLumSceneRig.inc.cpp:179` — returns `""` upward; callers treat empty as "no
  capture". Correct.
- `VoLumSceneRig.inc.cpp:557-578` — empty path flows into
  `IrFileSizeAcceptable("")`, which returns `true` on an `error_code`
  (`VoLumIrFileGuard.h:49-52`), then `_StageIR` fails and the user gets "VoLum could
  not load this impulse response". Acceptable, if a slightly odd route.
- `NeuralAmpModeler.cpp:836-850` — **LIKELY behaviour change**: `rel` is non-empty
  so the `else` branch that sets `mVolumMainLoadError = "LOAD FAILED - custom
  capture path is missing"` is skipped, but `fileToLoad` is now `""` and
  `_VolumQueueMainModelLoad` returns immediately on an empty path
  (`VoLumLoader.inc.cpp:39-40`). An unsafe `storedPath` therefore produces a silent
  no-load instead of a visible load error. Marginal, but it is the one caller that
  now builds a request from an empty string rather than reporting.

**Suggested fix**

```cpp
  for (const auto& part : path)
  {
    if (part == ".." || part == ".")
      return false;
  }
  return path.has_filename();
```

and, in `NeuralAmpModeler.cpp:838`, check the resolved path rather than `rel` so the
existing `LOAD FAILED` message still fires.

---

### F8 — Buffer-size writeback: correct where it fires, but half-persisted and still leaves the INI lying in the case it does not

**Severity:** Low
**Confidence:** CONFIRMED
**Introduced by:** iPlug2 `2352c01d3` — the writeback in `InitAudio`

**Evidence**

```847:861:iPlug2/IPlug/APP/IPlugAPP_host.cpp
    mDAC->startStream();
    ...
    if (mBufferSize != iovs && NormalizeAPPBufferSize(mBufferSize) == mBufferSize)
      mState.mBufferSize = mBufferSize;

    mActiveState = mState;
```

The representability guard is right: `NormalizeAPPBufferSize` rounds up into
`kBufferSizeOptions` (`IPlugAPP_host.h:74-86`), so requiring a fixed point means
the value is exactly one of the combo strings, and `PopulateAudioDialogs`'
`CB_FINDSTRINGEXACT` (`IPlugAPP_dialog.cpp:225-230`) will therefore select it.
**The dialog cannot display a different number than was persisted.** That part is
clean.

Two residuals:

- `mState.mBufferSize` is written but no `UpdateINI()` follows on the
  `TryToChangeAudio` success path (`IPlugAPP_host.cpp:561-564` returns `true`
  directly). It reaches disk only on the startup path (`:432-434`) and via the
  Preferences OK handler (`IPlugAPP_dialog.cpp:350`, "INI file will be changed see
  MainDialogProc"). Use **Apply** and close with the window's X and the negotiated
  size is not persisted.
- When the negotiated size is *not* representable (a driver settling on, say, 480),
  the writeback is skipped by design — so `settings.ini` and Preferences keep
  advertising the refused request while the stream runs at 480, which is the exact
  symptom the commit set out to fix, just narrowed. It is now also inconsistent
  with the Settings page, which reads the live value through the new
  `GetIOBufferSize()` (`IPlugAPP_host.h:234`), so the two surfaces disagree.

**Suggested fix**

Call `UpdateINI()` after a successful `InitAudio` in `TryToChangeAudio`. For the
non-representable case, either add the negotiated size to the combo for that
session, or annotate the Preferences label ("128 requested, 480 in use") so the
disagreement is explained rather than hidden.

---

### F9 — Preferences: the ASIO input resolution silently falls back to list position 0, which the surrounding comments say cannot happen

**Severity:** Low
**Confidence:** CONFIRMED (code path), SPECULATIVE (frequency)
**Introduced by:** iPlug2 `2352c01d3` — `PopulateDriverSpecificControls`

**Evidence**

```162:172:iPlug2/IPlug/APP/IPlugAPP_dialog.cpp
  if (driverType == kDeviceASIO && mAudioOutputDevs.size())
  {
    for (int i = 0; i < mAudioInputDevs.size(); i++)
    {
      if (mAudioInputDevs[i] == mAudioOutputDevs[outdevidx])
      {
        indevidx = i;
        break;
      }
    }
  }
```

If no input entry matches the selected output device — an output-only ASIO driver,
or one whose input side failed to probe (`ProbeAudioIO` only pushes a device into
`mAudioInputDevs` when `info.inputChannels > 0`, `IPlugAPP_host.cpp:333-337`) —
`indevidx` keeps whatever the name-match loop left, defaulting to `0`. The input
combo then names a *different* ASIO driver, and:

```192:200:iPlug2/IPlug/APP/IPlugAPP_dialog.cpp
  if (mDAC && mAudioInputDevs.size())
  {
    if (mAudioOutputDevs.size() && mAudioInputDevs[indevidx] == mAudioOutputDevs[outdevidx])
      inputDevInfo = outputDevInfo;
    else
      inputDevInfo = mDAC->getDeviceInfo(mAudioInputDevs[indevidx]);
```

takes the `else`, so `getDeviceInfo` is called on two *different* ASIO drivers in
one dialog pass. That is not worse than the pre-change code (which did the same),
but it contradicts the new comment's claim that "after the fix above the ASIO case
always names the same device twice", and the source pin
(`test_iplug_app_shutdown.cpp:280`, `CountOccurrences(src, "mDAC->getDeviceInfo(") == 2`)
counts occurrences in the *source*, not calls at runtime — it would pass with this
path fully live.

Index arithmetic itself is safe: `indevidx`/`outdevidx` are initialized to `0`, only
ever assigned from a valid loop index, and every subscript is guarded by the
matching `.size()` (`:162`, `:186`, `:192`, `:194`). Zero devices, duplicated names
(first match wins, deterministic) and a device disappearing between enumerate and
open are all survivable — the last one lands in `TryToChangeAudio`'s existing
`GetAudioDeviceIdx() == -1` fallback. `mAudioInputDevs`/`mAudioOutputDevs` hold
RtAudio *device ids* from `getDeviceIds()`-equivalent enumeration and are compared
as ids on both sides of `:166`, so the id/index distinction is handled correctly.

**Suggested fix**

Make the no-match case explicit: disable the input combo and skip the input probe
entirely when the ASIO output device has no input counterpart, rather than
displaying and probing an unrelated driver. Convert the count pin into an assertion
about the *resolution* (e.g. a small pure helper `ResolveAsioInputIndex(inDevs,
outDevs, outIdx) -> std::optional<int>`, unit-tested with mismatched, shorter and
empty input lists).

---

## Reviewed and found correct

Each of these was traced against the code, not just read in the diff.

- **`WriteJsonAtomically`'s `dump()` try/catch** (`VoLumSettingsFileIO.h:89-103`).
  Serialization happens *before* `MakeAtomicJsonTempPath`, so on throw there is no
  temp file to remove, the real file is untouched, and `ec` is set to
  `invalid_argument` with `false` returned. `nlohmann::type_error` derives from
  `std::exception` so the catch is effective. Both product callers check the
  result (`VoLumSettingsScene.inc.cpp:366`, `:373`, `:419`) and
  `ContentStore::Save` checks it at `VoLumContentStore.h:818`. No caller treats
  `false` as success. `test_volum_settings_atomic_write.cpp:127-148` covers the
  truncated-UTF-8 case including "previous good file survives" and "no temp file
  left behind". Correct and complete. *(One pre-existing nit, not from this range:
  `_VolumSaveSettingsToFile` returns early after a failed main-settings write, so
  the dual-amp sidecar and the content-store flush are skipped and the two files
  can diverge. Unchanged by this commit range.)*

- **`AddIR` rollback completeness.** Registry entry removed (`pop_back`,
  `VoLumCustomContentApi.h:282`), copied payload removed by the caller
  (`VoLumCustomOverlay.h:671`, immediate `RemoveStoredFile` — correct, since
  nothing committed references it), in-memory list rebuilt by `ReloadList()` after
  the loop, and no dangling selection: `mSel` is only assigned when `added > 0` and
  is assigned from the rebuilt `mItems`, not from the failed index
  (`VoLumCustomOverlay.h:688-693`). Batch behaviour is right too — a failure
  mid-batch leaves earlier successes selected. Complete.

- **`AddCustomAmp` / `UpdateCustomAmp` rollback.** `AddCustomAmp` pops the pushed
  entry on a failed save (`:163-167`); `UpdateCustomAmp` saves the previous
  manifest and restores it (`:194-200`), preserving the immutable id. The builder
  caller removes every file the transaction copied
  (`VoLumLayoutBuild.inc.cpp:1230-1235`) and only calls `_VolumSelectCustomAmp` on
  a valid index (`:1245-1246`). The edit path also re-resolves the target by id
  before writing (`:1196-1201`), which closes the stale-index-across-instances
  hole. Complete. *(Pre-existing and out of range: an edit that replaces a capture
  leaves the previous `storedPath` copy orphaned — `PrepareCustomNamImport` mints a
  new prefix and `UpdateCustomAmp` overwrites the manifest without queueing the old
  files. Worth a follow-up, not a regression from tonight.)*

- **`PrepareCustomNamImport`'s failure handling** (`VoLumCustomNamImport.h:39-45`,
  `:65-88`). Every early return goes through `fail()`, which deletes every file the
  transaction copied and clears `copiedPaths`; validator exceptions are caught
  (both `std::exception` and `...`); the registry is deliberately never mutated.
  The change in this range is formatting only.

- **`ContentStore::Load()`'s "missing vs unreadable" distinction, as a
  distinction.** Testing `is_regular_file` *before* opening is the right call and
  the cross-platform reasoning in `cc277ac` is accurate — libc++ opens a directory
  and fails on first read, which is indistinguishable from an empty file, so the
  pre-`cc277ac` code took the "corrupt, back it up, rewrite" branch on macOS only.
  Symlinked registries still work (`is_regular_file` follows links) and OneDrive
  cloud placeholders are reported as regular files by MSVC's non-name-surrogate
  reparse-tag handling. First run on a new machine stays a clean writable empty
  library (`exists()` short-circuits at `:752`, pinned at
  `test_volum_content_store.cpp:965-975`). The *lifecycle* of the flag it sets is
  F2; the detection itself is right.

- **The deferred-delete ordering, in the single-load case.** With no reload in
  play, a failed `Save()` leaves the registry entry present on disk *and* the
  payload present, and a later successful save writes the removal and then deletes
  — consistent at every point. `test_volum_content_store.cpp:977-1014` covers this
  correctly and would fail against the pre-fix ordering. Immediate deletion for
  uncommitted transaction copies is also the right split, and pinned at `:1016-1030`.

- **`RemoveStoredFile` going through `ResolveStored`** (`VoLumContentStore.h:878-885`).
  The escape guard now covers the immediate-delete path as well as the queued one,
  and `test_volum_content_store.cpp:896-920` genuinely deletes its sentinel against
  the pre-fix code.

- **`CloseAudio()`'s new teardown policy** (`VoLumAppShutdown.h:74-99`,
  `IPlugAPP_host.cpp:683-725`). The "callback dead" test is not racy in a way that
  matters: `mAudioDone` is set by the audio callback within one buffer period of
  `mAudioEnding` (`IPlugAPP_host.cpp:1019-1020`), so a 2 000 ms non-response means
  the callback is not running. Skipping `abortStream` and calling `closeStream`
  directly cannot leave the device open — `RtApiAsio::closeStream` stops a running
  stream itself (`RtAudio.cpp:3350-3355`) and `RtApiCore::closeStream` does the
  same for macOS (`:1482-1483`, `:1505-1506`), neither waiting on the callback.
  `closeStream` is also now wrapped so an escaping `RtAudioError` cannot terminate
  the process from a destructor. The `plan.closeStream` field is computed but never
  read (the caller early-returns on `!isStreamOpen()` instead) — harmless, and the
  test asserts it, so it is at least not dead in the pin.

- **Watchdog is standalone-only.** `VoLumAppShutdown.h` lives in `iPlug2/IPlug/APP/`
  and is included from exactly two places: `IPlugAPP_host.cpp` (APP-only TU) and
  `tests/test_iplug_app_shutdown.cpp`. The test never calls
  `VoLumArmShutdownWatchdog`, so the test binary cannot `_Exit` on itself. No
  plugin or host build path reaches the arm. The new `VoLumDiagLog.h` include from
  `IPlugAPP_host.cpp` also resolves on macOS: `IPLUG_INC_PATHS` contains
  `$(PROJECT_ROOT)` = `$(SRCROOT)/..` = `NeuralAmpModeler/`
  (`iPlug2/common-mac.xcconfig:51,91`, project `HEADER_SEARCH_PATHS` at
  `NeuralAmpModeler-macOS.xcodeproj/project.pbxproj:3336-3339`), and `VOLUM_LOG`
  swallows all exceptions (`VoLumDiagLog.h:141-144`) so it is safe from a
  destructor.

- **`IDC_BUTTON_OS_DEV_SETTINGS` null-guard, and the rest of the dialog's `mDAC`
  uses.** `IPlugAPP_dialog.cpp:505` is guarded, and I checked every other
  dereference in `PreferencesDlgProc` / the populate helpers: the only two are
  `:188` and `:197`, both now behind `if (mDAC && ...)`. `PopulateSampleRateList`
  handles the resulting unprobed `DeviceInfo`s (`:45`, guarded on
  `probed && probed`), and `PopulateAudioInputList` / `PopulateAudioOutputList`
  both bail on `!info->probed` (`:73`, `:96`), so the ASIO-failed-to-load path
  yields empty combos rather than a crash. Swapping the output probe ahead of the
  input probe has no side-effect ordering problem (the only state write is
  `mState.mAudioInChanR = mState.mAudioInChanL` at `:91`, which nothing upstream
  reads).

- **`ClearPresetHooksIfOwnedBy` / `PresetHookOwner`** (`VoLumCustomContentApi.h:493-512`,
  `NeuralAmpModeler.cpp:551`). Comparing an opaque `this` token and clearing only
  on a match is the right shape for a process-global hook pair; a later instance
  that claimed the hooks is correctly left alone. `_VolumClaimPresetOps` re-binds
  before every preset operation, so a stale owner can never be routed through.

- **Overlay action-code widening** (`VoLumOverlayActionCodes.h`,
  `VoLumCustomOverlay.h:394-409`). Families are `1<<20 + n*(1<<16)`, comfortably
  above the fixed codes (`kCabNameBase = 70`, `kArtBase = 80`) and comfortably
  inside `int` at the top family (`Popup` → 1 572 864). Every range test in
  `HandleManageAction` and `HandleBuilderAction` was updated to `kActionStride`;
  I found no residual `+ 100` or `+ 256` bound. `kPopupBase` is only ever used as
  `code - kPopupBase` with no upper-bound test, which is still correct because
  popup hotspots live in their own `mPopupHotspots` list.

- **`mOwnsGesture` double-click guard** (`VoLumCustomOverlay.h:189-192`, `:272-286`).
  Set on every `OnMouseDown` and consumed once in `OnMouseDblClick`; a dblclick
  whose down landed on a modal that has since closed is correctly inert. No path
  leaves it stuck true in a way that would swallow a legitimate double-click, since
  the next real down re-arms it.
