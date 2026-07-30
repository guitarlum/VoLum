## 1. Truncated state tails still mutate the live rig

**Severity** HIGH

**Where** `NeuralAmpModeler/Unserialization.cpp:633-639, 645-700, 858-863`

**What** The new short-read rejection only validates the path/parameter prefix. For a known VoLum chunk, `_UnserializeApplyConfig(config)` mutates live parameters before the mandatory selection and legacy per-amp payload are read. Those later reads are not checked: `GetVoLumChunkSelection` can return `-1`, its default selection is still copied into the plug-in, `chunk.Size() - pos` becomes `chunk.Size() + 1`, and all per-amp decoders continue receiving a negative position. The legacy decoder also assigns its local default gate/EQ values after failed reads, so this is not a harmless rejected load. The headerless legacy path has the same omission more directly: `_GetConfigFrom_Earlier` may return `-1`, but `_UnserializeApplyConfig` is called unconditionally. Returning `-1` after these writes does not restore the previous rig, and the new init-complete recovery allows later idle/save work to preserve that partial state.

**How a user reaches it** Open a project whose otherwise recognizable VoLum state was truncated after the serialized parameters, or load a truncated pre-header NAM/VoLum chunk. The host is told the load failed, but the instance has already changed parameters, selected amp, and/or per-amp scenes; a later save can replace the user's good state with those partial/default values.

**Fix** Decode the complete chunk into temporary parameters, selection, per-amp settings, lock snapshots, and id tail. Check the mandatory selection plus legacy per-amp byte count before reading, check `pos` after every decoder, and only commit the temporary state after all required reads succeed. At minimum, reject `pos < 0` before `_UnserializeApplyConfig` in the unknown-version path and before every assignment/restore in the known-version tail.

## 2. State restore now writes IGraphics controls from the host state thread

**Severity** HIGH

**Where** `NeuralAmpModeler/Unserialization.cpp:849-853` (callee: `NeuralAmpModeler/VoLumAmpMenus.inc.cpp:283-319`)

**What** `_UnserializeStateWithKnownVersion` now calls `_VolumSyncUiFromState()` directly. A host state callback is not UI-thread-affine. The callee dereferences the live `IGraphics` tree and mutates the amp list, hero, labels, cab row, and channel stepper; its final `_VolumApplyFocusedLaneCabs()` can also rescan rig directories and mutate shared channel vectors. If the editor is drawing or handling input on the UI thread while the host restores state on another thread, these unsynchronized control and container writes are a data race and can crash.

**How a user reaches it** Load a project, undo a plug-in state change, or switch a host preset with the VoLum editor open in a host that invokes processor state restore off the UI thread.

**Fix** Replace the direct call with an atomic/pending UI-sync request. Consume it from `OnIdle` (or the framework's UI-thread message mechanism), then call `_VolumSyncUiFromState()` only on the editor thread after verifying that the same editor is still attached.

## 3. The loader result drain still allocates and blocks on the audio thread

**Severity** MEDIUM

**Where** `NeuralAmpModeler/VoLumLoader.inc.cpp:135-155, 159-236` (allocation documented at `NeuralAmpModeler/NeuralAmpModeler.h:215-225`)

**What** Moving diagnostic file I/O to the worker only fixes one realtime violation. `_VolumDrainLoaderResults()` runs inside `ProcessBlock`, but a result prepared for an old sample rate or block size calls `ResamplingNAM::Reset()` there; that reset resizes/prewarms the resampler and encapsulated model and is explicitly documented as allocating. The same drain also takes blocking `lock_guard`s on both loader and staging mutexes after its initial try-lock. Completed/superseded result destruction can free model memory on this thread as well.

**How a user reaches it** Change audio device, sample rate, or block size while a MAIN, SUPPORT, or PRE model load is in flight. The first audio callback that receives the stale result performs model reset/prewarm allocation; ordinary completed loads also enter the blocking mutex sections. This can produce a dropout or stalled callback during a live amp switch.

**Fix** Treat sample-rate/block-size as a load generation. If a result is stale, discard/requeue it to the worker instead of resetting it in `ProcessBlock`. Publish prepared models through a non-blocking handoff, and retire superseded/old models to a non-audio-thread destruction queue.

## 4. `mVolumSupportSelected` lags the selection it shadows

**Severity** MEDIUM

**Where** `NeuralAmpModeler/NeuralAmpModeler.cpp:592-598`; `NeuralAmpModeler/VoLumLoader.inc.cpp:438-526`

**What** `ProcessBlock` now gates the SUPPORT lane with `mVolumSupportSelected`, but that atomic is changed only when `OnIdle` eventually executes `_VolumRequestSupportModelLoad()`. The actual selection changes earlier on the UI/state path. In particular, selecting “(none)” sets `kSupportAmpIdx` to `-1` and queues `mVolumSupportNeedsLoad`, while the atomic remains `true` and the old `mSupportModel` remains present. The audio plan therefore continues running the deselected support amp until the next idle dispatch reaches lines 480-487.

**How a user reaches it** With Dual Amp active and a loaded factory or custom support amp, choose “(none)”, or restore a scene/preset with no support amp. The UI and parameter say no support amp, but one or more audio blocks can still contain the previous support model.

**Fix** Update the explicit selection atomic at the selection/state command boundary, not at loader dispatch: clear it immediately when the resolved support is none/orphaned, and set it immediately for a valid factory/custom selection. Keep loading/readiness in separate flags so `ProcessBlock` cannot confuse “selected” with “loader has handled the request.”

## 5. Preset menus read the process-global bank before claiming it

**Severity** MEDIUM

**Where** `NeuralAmpModeler/VoLumSettingsPresets.inc.cpp:27-31, 112-145` (missed entry point: `NeuralAmpModeler/VoLumAmpMenus.inc.cpp:28-43`)

**What** Save, overwrite, and recall now call `_VolumClaimPresetOps()`, but opening the preset menu reads `MockPresetsForAmp()` without claiming first. That API ignores its `ampIdx` argument and indexes the bank through the process-global `ActivePresetOwnerKey`. A different VoLum instance can therefore leave the key pointing at its own amp. The first instance displays that other bank; after the user chooses a row, `_VolumRecallPreset()` claims the first instance and applies the same numeric index to a different bank, yielding a wrong recall or a no-op.

**How a user reaches it** Open two VoLum instances with different preset banks, interact with instance B so it becomes the global owner, then open instance A's preset menu and choose a row.

**Fix** Claim at the start of every owner-keyed preset surface, including `_VolumShowPresetMenu` and Manage-panel list reload/validation. Preferably remove the global key from read APIs and pass `_VolumActiveOwnerKey()` explicitly so displaying and acting on a list cannot refer to different banks.
