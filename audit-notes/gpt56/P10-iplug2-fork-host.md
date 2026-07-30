## Summary

The standalone host is not release-safe around device failure. I found three additional BLOCKER paths independent of the already-confirmed ASIO `CloseAudio()` hang: DirectSound has its own unbounded shutdown deadlock, ASIO mode synchronously initializes every installed driver several times before opening the selected one, and an occupied saved MIDI input can throw out of startup before the window exists. The highest-risk systemic issue is ASIO probing: the fork comment says repeated probing of one driver has already caused heap corruption, but `openStream()` still re-probes every driver internally, so the attempted mitigation does not actually remove the repeated initialization.

The current `settings.ini` mismatch (`indev=ASIO4ALL v2`, `outdev=FlexASIO`) is also mishandled: runtime correctly uses the output ASIO driver for both directions, but Preferences probes the stale input driver's channels and sample rates. I found three BLOCKER, five MAJOR, and three MINOR defects. All findings below are proved from reachable code; consequences that require a faulty or stalled external driver are stated as conditional triggers. The separately confirmed ASIO `CloseAudio()`/`RtApiAsio::stopStream()` infinite wait is intentionally not re-reported.

## VoLum modification inventory

Upstream comparison point: `dbca7cfbee0e916d3debadddaa82206557ac7fc6` (`VST3: SetBlockSize(DEFAULT_BLOCK_SIZE) in IPlugVST3Processor CTOR`), the merge base of `volum/asio-channel-routing` and `origin/master`. The fork has 17 commits after that base and a committed diff of 734 insertions/142 deletions across 16 files. The parent pins `6f55d11c6273a954842cc8f6b14dc1cf1aaae08b`; one additional working-tree patch modifies `IPlugAPP_host.h`.

- `iPlug2/IGraphics/IControl.cpp` — removes the local `WDL_NO_SUPPORT_UTF8` suppression around `dirscan.h`, preserving UTF-8 directory support after resolving the earlier VST3 conflict.
- `iPlug2/IGraphics/IGraphics.cpp` — coalesces corner-resizer mouse moves and applies one `Resize()` per display tick, with a final flush on mouse-up.
- `iPlug2/IGraphics/IGraphics.h` — adds the `PromptForFiles()` multi-select API/fallback and stores the pending coalesced-resize target.
- `iPlug2/IGraphics/Platforms/IGraphicsMac.h` — declares the macOS native `PromptForFiles()` override.
- `iPlug2/IGraphics/Platforms/IGraphicsMac.mm` — implements multi-file selection with `NSOpenPanel` and returns full paths plus the containing directory.
- `iPlug2/IGraphics/Platforms/IGraphicsMac_view.mm` — reverses Cocoa wheel deltas when `isDirectionInvertedFromDevice` is set so controls use device-relative direction.
- `iPlug2/IGraphics/Platforms/IGraphicsWin.cpp` — preserves fractional precision-touchpad wheel deltas; converts popup menus/tooltips to UTF-16 APIs; implements `GetOpenFileNameW` multi-select.
- `iPlug2/IGraphics/Platforms/IGraphicsWin.h` — declares the Windows native `PromptForFiles()` override.
- `iPlug2/IPlug/APP/IPlugAPP_dialog.cpp` — exposes every physical input/output channel, makes guitar input mono and outputs independently selectable, stabilizes the buffer list, adds driver-change rollback, and opens the VoLum manual.
- `iPlug2/IPlug/APP/IPlugAPP_host.cpp` — adds driver-construction fallback, channel routing in software, arbitrary callback-size accumulation, bounded fade waiting, audio-open rollback, and mono-input mirroring.
- `iPlug2/IPlug/APP/IPlugAPP_host.h` — fixes `AppState` output-channel copying, defines normalized buffer choices, adds routing/accumulator state; the working-tree patch also adds `GetIOBufferSize()` and `GetStreamLatencyFrames()` for the VoLum latency readout.
- `iPlug2/IPlug/APP/VoLumIPlugAPPVectorAccumulator.h` — new fixed-quantum accumulator that carries partial input/output vectors across RtAudio callbacks whose sizes are not multiples of 64.
- `iPlug2/IPlug/IPlugLogger.h` — changes Windows debug output from `OutputDebugStringA` to UTF-16 `OutputDebugStringW`.
- `iPlug2/IPlug/IPlugPaths.cpp` — removes the old out-of-line Windows UTF conversion helpers after relocation.
- `iPlug2/IPlug/IPlugPaths.h` — removes the old public declarations of those conversion helpers.
- `iPlug2/IPlug/IPlugUtilities.h` — adds header-local Windows UTF-8/UTF-16 conversion helpers used by graphics and logging.
- `NeuralAmpModeler/iplug2-patches/0001-app-host-expose-io-buffer-and-stream-latency.patch` — the uncommitted-on-fork latency getter patch currently applied to `IPlugAPP_host.h`.
- `NeuralAmpModeler/iplug2-patches/apply-iplug2-patches.ps1` — idempotently checks reverse/forward applicability and applies all working-tree patches on Windows.
- `NeuralAmpModeler/iplug2-patches/apply-iplug2-patches.sh` — equivalent idempotent patch application for macOS/Unix build entry points.
- `NeuralAmpModeler/iplug2-patches/README.md` — documents the mirror branch, committed fork changes, and the remaining latency working-tree patch.

