# VoLum 1.2.1 pre-release audit — findings

Audit of every VoLum-owned file plus the iPlug2 fork's VoLum patches and the
vendor call sites, run against `release/1.2.1` at `ebb0215`.

Ground rules for this pass:

- **Audio voicing is frozen.** Defects (gap, click, NaN, wrong gain, a control
  that does not apply) may be fixed. Intended tone — filter curves, drive,
  envelope times, mix laws, effect tuning — is reported only, never changed.
- Findings are only recorded here with a mechanism. "Looks risky" without a code
  path does not get an entry.

Severity: **BLOCKER** (crash / hang / data loss / unusable), **MAJOR**
(user-visible wrong behavior), **MINOR**, **NIT**.

---

## Baseline

- `run-tests-win.ps1`: 607 doctests, 2 632 860 assertions, all green.
- The working tree's only dirt is the tracked iPlug2 patch
  (`iplug2-patches/0001-app-host-expose-io-buffer-and-stream-latency.patch`),
  applied idempotently by the test script. Expected, keep out of commits.

---

## F1 — BLOCKER: closing the standalone can hang the process forever

**Status:** root cause proven from a live hung process (dump + disassembly +
source). This is the bug the user hit: "after installing I couldn't start it
until I killed a dead background process."

**Where:** `iPlug2/Dependencies/IPlug/RTAudio/RtAudio.cpp:3431`
(`RtApiAsio::stopStream`), reached from `iPlug2/IPlug/APP/IPlugAPP_host.cpp:621`
(`IPlugAPPHost::CloseAudio`, a VoLum fork patch).

**Mechanism.** Clicking the window close button runs, on the UI thread:

`MainDlgProc WM_CLOSE` → `DestroyWindow` → `MainDlgProc WM_DESTROY` →
`IPlugAPPHost::sInstance = nullptr` (`IPlugAPP_dialog.cpp:582`; `sInstance` is a
`unique_ptr`, so the host and the whole plugin are destroyed *inside*
`WM_DESTROY`) → `~IPlugAPPHost` → `CloseAudio()` → `mDAC->abortStream()` →
`RtApiAsio::stopStream()`:

```cpp
if ( handle->drainCounter == 0 ) {
  handle->drainCounter = 2;
  WaitForSingleObject( handle->condition, INFINITE );  // block until signaled
}
```

`handle->condition` is signaled **by the ASIO callback** once the drain
completes. If the driver has stopped calling back, nothing ever signals it and
the UI thread blocks forever.

VoLum's `CloseAudio` patch already anticipates a dead callback and waits for it
in a *bounded* loop (`while (!mAudioDone && waitCount < 200) Sleep(10)`, i.e. 2
seconds), and it even logs `"CloseAudio timed out waiting for callback fade"`.
But on timeout it proceeds to call `abortStream()` regardless — straight into
the unbounded wait. **The guard detects the exact condition that makes the next
call hang, and then makes that call anyway.**

**Evidence from the hung process (PID 34468, captured live):**

- 6 threads, all in `Wait`, zero CPU delta over 5 s. Fully blocked, not spinning.
- Top-level window `#32770` titled `VoLum` exists but `IsWindowVisible` is false
  and `IsHungAppWindow` is true — created, never shown, message loop dead.
- Thread 0 (UI): `NtWaitForSingleObject` ← `WaitForSingleObjectEx` ← `VoLum+0xb021`
  ← `VoLum+0x70cf1` ← `VoLum+0x6ebbe` ← `UserCallDlgProcCheckWow` ←
  `NtUserDestroyWindow` ← ... ← `uxtheme!OnDwpSysCommand` ← `OnDwpNcLButtonDown`.
  So: title-bar click, close, destroy, and a wait that never returns.
- Disassembly of `VoLum+0x70cf1` is a byte-for-byte match for the patched
  `CloseAudio`: `mAudioEnding = true`, `cmp ebx,0C8h` / `Sleep(10)` / `inc ebx`
  fade loop, then `call [rax+48h]` (`abortStream`). `VoLum+0xb021` matches
  `stopStream`'s `drainCounter == 0` check, `drainCounter = 2` store,
  `mov edx,0FFFFFFFFh` (INFINITE) and the `WaitForSingleObject` call.
- Thread 3 is FlexASIO's own thread, blocked in `KERNELBASE!GetOverlappedResult`
  — i.e. the driver is wedged, which is why the callback never fires again.
