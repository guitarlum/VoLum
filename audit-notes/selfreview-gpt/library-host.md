## 1. A second spelling of the same payload bypasses the “still referenced” delete guard

**Severity** HIGH

**Where** `NeuralAmpModeler/VoLumContentStore.h:1032-1059`

**What** `RegistryReferences()` compares registry strings byte-for-byte, but `FlushPendingFileDeletes()` deletes the resolved filesystem object. On Windows, `ir/foo.wav`, `ir\foo.wav`, and case variants can resolve to the same file while comparing unequal. If two surviving/mangled registry entries alias one payload and one is deleted, the guard misses the other reference and removes the payload that the just-written registry still names.

**How a user reaches it** A synced, restored, or hand-merged `volum-content.json` contains two entries that refer to the same Windows file using different separator or case spelling. The user deletes one entry. The registry save succeeds, `RegistryReferences()` does not match the surviving spelling, and the shared `.wav`/`.nam` is deleted.

**Fix** Compare resolved file identity, not stored text: resolve every candidate through `ResolveStored()` and use normalized, platform-aware paths (or `std::filesystem::equivalent` for existing payloads). Also normalize stored paths on load and reject duplicate physical payload references.

## 2. Write failures are still not tied to the operation that caused them

**Severity** MEDIUM

**Where** `NeuralAmpModeler/VoLumContentStore.h:828-856`; `NeuralAmpModeler/VoLumCustomContentApi.h:213-220,334-343`; `NeuralAmpModeler/VoLumCustomOverlay.h:1200-1202,1226-1228`

**What** The new global `mLastWriteFailed` bit is cleared by any successful `Save()`, but custom-amp deletion and both IR-shaping edit paths never call `TakeWriteFailure()`. Their real failure can therefore be silently cleared by a later save from any plugin instance. Conversely, a later unrelated dialog can consume a stale failure. The “exactly once, to the right dialog” contract is not met.

**How a user reaches it** With the registry temporarily unwritable, the user deletes a custom amp or changes an IR’s level/cut. The UI updates but shows no save warning. Another instance then serializes state or performs successful library CRUD, clearing `mLastWriteFailed`; the original change disappears on reload with no report.

**Fix** Return a per-operation result from every mutator (`RemoveCustomAmp`, `SetIRShaping`, rename/delete/preset operations) and handle it synchronously in the initiating UI. Remove the process-global consumable failure bit, or replace it with operation IDs that cannot be cleared by unrelated saves.

## 3. A failed payload removal is forgotten permanently

**Severity** LOW

**Where** `NeuralAmpModeler/VoLumContentStore.h:1049-1061`

**What** `FlushPendingFileDeletes()` ignores the `std::filesystem::remove()` error and unconditionally clears the entire queue. Once the registry no longer references the item, a sharing violation, read-only file, or transient antivirus lock leaves an orphan that is never retried.

**How a user reaches it** The user deletes an imported IR, pedal, or amp while another process briefly has its payload open. The registry save succeeds, removal fails, and later saves have no record of the orphan.

**Fix** Keep failed paths in a replacement pending vector and retry them after later successful saves; treat “already absent” as success. Persist a cleanup journal if retries must survive process exit.

## 4. The shutdown watchdog is disarmed before the MIDI input port is closed

**Severity** HIGH

**Where** `iPlug2/IPlug/APP/IPlugAPP_host.cpp:54-64`; `iPlug2/IPlug/APP/IPlugAPP_host.h:250-253`

**What** The destructor only calls `mMidiIn->cancelCallback()`, then disarms the watchdog. It never calls `mMidiIn->closePort()`. Because `mMidiIn` is a `unique_ptr` member, its backend is destroyed—and its open port closed—after the destructor body, outside the watchdog. A blocking MIDI-input teardown can therefore still leave the standalone process alive with no window.