## Findings

### F-P10-1: BLOCKER — DirectSound can deadlock shutdown when either duplex endpoint stops advancing

**Where:** `iPlug2/Dependencies/IPlug/RTAudio/RtAudio.cpp:6323-6326,6442-6450,6533-6545,6616-6680`; host trigger at `iPlug2/IPlug/APP/IPlugAPP_host.cpp:621-650`.

**Evidence:**

```cpp
// RtAudio.cpp:6616-6680
MUTEX_LOCK( &stream_.mutex );
// ...
while ( true ) {
  result = dsWriteBuffer->GetCurrentPosition( NULL, &safeWritePointer );
  // ...
  result = dsCaptureBuffer->GetCurrentPosition( NULL, &safeReadPointer );
  // ...
  if ( safeWritePointer != startSafeWritePointer &&
       safeReadPointer != startSafeReadPointer ) break;
  Sleep( 1 );
}
```

```cpp
// RtAudio.cpp:6533-6545, then stopStream at 6442-6450
void RtApiDs :: abortStream()
{
  // ...
  handle->drainCounter = 2;
  stopStream();
}

if ( stream_.mode == OUTPUT || stream_.mode == DUPLEX ) {
  if ( handle->drainCounter == 0 ) {
    handle->drainCounter = 2;
    WaitForSingleObject( handle->condition, INFINITE );
  }
  stream_.state = STREAM_STOPPED;
  MUTEX_LOCK( &stream_.mutex );
```

```cpp
// RtAudio.cpp:6323-6326
stream_.callbackInfo.isRunning = false;
WaitForSingleObject((HANDLE) stream_.callbackInfo.thread, INFINITE);
CloseHandle((HANDLE) stream_.callbackInfo.thread);
```

**Mechanism:** The DirectSound callback thread takes `stream_.mutex` before the duplex-start synchronization loop. That loop has no timeout and does not re-check `callbackInfo.isRunning` or stream state. If either playback or capture position remains fixed, the callback owns the mutex forever. `IPlugAPPHost::CloseAudio()` eventually calls `abortStream()`; DirectSound skips its condition wait because `abortStream()` pre-sets `drainCounter`, but then blocks trying to acquire the mutex held by the callback. A callback blocked earlier (for example in the user callback) instead reaches `closeStream()`'s infinite thread join.

**Trigger:** Run the standalone with DirectSound duplex, then unplug/disable an endpoint or encounter a driver stall after `Start()` succeeds but before both device cursors advance; close VoLum or change the audio device.

**Impact:** The UI thread never returns from shutdown/device switching. The window can remain frozen and the process retains the audio resources until forcibly killed. This is the DirectSound sibling of the confirmed ASIO shutdown bug, not a duplicate report of it.

**Fix sketch:** Make DirectSound startup waiting bounded and cancellation-aware, release the mutex while polling, stop the underlying buffers before joining, and replace every UI-thread infinite wait with a bounded failure path that can abandon/quarantine the backend.

**Proposed regression test:** `directsound_close_cancels_frozen_duplex_preroll` — use a fake DirectSound endpoint whose capture cursor never advances, request close, and assert the host returns within a fixed deadline without waiting on the callback thread indefinitely.

### F-P10-2: BLOCKER — ASIO mode initializes every installed driver repeatedly before opening the selected one

**Where:** `iPlug2/IPlug/APP/IPlugAPP_host.cpp:290-300,674-692,759-761`; `iPlug2/Dependencies/IPlug/RTAudio/RtAudio.cpp:2851-2874,2947-2955,2981-2997`.

**Evidence:**

```cpp
// IPlugAPP_host.cpp:290-300 — first full-system probe
for (int i=0; i<nDevices; i++)
{
  try
  {
    info = mDAC->getDeviceInfo(i);
  }
  catch (RtAudioError& e)
  {
    e.printMessage();
    continue;
  }
```

```cpp
// IPlugAPP_host.cpp:674-683 — selected device is probed again
// calling getDeviceInfo twice on the same ASIO device id
// can leave the driver in a bad state and corrupt the heap ...
// (observed crash 0xc0000374 on RME Babyface Pro FS).
RtAudio::DeviceInfo inDevInfo;
try { inDevInfo = mDAC->getDeviceInfo(inId); }
catch (RtAudioError& e) { e.printMessage(); }
```