- Thread 1 is VoLum's async model loader, idle in `SleepConditionVariableSRW`.
  Not implicated; `_VolumStopLoader` is never reached because the destructor
  blocks earlier, in `CloseAudio`.
- `volum.log` for that launch confirms it got fully up before dying:
  `[startup] ... (standalone) instance created`, `[audio] reset: 48000 Hz, block
  64, in 1 / out 2`, then both models loaded, then nothing.

**Impact.** The window vanishes but the process lives forever, holding the audio
device. The next launch therefore cannot open the device, so VoLum appears not to
start at all. The only recovery is Task Manager. There is no diagnostic for the
user — the log simply stops.

**Contributing condition.** The user's `settings.ini` had
`indev=ASIO4ALL v2` with `outdev=FlexASIO` — two different ASIO drivers for one
duplex stream, which ASIO cannot do (see F2). A second process (a live VST3
instance in a DAW) also held the hardware two seconds before the standalone
opened it.

**Fix sketch.** Bound the driver-teardown waits rather than trusting the driver:
give `RtApiAsio::stopStream`'s drain wait a timeout and continue on expiry; do
the same for the equivalent DirectSound drain wait
(`RtAudio.cpp:6445`) and the callback-thread join (`RtAudio.cpp:6325`). Then
VoLum's existing bounded fade guard becomes meaningful instead of decorative.
Device teardown only — no DSP or voicing impact. Requires a commit in the iPlug2
fork plus a submodule pointer bump.

---

## F2 — MAJOR: under ASIO, Preferences populates the input channel list from the wrong device

**Where:** `iPlug2/IPlug/APP/IPlugAPP_dialog.cpp:155-177`
(`PopulateDriverSpecificControls`).

**Mechanism.** `indevidx` indexes `mAudioInputDevs`; `outdevidx` indexes
`mAudioOutputDevs`. `ProbeAudioIO` builds those two vectors from different
predicates:

```cpp
if(info.inputChannels > 0)  mAudioInputDevs.push_back(i);
if(info.outputChannels > 0) mAudioOutputDevs.push_back(i);
```

so a driver exposing only inputs appears in one list and not the other, and the
two indices are **not interchangeable**. Under ASIO the dialog nonetheless
selects the input combo by `outdevidx`:

```cpp
if(driverType == kDeviceASIO)
  SendDlgItemMessage(hwndDlg,IDC_COMBO_AUDIO_IN_DEV,CB_SETCURSEL, outdevidx, 0);
```

but then populates the input **channel** list from `indevidx`:

```cpp
inputDevInfo = mDAC->getDeviceInfo(mAudioInputDevs[indevidx]);
PopulateAudioInputList(hwndDlg, &inputDevInfo);
```

So three different devices can be in play at once: the stream actually opens on
`mAudioOutDev` for both directions (`IPlugAPP_host.cpp:412` and `:468` use the
*output* name as the input ID under ASIO), the input combo displays whatever sits
at `outdevidx` in the input list, and the input channel dropdown is built from
`mAudioInDev`'s channel count.

**Trigger.** Any ASIO setup where the input and output device lists differ. The
user's machine is exactly this case: `indev=ASIO4ALL v2`, `outdev=FlexASIO`,
reachable because `IPlugAPP_dialog.cpp:374-378` seeds `mAudioInDev` from
`mAudioInputDevs[0]` and `mAudioOutDev` from `mAudioOutputDevs[0]` independently
when the driver type changes, and ASIO4ALL reports zero output channels when its
outputs are not enabled.

**Impact.** The input channel dropdown offers the wrong channel count for the
device that is actually open, so "IN 1" can mean a channel that does not exist
or is not the one the user patched. If `outdevidx` exceeds the input combo's item
count, `CB_SETCURSEL` clears the selection and the input device shows blank.

**Correction to an earlier read of this finding:** ignoring `indev` under ASIO is
deliberate and correct — one driver owns both directions, and the dialog does
disable the input combo. The defect is the index mixing and the channel list, not
the ignored device.

**Fix sketch.** Under ASIO, drive both combos and both channel lists from the
single selected driver, and keep `mAudioInDev` in sync with `mAudioOutDev` so the
persisted config cannot describe an impossible pair.

---

## F5 — MAJOR: null `mDAC` dereference crashes Preferences when the ASIO driver fails to load