**How a user reaches it** The standalone has an open MIDI input whose driver wedges or device disappears during exit. Audio and MIDI output close, the watchdog is disarmed, and member destruction then blocks while closing the still-open input.

**Fix** Call `mMidiIn->closePort()` while armed and reset the MIDI objects before disarming so their backend destructors are covered too. Catch teardown exceptions inside the guarded scope.

## 5. Retrying a busy MIDI device in Preferences can persist “off”

**Severity** MEDIUM

**Where** `iPlug2/IPlug/APP/IPlugAPP_host.cpp:576-590`; `iPlug2/IPlug/APP/IPlugAPP_dialog.cpp:346-350,519-534,668-674`

**What** On an interactive open failure, `SelectMIDIDevice()` changes `mState` to `off`, while the combo remains visually selected on the requested device. Clicking OK later writes `mState` to `settings.ini`, so the supposedly session-only fallback becomes permanent. A user cannot save a desired device for the next launch while that device is currently busy.

**How a user reaches it** Startup falls back to MIDI off because a DAW owns the port. In Preferences the user selects that still-busy port and clicks OK, believing the visible selection will be retried next launch. The failed open changed the hidden state to off, and OK persists off.

**Fix** Separate desired/persisted MIDI selection from active-session port state. On open failure keep the desired name, show an error/status, and only mark the runtime port inactive; make the combo reflect whichever state is actually persisted.

## 6. Switching to ASIO still destroys the saved DirectSound input choice

**Severity** MEDIUM

**Where** `iPlug2/IPlug/APP/IPlugAPP_host.cpp:528-537`; `iPlug2/IPlug/APP/IPlugAPP_dialog.cpp:371-407`

**What** The final ASIO fix deliberately stopped rewriting `mAudioInDev` so it could retain the DirectSound/CoreAudio input, but the driver-change handler still overwrites that same field with the first input device of every newly selected backend. The preservation claimed by the host code therefore does not survive an ordinary Preferences switch.

**How a user reaches it** The user selects a non-default DirectSound input, switches to ASIO, then switches back. Line 398 stored the first ASIO input in the shared field; switching back stores the first DirectSound input, so the original choice is gone.

**Fix** Keep per-backend device selections (at minimum separate ASIO and DirectSound input/output state), or cache and restore the previous backend’s values instead of assigning element zero on every driver change.

## 7. Short malformed UTF-8 bypasses the new validator

**Severity** LOW

**Where** `NeuralAmpModeler/VoLumCustomModel.h:140-152,569-576`

**What** `Utf8Prefix()` validates every sequence, but `ClampName()` returns the original string unchanged whenever its byte length is already within the cap. A malformed short name therefore reaches JSON serialization; `WriteJsonAtomically()` catches the exception, but the user’s otherwise valid operation fails instead of the persistence-edge clamp sanitizing it.

**How a user reaches it** A filename or external text source supplies a short ill-formed UTF-8 display name. Import/capture preparation accepts it because it is under 24/28 bytes, then the registry save fails with invalid argument.

**Fix** Always scan with `Utf8Prefix(s, maxBytes)` (or first validate the whole string), even when `s.size() <= maxBytes`; reject or sanitize malformed input before mutating the registry.

## 8. Interrupted atomic writes leave permanent temp files

**Severity** LOW

**Where** `NeuralAmpModeler/VoLumSettingsFileIO.h:105-129`

**What** Controlled write and replace failures remove the temporary file, but a process crash or kill after line 107 and before line 124 leaves `*.tmp.<ticks>.<thread>` indefinitely. No later write/load reaps stale temps.

**How a user reaches it** VoLum or its host is terminated while a settings or content-registry temp is being written. The durable target remains intact, but each interrupted write can leave another orphan beside it.

**Fix** On the next write or startup, remove stale temp files matching only that target’s generated suffix and old enough not to belong to a concurrent writer; alternatively maintain an owned temp-file guard plus startup cleanup.