```cpp
// RtAudio.cpp:2851-2874 — each getDeviceInfo loads and initializes a DLL
ASIOError result = drivers.asioGetDriverName((int) device, driverName, 32);
// ...
if (!drivers.loadDriver(driverName)) {
  // ...
  return info;
}
result = ASIOInit(&driverInfo);
if (result != ASE_OK) {
  // ...
  return info;
}
```

```cpp
// RtAudio.cpp:2947-2955 and 2981-2985 — openStream probes all of them again
void RtApiAsio :: saveDeviceInfo(void)
{
  devices_.clear();
  unsigned int nDevices = getDeviceCount();
  devices_.resize(nDevices);
  for (unsigned int i=0; i<nDevices; i++)
    devices_[i] = getDeviceInfo(i);
}

// In probeDeviceOpen():
this->saveDeviceInfo();
```

**Mechanism:** Selecting ASIO first makes `ProbeAudioIO()` load/`ASIOInit()` every registered driver. `InitAudio()` then probes the selected driver again. `openStream()` calls `RtApiAsio::probeDeviceOpen()`, which calls `saveDeviceInfo()` and initializes every driver yet again before the actual selected-driver `ASIOInit()`. The host's “probe once” mitigation only avoids one direct output lookup; it misses the transitive full re-probe inside `openStream()`. All calls are synchronous on the startup/preferences UI thread and have no timeout or process isolation.

**Trigger:** Launch with `driver=1` or choose ASIO in Preferences. A single unrelated registered ASIO driver whose `loadDriver()`/`ASIOInit()` hangs or faults is sufficient; the selected RME path also repeats the exact initialization pattern the fork comment associates with observed heap corruption.

**Impact:** A bad unselected ASIO driver can hang or crash VoLum before the main window appears. Because startup immediately retries ASIO from unchanged `settings.ini`, the user has no in-app recovery path. Even with good drivers, the selected driver is initialized repeatedly despite the fork's documented crash avoidance.

**Fix sketch:** Enumerate ASIO registry names without initializing every DLL; cache one validated device-information pass; remove the pre-open selected-device probe or change RtAudio so `openStream()` consumes the cache instead of calling `saveDeviceInfo()` again. Fault/hang containment needs an out-of-process probe or a startup safe mode that can bypass saved ASIO settings.

**Proposed regression test:** `asio_open_initializes_only_selected_driver_once` — inject two fake ASIO drivers, count `loadDriver`/`ASIOInit` calls, and assert opening driver B neither initializes driver A nor initializes B repeatedly. A companion process-level test should make driver A hang/fault and assert VoLum still reaches a recoverable UI when B is selected.

### F-P10-3: BLOCKER — An occupied saved MIDI port can terminate every startup before the window appears

**Where:** `iPlug2/IPlug/APP/IPlugAPP_host.cpp:70-74,531-618`; `iPlug2/Dependencies/IPlug/RTMidi/RtMidi.cpp:2544-2576`; `iPlug2/IPlug/APP/IPlugAPP_main.cpp:55-57,138-142`.

**Evidence:**

```cpp
// IPlugAPP_host.cpp:70-74
ProbeAudioIO();
InitMidi();
ProbeMidiIO();
SelectMIDIDevice(ERoute::kInput, mState.mMidiInDev.Get());
SelectMIDIDevice(ERoute::kOutput, mState.mMidiOutDev.Get());
```

```cpp
// IPlugAPP_host.cpp:545-557 — no RtMidiError handler
if (mMidiIn)
{
  mMidiIn->closePort();
  // ...
  mMidiIn->openPort(port-1);
  return true;
}
```

```cpp
// RtMidi.cpp:2568-2576
MMRESULT result = midiInOpen(&data->inHandle, portNumber, ...);
if (result != MMSYSERR_NOERROR) {
  errorString_ = "MidiInWinMM::openPort: error creating Windows MM MIDI input port.";
  error(RtMidiError::DRIVER_ERROR, errorString_);
  return;
}

// RtMidi.cpp:556-558
else {
  std::cerr << '\n' << errorString << "\n\n";
  throw RtMidiError(errorString, type);
}
```

```cpp
// IPlugAPP_main.cpp:55-57,138-142
IPlugAPPHost* pAppHost = IPlugAPPHost::Create();
pAppHost->Init();
pAppHost->TryToChangeAudio();
// ...
catch(std::exception e)
{
  DBGMSG("Exception: %s", e.what());
  return 1;
}
```

**Mechanism:** A Windows MIDI input is opened synchronously from `Init()`. WinMM failures such as an already-allocated/held port become `RtMidiError::DRIVER_ERROR`; `SelectMIDIDevice()` does not catch it. The only surrounding handler is the outer `WinMain` catch, which exits before `CreateDialog()`. In release builds `DBGMSG` is compiled away, and the saved MIDI name is not reset, so the next launch repeats the same failure.

**Trigger:** Save a MIDI input in Preferences, close VoLum, let another application hold that WinMM input exclusively (or leave the device present but unable to open), then launch VoLum.