**Where:** `iPlug2/IPlug/APP/IPlugAPP_dialog.cpp:476`.

```cpp
if( (_this->mState.mAudioDriverType == kDeviceASIO) && (_this->mDAC->isStreamRunning() == true))
  ASIOControlPanel();
```

**Mechanism.** `mDAC` is a `unique_ptr` that is legitimately null:
`TryToChangeAudioDriverType()` nulls it and leaves it null when the `RtAudio`
constructor throws (`IPlugAPP_host.cpp:449-453`), and
`RestoreActiveAudioStateAfterFailure` returns early without recreating it
(`:403-407`). Meanwhile `PopulateDriverSpecificControls` enables the button
purely from the driver combo's selection, with no reference to `mDAC`:

```cpp
if(driverType == kDeviceASIO)
{
  ComboBox_Enable(GetDlgItem(hwndDlg, IDC_COMBO_AUDIO_IN_DEV), FALSE);
  Button_Enable(GetDlgItem(hwndDlg, IDC_BUTTON_OS_DEV_SETTINGS), TRUE);
}
```

So the button is live while `mDAC` is null, and the click handler dereferences it
without a guard. Every other call site in the file is guarded or preceded by a
`.size()` check on a list that `ProbeAudioIO` clears when `mDAC` is null
(`:270-277`), which is why this is the only one that bites.

**Trigger.** `settings.ini` has `driver=1` (ASIO) but no ASIO driver can be
instantiated — the user uninstalled FlexASIO or ASIO4ALL, or the ASIO subsystem
is broken. Launch VoLum, open Preferences, click the device-settings button.

**Impact.** Hard crash of the standalone, from a button the UI presents as
enabled, in precisely the situation where the user is trying to repair their
audio configuration.

**Fix sketch.** Guard the dereference, and gate the button on `mDAC` being
non-null rather than on the combo selection alone.

---

## F6 — MINOR: a mismatched ASIO pair can be persisted and then silently ignored

**Where:** `iPlug2/IPlug/APP/IPlugAPP_dialog.cpp:374-378`.

On a driver-type change, `mAudioInDev` and `mAudioOutDev` are seeded from index 0
of two independently built lists, so under ASIO they can name different drivers.
`indev` is then ignored for the rest of the session
(`IPlugAPP_host.cpp:412`, `:468`). Harmless to audio, but it makes the persisted
config actively misleading when diagnosing a user report — it cost real time
while investigating F1, because the config appeared to describe an impossible
stream that was never actually attempted.

---

## F3 — MINOR: shipped Windows PDBs cannot symbolize the shipped installer build

**Where:** `NeuralAmpModeler/scripts/makedist-win.bat` and the CI packaging step.

**Mechanism.** The app is linked twice in one packaging run. From the PE debug
directories of the artifact the user installed (`VoLum-win (6).zip`):

- installer's `VoLum.exe`: link 2026-07-29 12:06:26Z, PDB age **1**
- portable zip's `VoLum_x64.exe`: link 2026-07-29 12:09:56Z, PDB age **2**
- both reference the same PDB GUID `CF3DB24B5A664750B74205DA9B3D6B03`
- the shipped `NeuralAmpModeler-app_x64.pdb` is age 2

So the PDB matches the portable exe and **mismatches the installer exe**. `cdb`
refuses it with `mismatched pdb`, which is exactly what happened while
diagnosing F1 — the crash of the binary most users run could not be symbolized
with the symbols shipped alongside it.

**Impact.** No user crash or hang report from an installed build can be turned
into a stack trace. This directly undercuts the 1.2.1 diagnostic-log feature,
whose whole purpose is making user reports actionable.

**Fix sketch.** Build once and package the same binary into both the installer
and the portable zip, or capture the PDB from the same link that produced the
shipped exe. Packaging only; no product code change.

---

## F4 — NIT: a failed iPlug2 patch step cannot fail the test run

**Where:** `NeuralAmpModeler/scripts/run-tests-win.ps1:24`.

The patch script is invoked bare, while every other check goes through
`Invoke-Check`, which propagates the exit code. The file's own comment explains
why that guard exists; the patch call is the one place that skips it.

---

## Subagent findings

Nine parallel Opus 5 subagents audited one subsystem each; a second pass with
GPT 5.6 covers the same partitions independently. Their findings are merged in
below as they land, deduplicated against the above.

<!-- MERGE POINT: subagent findings -->