**Impact:** VoLum exits without a window or actionable message on every launch while the port remains unavailable. The user cannot turn MIDI off from the app and must release the device or edit/delete `settings.ini`.

**Fix sketch:** Catch `RtMidiError` inside `SelectMIDIDevice()`/startup, close any partial port, set the failed direction to `off`, persist the recoverable state, and show a nonfatal message after the main window exists.

**Proposed regression test:** `startup_busy_saved_midi_port_falls_back_to_off` — make fake `openPort()` throw `DRIVER_ERROR`, then assert initialization succeeds, the main window is created, MIDI is off, and the repaired setting is persisted.

### F-P10-4: MAJOR — A mismatched ASIO pair makes Preferences inspect a different input driver than runtime opens

**Where:** `iPlug2/IPlug/APP/IPlugAPP_host.cpp:461-476`; `iPlug2/IPlug/APP/IPlugAPP_dialog.cpp:133-179`; current configuration `C:/Users/SteffenDangmann/AppData/Local/VoLum/settings.ini:3-4`.

**Evidence:**

```cpp
// IPlugAPP_host.cpp:466-476 — output device is authoritative at runtime
#if defined OS_WIN
  if(mState.mAudioDriverType == kDeviceASIO)
    inputID = GetAudioDeviceIdx(mState.mAudioOutDev.Get());
  else
    inputID = GetAudioDeviceIdx(mState.mAudioInDev.Get());
#endif
outputID = GetAudioDeviceIdx(mState.mAudioOutDev.Get());
```

```cpp
// IPlugAPP_dialog.cpp:155-176 — display mirrors output, probing does not
if(driverType == kDeviceASIO)
  SendDlgItemMessage(hwndDlg, IDC_COMBO_AUDIO_IN_DEV, CB_SETCURSEL, outdevidx, 0);
// ...
if (mAudioInputDevs.size())
{
  inputDevInfo = mDAC->getDeviceInfo(mAudioInputDevs[indevidx]);
  PopulateAudioInputList(hwndDlg, &inputDevInfo);
}
if (mAudioOutputDevs.size())
{
  outputDevInfo = mDAC->getDeviceInfo(mAudioOutputDevs[outdevidx]);
  PopulateAudioOutputList(hwndDlg, &outputDevInfo);
}
PopulateSampleRateList(hwndDlg, &inputDevInfo, &outputDevInfo);
```

```ini
; Current settings.ini:3-4
indev=ASIO4ALL v2
outdev=FlexASIO
```

**Mechanism:** ASIO runtime correctly forces `inId == outId` by resolving both from `mAudioOutDev`. Preferences disables the input combo and visually selects `outdevidx`, but it still resolves `inputDevInfo` from `indevidx`, which is based on the stale `mAudioInDev`. Channel choices and the sample-rate intersection therefore come from ASIO4ALL input plus FlexASIO output, while `InitAudio()` opens FlexASIO input and output. Changing the output combo never canonicalizes `mAudioInDev`.

**Trigger:** Load the current mismatched settings, or retain any stale ASIO input name while choosing a different ASIO output driver.

**Impact:** Preferences can offer input channels the active driver does not have, omit channels it does have, and compute sample-rate choices from the wrong pair. Applying such a selection silently clamps routing or fails opening, and `settings.ini` continues to claim a pair that can never be opened as an ASIO duplex stream.

**Fix sketch:** On Windows ASIO, canonicalize `mAudioInDev = mAudioOutDev` whenever settings are read or output changes, and use the same resolved device ID/info for both channel lists and sample-rate population.

**Proposed regression test:** `asio_mismatched_saved_pair_uses_output_driver_for_both_directions` — seed ASIO4ALL/FlexASIO with different input counts/rates, load the mismatched state, and assert the dialog, active state, and persisted state all use FlexASIO for both directions.

### F-P10-5: MAJOR — Driver-renegotiated buffer size is used for audio but the UI and INI keep the rejected request

**Where:** `iPlug2/IPlug/APP/IPlugAPP_host.cpp:739-761,773-775`; `iPlug2/Dependencies/IPlug/RTAudio/RtAudio.cpp:3233-3240`; `iPlug2/IPlug/APP/IPlugAPP_dialog.cpp:195-207`; `iPlug2/IPlug/APP/IPlugAPP_host.cpp:194-199`.

**Evidence:**

```cpp
// IPlugAPP_host.cpp:739,759-761,773-775
mBufferSize = iovs; // mBufferSize may get changed by stream
// ...
mDAC->openStream(..., &mBufferSize, &AudioCallback, this, &options);
// ...
mDAC->startStream();
mActiveState = mState;
```

```cpp
// RtAudio.cpp:3233-3240
result = ASIOCreateBuffers(handle->bufferInfos, nChannels, stream_.bufferSize, &asioCallbacks);
if (result != ASE_OK) {
  *bufferSize = preferSize;
  stream_.bufferSize = *bufferSize;
  result = ASIOCreateBuffers(handle->bufferInfos, nChannels, stream_.bufferSize, &asioCallbacks);
}
```

```cpp
// IPlugAPP_dialog.cpp:202-207 — combo is based on requested state
mState.mBufferSize = NormalizeAPPBufferSize(mState.mBufferSize);
str.SetFormatted(32, "%i", mState.mBufferSize);
LRESULT iovsidx = SendDlgItemMessage(
  hwndDlg, IDC_COMBO_AUDIO_BUF_SIZE, CB_FINDSTRINGEXACT, -1, (LPARAM) str.Get());
SendDlgItemMessage(hwndDlg, IDC_COMBO_AUDIO_BUF_SIZE, CB_SETCURSEL, iovsidx, 0);
```

**Mechanism:** RtAudio is allowed to overwrite `mBufferSize` and explicitly does so when an ASIO driver rejects the requested size but accepts its preferred size. The host correctly runs with the new `mBufferSize`, and the latency patch reports it, but never copies it back to `mState.mBufferSize`. `mActiveState`, Preferences, and `UpdateINI()` retain the rejected request.

**Trigger:** Request 128 from a driver that rejects it and falls back to preferred size 64—the exact requested-128/reported-64 behavior in the supplied real-run context.

**Impact:** The dialog says 128 while audio actually runs at 64, CPU/latency behavior differs from the user's selection, and every launch retries the rejected size. The latency line can contradict the buffer combo because it reads the real `mBufferSize`.

**Fix sketch:** After a successful `openStream()`, copy the negotiated `mBufferSize` into `mState` before assigning `mActiveState`, update/persist the combo, and explicitly notify when the driver changed the request.

**Proposed regression test:** `negotiated_buffer_size_replaces_requested_state` — fake `openStream()` changing 128 to 64 and assert active state, UI selection, INI, `GetIOBufferSize()`, and the latency report all say 64.

### F-P10-6: MAJOR — Routing L and R to the same physical output silently discards L

**Where:** `iPlug2/IPlug/APP/IPlugAPP_dialog.cpp:94-113,434-448`; `iPlug2/IPlug/APP/IPlugAPP_host.cpp:707-735,887-899`.

**Evidence:**

```cpp
// IPlugAPP_dialog.cpp:104-113 — both selectors contain every channel
for (int i = 0; i < (int) info->outputChannels; i++)
{
  buf.SetFormatted(20, "%i", i+1);
  SendDlgItemMessage(hwndDlg,IDC_COMBO_AUDIO_OUT_L,CB_ADDSTRING,0,(LPARAM)buf.Get());
  SendDlgItemMessage(hwndDlg,IDC_COMBO_AUDIO_OUT_R,CB_ADDSTRING,0,(LPARAM)buf.Get());
}
```

```cpp
// IPlugAPP_host.cpp:890-899
if (pluginOuts >= 1)
{
  const int devCh = (outOffsetL < devOuts) ? outOffsetL : 0;
  pOutputBufferD[devCh * nFrames + i] =
    _this->mAudioVectorAccumulator.GetOutputChannel(0)[readIdx] * APP_MULT;
}
if (pluginOuts >= 2)
{
  const int devCh = (outOffsetR < devOuts) ? outOffsetR : /* ... */;
  pOutputBufferD[devCh * nFrames + i] =
    _this->mAudioVectorAccumulator.GetOutputChannel(1)[readIdx] * APP_MULT;
}
```

**Mechanism:** The dialog permits `out1 == out2`; a one-channel output also forces both offsets to zero through clamping. The callback writes L first and then assigns R to the same sample address, overwriting L rather than rejecting/canonicalizing the impossible stereo route.

**Trigger:** Select the same physical channel in Output 1 (L) and Output 2 (R), or open a one-output device while the stereo defaults remain 1/2.

**Impact:** The entire left plugin output disappears. For dual-amp/stereo operation this can remove one lane while the UI still presents both output routes as active.

**Fix sketch:** Prevent duplicate L/R selections when two distinct outputs are required. For a one-channel device, expose an explicit supported mono routing mode instead of silently resolving both selectors to one address; the actual mono policy is a product decision, not an audio-voicing change proposed by this audit.

**Proposed regression test:** `duplicate_physical_output_route_is_rejected_or_canonicalized` — feed distinct L/R sentinels, request the same physical channel, and assert the host reports/corrects the invalid route rather than silently replacing L with R.

### F-P10-7: MAJOR — The visible MIDI channel selectors are inert

**Where:** `iPlug2/IPlug/APP/IPlugAPP_dialog.cpp:250-265,508-516`; `iPlug2/IPlug/APP/IPlugAPP_host.cpp:947-975`; `iPlug2/IPlug/APP/IPlugAPP.cpp:74-90`; declarations at `iPlug2/IPlug/APP/IPlugAPP_host.h:254-255`.

**Evidence:**

```cpp
// IPlugAPP_dialog.cpp:508-516
case IDC_COMBO_MIDI_IN_CHAN:
  if (HIWORD(wParam) == CBN_SELCHANGE)
    mState.mMidiInChan = (int) SendDlgItemMessage(
      hwndDlg, IDC_COMBO_MIDI_IN_CHAN, CB_GETCURSEL, 0, 0);
  break;
case IDC_COMBO_MIDI_OUT_CHAN:
  if (HIWORD(wParam) == CBN_SELCHANGE)
    mState.mMidiOutChan = (int) SendDlgItemMessage(
      hwndDlg, IDC_COMBO_MIDI_OUT_CHAN, CB_GETCURSEL, 0, 0);
```

```cpp
// IPlugAPP_host.cpp:967-975 — every short message is forwarded unchanged
else if (pMsg->size())
{
  IMidiMsg msg;
  msg.mStatus = pMsg->at(0);
  msg.mData1 = pMsg->size() > 1 ? pMsg->at(1) : 0;
  msg.mData2 = pMsg->size() > 2 ? pMsg->at(2) : 0;
  _this->mIPlug->mMidiMsgsFromCallback.Push(msg);
}
```

```cpp
// IPlugAPP.cpp:78-83 — output-channel handling is explicitly disabled
//TODO: midi out channel
// uint8_t status;
// if(mAppHost->mMidiOutChannel > -1)
//   status = mAppHost->mMidiOutChannel-1 |
//            ((uint8_t) msg.StatusMsg() << 4);
```

**Mechanism:** Preferences stores `mState.mMidiInChan`/`mMidiOutChan`, but no code copies those values to `mMidiInChannel`/`mMidiOutChannel`; the input callback never filters by status-channel nibble, and MIDI output sends the original status. Repository-wide use search finds the state fields only in INI/UI code and the active fields only in the commented-out TODO.

**Trigger:** Choose MIDI input channel 2, then send a note on channel 1; or choose output channel 2 and send MIDI from the plugin.

**Impact:** The app advertises and persists channel routing that does nothing. VoLum currently declares `PLUG_DOES_MIDI_IN/OUT` as 0, so the least misleading release behavior would be to hide the whole MIDI section; if MIDI is intended, its channel controls are functionally wrong.

**Fix sketch:** Hide/disable MIDI controls when the plugin declares no MIDI capability. Otherwise, apply the input-channel filter in `MIDICallback()` and output-channel remap in `SendMidiMsg()`, updating active channel state when settings change.

**Proposed regression test:** `standalone_midi_channel_selection_is_enforced` — with input set to channel 2, assert channel-1 messages are dropped and channel-2 messages pass; with MIDI disabled, assert the controls are not exposed.

### F-P10-8: MAJOR — Fresh macOS installs save first-run settings to a different filename and lose them on restart

**Where:** `iPlug2/IPlug/APP/IPlugAPP_host.cpp:92-108,143-159`.

**Evidence:**

```cpp
// Existing-directory path, IPlugAPP_host.cpp:99,106-108
mINIPath.SetFormatted(MAX_PATH_LEN,
  "%s/Library/Application Support/%s/", getenv("HOME"), BUNDLE_NAME);
if(stat(mINIPath.Get(), &st) == 0)
{
  mINIPath.Append("settings.ini");
```

```cpp
// Fresh-directory path, IPlugAPP_host.cpp:151-159
mode_t process_mask = umask(0);
int result_code = mkdir(mINIPath.Get(), S_IRWXU | S_IRWXG | S_IRWXO);
umask(process_mask);

if(!result_code)
{
  mINIPath.Append("\\settings.ini");
  UpdateINI();
}
```

**Mechanism:** On a fresh macOS account, the host creates the VoLum support directory and appends a Windows backslash. On Unix, backslash is a legal filename character, not a separator, so that process reads/writes a file named `\settings.ini` inside the directory. On the next launch the directory-exists branch appends `settings.ini` without the backslash, does not find the first file, and writes defaults.

**Trigger:** First launch on macOS when `~/Library/Application Support/VoLum/` does not yet exist; configure audio/MIDI settings, quit, and relaunch.

**Impact:** All first-session standalone device settings are apparently accepted and written, then disappear on the second launch. The stray backslash-named file remains.

**Fix sketch:** Append `"settings.ini"` (the base path already ends in `/`) or construct the path through a platform path join helper in both branches.

**Proposed regression test:** `mac_first_run_settings_path_matches_second_run` — point `HOME` at a temporary tree with no VoLum directory, initialize and write non-default settings, initialize again, and assert the same file/path and values are loaded.

### F-P10-9: MINOR — Out-of-range saved channels are clamped only for DSP, leaving blank controls and permanently invalid INI state

**Where:** `iPlug2/IPlug/APP/IPlugAPP_host.cpp:121-130,700-709`; `iPlug2/IPlug/APP/IPlugAPP_dialog.cpp:84-90,105-113`.

**Evidence:**

```cpp
// IPlugAPP_host.cpp:121-130 — unvalidated unsigned state
mState.mAudioInChanL = GetPrivateProfileInt("audio", "in1", 1, mINIPath.Get());
mState.mAudioInChanR = mState.mAudioInChanL;
mState.mAudioOutChanL = GetPrivateProfileInt("audio", "out1", 1, mINIPath.Get());
mState.mAudioOutChanR = GetPrivateProfileInt("audio", "out2", 2, mINIPath.Get());
```

```cpp
// IPlugAPP_host.cpp:700-709 — only local wants are clamped
auto clamp1Based = [](uint32_t v, int hi) {
  int iv = static_cast<int>(v);
  if (iv < 1) iv = 1;
  if (hi >= 1 && iv > hi) iv = hi;
  return iv;
};
const int wantInL  = devInChans  > 0 ? clamp1Based(mState.mAudioInChanL,  devInChans)  : 1;
const int wantOutL = devOutChans > 0 ? clamp1Based(mState.mAudioOutChanL, devOutChans) : 1;
const int wantOutR = devOutChans > 0 ? clamp1Based(mState.mAudioOutChanR, devOutChans) : 1;
```

```cpp
// IPlugAPP_dialog.cpp:105-113 — the invalid state is used as the combo index
for (int i = 0; i < (int) info->outputChannels; i++)
{
  // add items 1..outputChannels
}
SendDlgItemMessage(hwndDlg, IDC_COMBO_AUDIO_OUT_L,
                   CB_SETCURSEL, mState.mAudioOutChanL - 1, 0);
SendDlgItemMessage(hwndDlg, IDC_COMBO_AUDIO_OUT_R,
                   CB_SETCURSEL, mState.mAudioOutChanR - 1, 0);
```

**Mechanism:** Runtime routing creates clamped local values but never writes them back to `mState`. The combo boxes then receive an out-of-range index and show no selected channel; `UpdateINI()` writes the original invalid number again.

**Trigger:** Edit `in1/out1/out2` to 0, negative, or larger than the device channel count; the same occurs naturally when a same-named replacement device has fewer channels.

**Impact:** Audio is routed to a hidden clamped channel while Preferences appears blank, and the invalid value survives every launch. Negative profile integers first convert to very large `uint32_t`, making the behavior especially opaque.

**Fix sketch:** Validate signed profile values before conversion and canonicalize all channel state against the selected device immediately after probing, then persist any repair.

**Proposed regression test:** `invalid_saved_channels_are_clamped_and_persisted` — load `in1=0,out1=99,out2=-1` against a two-output device and assert UI, active state, routing offsets, and rewritten INI all contain valid visible channels.

### F-P10-10: MINOR — The fork’s UTF-16 logger conversion turns every nonempty Windows debug message into an empty string

**Where:** `iPlug2/IPlug/IPlugLogger.h:45-66`; `iPlug2/IPlug/IPlugUtilities.h:318-329`; `iPlug2/WDL/wdlutf8.h:238-249`.

**Evidence:**

```cpp
// IPlugLogger.h:64-66
wchar_t bufW[4096];
UTF8ToUTF16(bufW, buf, WDL_utf8_get_charlen(buf));
OutputDebugStringW(bufW);
```

```cpp
// IPlugUtilities.h:318-328
static void UTF8ToUTF16(wchar_t* utf16Str, const char* utf8Str, int maxLen)
{
  int requiredSize = MultiByteToWideChar(CP_UTF8, 0, utf8Str, -1, NULL, 0);
  if (requiredSize > 0 && requiredSize <= maxLen)
  {
    MultiByteToWideChar(CP_UTF8, 0, utf8Str, -1, utf16Str, requiredSize);
    return;
  }
  utf16Str[0] = 0;
}
```

```cpp
// wdlutf8.h:238-249 — cpos excludes the terminating NUL
while (bpos < bytepos && str[bpos])
{
  bpos += wdl_utf8_parsechar(str+bpos,NULL);
  cpos++;
}
return cpos;
#define WDL_utf8_get_charlen(rd) WDL_utf8_bytepos_to_charpos((rd), 0x7fffffff)
```

**Mechanism:** `MultiByteToWideChar(..., -1, ...)` returns a required size that includes the terminating NUL. `WDL_utf8_get_charlen()` excludes it. Even an ASCII message of N characters therefore asks for N+1 wide units with `maxLen == N`, fails the condition, and sets `bufW[0]=0`.

**Trigger:** Emit any nonempty `DBGMSG` in a Windows debug build.

**Impact:** The debugger receives an empty string. This removes the very diagnostics used throughout the host for device-open, timeout, and recovery failures; release builds already compile `DBGMSG` out.

**Fix sketch:** Pass the actual destination capacity (`4096`) to `UTF8ToUTF16`, not the source character count.

**Proposed regression test:** `windows_dbgmsg_preserves_nonempty_utf8_text` — intercept the output function, log ASCII and multibyte UTF-8 messages, and assert the UTF-16 text and terminator are present.

### F-P10-11: MINOR — Device selection writes through a zero-sized `std::string`

**Where:** `iPlug2/IPlug/APP/IPlugAPP_dialog.cpp:303-309`, called at `392-393,406-407,493-504`.

**Evidence:**

```cpp
auto getComboString = [&](WDL_String& str, int item, WPARAM idx) {
  std::string tempString;
  long len = (long) SendDlgItemMessage(hwndDlg, item, CB_GETLBTEXTLEN, idx, 0) + 1;
  tempString.reserve(len);
  SendDlgItemMessage(hwndDlg, item, CB_GETLBTEXT, idx, (LPARAM) tempString.data());
  str.Set(tempString.c_str());
};
```

**Mechanism:** `reserve(len)` changes capacity but leaves `size()==0`. In C++17, `data()` is writable only through the string's existing element range (plus its managed terminator); asking Win32 to write `len` characters into reserved-but-unconstructed storage violates the string contract. The string size also remains zero after the API call. Current MSVC commonly leaves the bytes readable long enough for `str.Set()`, but that manifestation is library-dependent; the undefined write is proved.

**Trigger:** Choose any audio or MIDI device from the Preferences combo.

**Impact:** Device names can become empty/corrupted under a different standard-library implementation, checked iterator mode, or optimizer behavior, causing the wrong setting to be saved and fallback on the next open. Unicode/long names increase the value of a deterministic test.

**Fix sketch:** `resize(len)` before `CB_GETLBTEXT`, use the API's returned character count, then resize to that count before copying to `WDL_String`; reject `CB_ERR`.

**Proposed regression test:** `preferences_combo_selection_copies_exact_device_name` — select a long Unicode device label under checked/ASan instrumentation and assert the exact label reaches `mState` without an out-of-bounds write.

## Areas read and found clean

- Established the fork topology from `git log`, all remotes, the upstream merge base, per-commit file stats, full fork diff, and current working-tree patch. No additional modified iPlug2 file was omitted from the inventory.
- Read `IPlugAPP_host.cpp/.h` completely: settings load/write, device enumeration, driver switching and rollback, MIDI setup/callback, stream open/close, channel routing, accumulator integration, and latency getters. The separately supplied ASIO `CloseAudio()` hang was confirmed in code but intentionally excluded as an existing finding.
- Read `IPlugAPP_dialog.cpp` completely: sample-rate/channel population, all Preferences commands, ASIO control panel, main dialog creation, Help/About, and `WM_CLOSE`/`WM_DESTROY`. Aside from findings above, modal Preferences serialization prevents a second UI command from re-entering `InitAudio()` while one open is synchronously in progress.
- Read `IPlugAPP.cpp`, `IPlugAPP.h`, and `IPlugAPP_main.cpp` completely. `WM_DESTROY` closes the editor before resetting the singleton, but the plugin object remains alive while `CloseAudio()` runs; no additional plugin-state use-after-free was proved.
- Read the custom `VoLumIPlugAPPVectorAccumulator.h` and its test. `Reset()` allocates before `startStream()`, per-frame methods do not allocate, and producer/consumer rates stay balanced for non-divisible callback sizes. No accumulator corruption was found.
- Traced the full current ASIO mismatch from the real `settings.ini`: runtime opens FlexASIO for both directions; the independent-pair defect is specifically in state/dialog probing, not an attempt by RtAudio to open two ASIO drivers.
- Read RtAudio's ASIO open/start/stop/callback and DirectSound open/start/stop/close/callback paths, including the requested waits at lines 6325 and 6445. Host `abortStream()` makes DirectSound skip line 6445, but the mutex wait and line-6325 join remain unbounded as F-P10-1 documents.
- Read RtMidi WinMM input/output open/close/callback/error paths. SysEx callback data is copied into `SysExData` before RtMidi clears its vector, so no dangling SysEx payload was found.
- Read every remaining fork-modified graphics/logging/path file: multi-file dialogs on Windows/macOS, fractional wheel handling, macOS direction normalization, popup/tool-tip UTF conversion, and resize coalescing. VoLum calls `PromptForFiles()` with extension names such as `"nam"` (without a leading dot), matching the Windows filter builder.
- Read the latency working-tree patch and its VoLum caller. `GetIOBufferSize()` returns RtAudio's negotiated value and `GetStreamLatencyFrames()` guards closed/null streams; the defect is stale settings/UI state in F-P10-5, not those getters.
- Read both patch-application scripts and their README. They are idempotent for an already-applied patch and fail closed when neither forward nor reverse application checks succeed.
