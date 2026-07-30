## Summary

The current 1.2.x binary layout is mostly disciplined: parameter indices are append-only, the fixed per-amp blocks are written and read in the same order, and the additive ID tail covers the newer pitch, tremolo, delay-sync, custom-content, and lock-snapshot fields. The subsystem is nevertheless not release-safe. I found four BLOCKERs: hostile/corrupt string lengths can drive an out-of-bounds read, a malformed version can throw through the plugin ABI, custom-amp project state is actually restored from mutable process-global storage, and process-global preset callbacks can target a different or already-destroyed plugin instance.

The highest user-work risk is the custom-main scene design. The DAW chunk stores the custom amp's ID and raw live parameters, but not its `VoLumAmpSettings` scene; restoration selects the ID and then overwrites the just-deserialized parameters with the single scene in `GlobalContentStore`. Two tracks using the same custom amp therefore collapse to whichever track was serialized last, and moving the project to a machine without that registry entry falls back to unrelated factory state.

## Findings

### F-P2-1: BLOCKER — A corrupt string length can overflow `GetStr` and read outside the state chunk

**Where:** `NeuralAmpModeler/NeuralAmpModeler.cpp:1123-1128`; `NeuralAmpModeler/Unserialization.cpp:517-521,88-102`; callee contract at `iPlug2/IPlug/IPlugStructs.h:90-105`.

**Mechanism:** VoLum passes the host-provided chunk directly to `IByteChunk::GetStr` for the header, version, NAM path, and IR path. The iPlug2 callee reads a signed `int len`, computes `strStartPos + len` in signed `int`, and only checks the wrapped result against the upper bound. It neither rejects negative lengths nor prevents integer overflow. On the Windows/MSVC target, a sufficiently large positive length wraps `strEndPos` negative, passes `strEndPos <= srcSize`, and calls `WDL_String::Set` with that huge positive length:

```cpp
// NeuralAmpModeler/NeuralAmpModeler.cpp:1123-1128
int NeuralAmpModeler::UnserializeState(const IByteChunk& chunk, int startPos)
{
  WDL_String header;
  int pos = startPos;
  pos = chunk.GetStr(header, pos);
```

```cpp
// iPlug2/IPlug/IPlugStructs.h:90-104
static inline int GetStr(const uint8_t* pSrc, int srcSize, WDL_String& str, int startPos)
{
  int len;
  int strStartPos = GetBytes(pSrc, srcSize, &len, sizeof(len), startPos);
  if (strStartPos >= 0)
  {
    int strEndPos = strStartPos + len;
    if (strEndPos <= srcSize)
    {
      if (len > 0)
        str.Set((char*) (pSrc + strStartPos), len);
```

The same unsafe call is repeated for the version and persisted paths:

```cpp
// NeuralAmpModeler/Unserialization.cpp:91-101
int pos = startPos;
WDL_String path;
pos = chunk.GetStr(path, pos);
config["NAMPath"] = std::string(path.Get());
pos = chunk.GetStr(path, pos);
config["IRPath"] = std::string(path.Get());
```

**Trigger:** A DAW project or preset state has the correct VoLum header, followed by a length prefix such as `INT_MAX` for the version, NAM path, or IR path. The same issue is reachable in the first header field of a wholly garbage chunk.

**Impact:** Out-of-bounds read, excessive allocation, host crash, or process termination while opening a project. This is proved for the Win32/MSVC build's wrapping behavior; other compilers still encounter undefined signed overflow.

**Fix sketch:** Before every string read, validate the prefix using non-overflowing arithmetic: `len >= 0`, `start <= size`, and `len <= size - start`. Prefer fixing `IByteGetter::GetStr` upstream, but VoLum should still treat every negative read result as a hard parse failure. Put a practical maximum on version/path/tail string sizes.

**Proposed regression test:** `test_state_rejects_overflowing_string_lengths_without_read_or_allocation` — feed a valid header plus an `INT_MAX` version/path length under ASan; assert a clean failure, no exception, no allocation proportional to the declared length, and no state mutation.

### F-P2-2: BLOCKER — A missing or malformed version throws through the plugin state callback

**Where:** `NeuralAmpModeler/Unserialization.cpp:517-521`; `NeuralAmpModeler/VoLumChunkVersion.h:22-36`; VST3 boundary at `iPlug2/IPlug/VST3/IPlugVST3_Common.h:50-85`.

**Mechanism:** The known-header path constructs `ChunkVersion` without checking the preceding `GetStr` result and without a `try/catch`. `ChunkVersion` uses `std::stoi` and explicitly throws unless there are exactly three components:

```cpp
// NeuralAmpModeler/Unserialization.cpp:517-521
WDL_String wVersion;
pos = chunk.GetStr(wVersion, pos);
std::string versionStr(wVersion.Get());
volum::ChunkVersion version(versionStr);
```

```cpp
// NeuralAmpModeler/VoLumChunkVersion.h:22-36
explicit ChunkVersion(const std::string& versionStr)
{
  std::istringstream stream(versionStr);
  std::string token;
  std::vector<int> parts;

  while (std::getline(stream, token, '.'))
    parts.push_back(std::stoi(token));

  if (parts.size() != 3)
    throw std::invalid_argument("Version string must have exactly 3 dot-separated segments");
```

The VST3 adapter calls `UnserializeState` directly and has no exception barrier:

```cpp
// iPlug2/IPlug/VST3/IPlugVST3_Common.h:67-75
chunk.PutBytes(buffer, bytesRead);
}
int pos = pPlug->UnserializeState(chunk,0);

Steinberg::int32 savedBypass = 0;
pState->seek(pos,Steinberg::IBStream::IStreamSeekMode::kIBSeekSet);
```

**Trigger:** Load a chunk containing only the valid `"###NeuralAmpModeler###"` header, or a valid header followed by `""`, `"1.2"`, `"1.x.0"`, or an out-of-range decimal component.

**Impact:** An uncaught `std::invalid_argument`/`std::out_of_range` crosses a C++ plugin ABI callback. In normal hosts this terminates the plugin process or the host while opening the project.

**Fix sketch:** Check `pos` after reading the version; parse with a non-throwing, range-checked routine; reject malformed versions before touching live state. Keep an exception barrier at `UnserializeState` as defense in depth.

**Proposed regression test:** `test_state_valid_header_invalid_version_is_cleanly_rejected` — cover empty, short, nonnumeric, negative, and overflowing components; assert no throw, no state change, and a failure position/status.

### F-P2-3: MAJOR — Truncated reads partially overwrite live state before returning failure

**Where:** `NeuralAmpModeler/Unserialization.cpp:88-104,592-612,629-665,741-799,802-807`; `NeuralAmpModeler/VoLumChunkCodec.h:231-334`.

**Mechanism:** Every reader chains `pos = chunk.Get(...)`, but none stops when `pos` becomes `-1`. Locals in `_UnserializePathsAndExpectedKeys` are initialized to zero, so all parameters after the truncation point are inserted into `config` as zero and then applied to live `IParam`s before any tail validation:

```cpp
// NeuralAmpModeler/Unserialization.cpp:98-104
for (auto it = paramNames.begin(); it != paramNames.end(); ++it)
{
  double v = 0.0;
  pos = chunk.Get(&v, pos);
  config[*it] = v;
}
return pos;
```

```cpp
// NeuralAmpModeler/Unserialization.cpp:598-612
if (!(version >= volum::ChunkVersion(0, 9, 0)))
  volum::MigrateDelayReverbToV0_9_0(config);
// ...
_UnserializeApplyConfig(config);

// VoLum tail
volum::VoLumChunkSelection selection;
pos = volum::GetVoLumChunkSelection(chunk, pos, selection);
mVolumAmpIdx = selection.ampIdx;
```

The per-amp readers continue the same pattern for dozens of fields:

```cpp
// NeuralAmpModeler/VoLumChunkCodec.h:232-248
pos = chunk.Get(&s.speakerIdx, pos);
pos = chunk.Get(&s.channelIdx, pos);
pos = chunk.Get(&s.inputLevel, pos);
// ...
pos = chunk.Get(&ng, pos);
pos = chunk.Get(&eq, pos);
s.noiseGateActive = (ng != 0);
s.eqActive = (eq != 0);
```

Finally the method restores those partially decoded scenes to live DSP/UI state and returns `pos`, even when it is `-1`:

```cpp
// NeuralAmpModeler/Unserialization.cpp:741-799
mVolumInitComplete = false;
_VolumRestoreFromSettings(mVolumAmpIdx);
_VolumApplyLiveLockSnapshots();
// ...
return pos;
```

**Trigger:** Truncate a valid 1.0.x, 1.1.x, 1.2.0, or 1.2.1 state at any path, parameter, per-amp, lock, or ID-tail boundary. A zero-length chunk also enters the unknown-version reader and applies its zero-filled config.

**Impact:** The plugin can be left half-restored: earlier values come from the project, later values become zero/default or retain constructor/global values. VST3 may subsequently report state failure when it cannot seek/read bypass, but the instance has already been mutated. This is silent wrong sound rather than an atomic rejection.

**Fix sketch:** Decode into a detached scratch state. After every read, stop on `pos < 0`; validate the complete expected layout and tail; only then commit parameters, scenes, selection, locks, paths, and content references in one step.

**Proposed regression test:** `test_every_chunk_truncation_offset_is_atomic` — for each byte offset of a known-good 1.2.1 chunk, truncate there and assert either full successful equality or clean failure with every live parameter/member unchanged.

### F-P2-4: BLOCKER — Custom-main project states collapse into one mutable global scene

**Where:** `NeuralAmpModeler/NeuralAmpModeler.cpp:1011-1059`; `NeuralAmpModeler/VoLumSettingsLocks.inc.cpp:83-95`; `NeuralAmpModeler/VoLumSceneRig.inc.cpp:306-329`; `NeuralAmpModeler/Unserialization.cpp:748-774`.

**Mechanism:** When a custom main amp is focused, `_VolumSaveCurrentToSettings` redirects the live snapshot away from the per-instance `mVolumAmpSettings` array into `GlobalContentStore().reg().customScenes[id]`:

```cpp
// NeuralAmpModeler/VoLumSettingsLocks.inc.cpp:83-95
volum::VoLumAmpSettings* target = &mVolumAmpSettings[mVolumAmpIdx];
if (mVolumCustomMainIdx >= 0)
{
  const std::string id = volum::custom::CustomAmpIdAt(mVolumCustomMainIdx);
  if (!id.empty())
    target = &volum::content::GlobalContentStore().reg().customScenes[id];
}
auto& s = *target;
```

`SerializeState` explicitly documents that only the ID, not that scene, enters the chunk. Although `SerializeParams` contains the current live numeric parameters, the custom scene is external:

```cpp
// NeuralAmpModeler/NeuralAmpModeler.cpp:1023-1032
const_cast<NeuralAmpModeler*>(this)->_VolumSaveCurrentToSettings();
// A focused custom amp keeps its scene in the shared content library (only
// its id lives in the chunk) ...
if (mVolumCustomMainIdx >= 0)
  volum::content::GlobalContentStore().Save();
```

```cpp
// NeuralAmpModeler/NeuralAmpModeler.cpp:1052-1058
// VoLum 1.2.0 id tail: project references into the shared content library
volum::ChunkIdTail idTail;
idTail.customMainId = volum::custom::CustomAmpIdAt(mVolumCustomMainIdx);
```

On restore, selecting that ID applies the one mutable global scene and overwrites the raw parameters just decoded from the project:

```cpp
// NeuralAmpModeler/VoLumSceneRig.inc.cpp:321-329
const std::string ampId = volum::custom::CustomAmpIdAt(customIdx);
if (!ampId.empty())
  _VolumApplyAmpSettings(volum::content::GlobalContentStore().reg().customScenes[ampId]);
```

```cpp
// NeuralAmpModeler/Unserialization.cpp:772-774
const int cmi = volum::custom::CustomAmpIndexById(idTail.customMainId);
if (cmi >= 0)
  _VolumSelectCustomAmp(cmi); // applies the custom scene + cabs + .nam load
```

**Trigger:** Put the same custom amp on two DAW tracks, set different tones/effects, and save the project. Host serialization of the second instance overwrites `customScenes[id]`. Reopen the project. Alternatively, reopen after editing that custom amp's global scene elsewhere, or move the project to a machine where the custom ID is absent.

**Impact:** All instances using that custom amp restore the last globally saved scene rather than their own project snapshot. If the ID is absent, the base factory scene applied earlier remains, again discarding the saved custom sound. This is deterministic loss of per-track/project work.

**Fix sketch:** Store the active custom-main `VoLumAmpSettings` snapshot in the DAW chunk (including all ID-tail-only fields), and treat the content-store ID as asset identity only. On restore, resolve the asset/model by ID but apply the chunk's embedded scene. Do not write project-instance edits back into a shared scene during host serialization.

**Proposed regression test:** `test_two_instances_same_custom_amp_restore_independent_scenes` — serialize instances A and B with the same custom ID but different values, in both save orders; deserialize both and assert each equals its own source. Repeat with the registry scene changed and with the ID absent.

### F-P2-5: BLOCKER — Process-global preset hooks call the wrong or a destroyed plugin instance

**Where:** `NeuralAmpModeler/VoLumCustomContentApi.h:441-470,484-532`; `NeuralAmpModeler/VoLumSettingsPresets.inc.cpp:4-13,95-128`; `NeuralAmpModeler/NeuralAmpModeler.cpp:540-556`.

**Mechanism:** The preset owner, capture callback, and apply callback are function-local statics shared by every plugin instance in the host process:

```cpp
// NeuralAmpModeler/VoLumCustomContentApi.h:447-470
inline std::string& ActivePresetOwnerKey()
{
  static std::string key = content::FactoryOwnerKey(0);
  return key;
}
// ...
inline PresetSettingsCapture& PresetCaptureHook()
{
  static PresetSettingsCapture h;
  return h;
}
inline PresetSettingsApply& PresetApplyHook()
{
  static PresetSettingsApply h;
  return h;
}
```

Every constructor overwrites the two hooks with lambdas capturing its own `this`:

```cpp
// NeuralAmpModeler/VoLumSettingsPresets.inc.cpp:4-13
volum::custom::PresetCaptureHook() = [this]() -> volum::VoLumAmpSettings {
  _VolumSaveCurrentToSettings();
  return _VolumActiveScene();
};
volum::custom::PresetApplyHook() = [this](const volum::VoLumAmpSettings& s) { _VolumApplyRecalledPreset(s); };
```

Preset operations invoke whichever instance installed the global hook last:

```cpp
// NeuralAmpModeler/VoLumCustomContentApi.h:504-505,531-532
if (PresetCaptureHook())
  pr.settings = PresetCaptureHook()();
// ...
if (PresetApplyHook())
  PresetApplyHook()(it->second[(size_t)idx].settings);
```

The destructor neither unregisters nor rebinds these callbacks:

```cpp
// NeuralAmpModeler/NeuralAmpModeler.cpp:540-555
NeuralAmpModeler::~NeuralAmpModeler()
{
  _VolumStopLoader();
  _VolumSaveCurrentToSettings();
  // ...
  _DeallocateIOPointers();
}
```

**Trigger:** Create plugin instance A, then B. Use Save As, Overwrite, or Recall in A: the hook runs against B. For the crash case, destroy B while A remains, then use a preset in A; the global lambda dereferences B's freed `this`.

**Impact:** Saving a preset from one track captures another track's sound; recall changes the wrong track; after destruction this is a use-after-free with crash or memory corruption potential.

**Fix sketch:** Remove the global hooks/owner. Pass an instance-bound capture/apply function or `VoLumAmpSettings` directly through each UI callback. If a shared registry API must remain, give operations an explicit instance/context argument and never store raw-`this` process-global callbacks.

**Proposed regression test:** `test_preset_operations_are_instance_local_and_survive_peer_destruction` — create A/B with distinct states, save and recall through A and assert B is unchanged; destroy B and repeat under ASan.

### F-P2-6: MAJOR — 1.0.x/1.1.x chunks inherit machine-global custom selections and IDs

**Where:** constructor at `NeuralAmpModeler/NeuralAmpModeler.cpp:498-516`; global settings load at `NeuralAmpModeler/VoLumSettingsScene.inc.cpp:422-473`; old-chunk path at `NeuralAmpModeler/Unserialization.cpp:629-739,741-752`; ID JSON fields at `NeuralAmpModeler/VoLumAmpSettingsJson.h:61-89`.

**Mechanism:** Every plugin instance loads `volum-settings.json` before host state, populating the factory scene array and deferred custom-main/preset IDs:

```cpp
// NeuralAmpModeler/NeuralAmpModeler.cpp:511-516
_VolumInstallPresetHooks();
_VolumSyncPresetOwner();
_VolumLoadSettingsFromFile();
_VolumRestoreFromSettings(mVolumAmpIdx);
```

```cpp
// NeuralAmpModeler/VoLumSettingsScene.inc.cpp:457-473
volum::VolumUserSettingsFromJson(j, mVolumAmpSettings.data(), volum::kAmpCount, &mVolumAmpIdx,
                                 /* ... */);
// ...
if (j.contains("volumCustomMainId") && j["volumCustomMainId"].is_string())
  mVolumRestoreCustomMainId = j["volumCustomMainId"].get<std::string>();
if (j.contains("volumActivePresetId") && j["volumActivePresetId"].is_string())
  mVolumRestorePresetId = j["volumActivePresetId"].get<std::string>();
```

`AmpSettingsFromJson` includes custom IR IDs, but the legacy binary readers overwrite only numeric/bool fields. For <=1.1.0, `TryGetChunkIdTail` correctly returns false; however there is no `else` that clears `activeIrId`, `supportActiveIrId`, `supportCustomId`, `mVolumRestoreCustomMainId`, or `mVolumRestorePresetId`. Only the `if (haveIdTail)` body replaces them:

```cpp
// NeuralAmpModeler/Unserialization.cpp:667-675,724-739
volum::ChunkIdTail idTail;
int idTailPos = pos;
const bool haveIdTail = volum::TryGetChunkIdTail(chunk, pos, chunk.Size(), idTail, &idTailPos);
if (haveIdTail)
{
  // ...
  mVolumAmpSettings[i].activeIrId = idTail.perAmpIrId[i];
  mVolumAmpSettings[i].supportActiveIrId = idTail.perAmpSupportIrId[i];
  mVolumAmpSettings[i].supportCustomId = idTail.perAmpSupportId[i];
  // ...
}
```

The contaminated scene is then applied:

```cpp
// NeuralAmpModeler/Unserialization.cpp:741-743
mVolumInitComplete = false;
_VolumRestoreFromSettings(mVolumAmpIdx);
_VolumApplyLiveLockSnapshots();
```

**Trigger:** Use the 1.2.1 standalone app so `volum-settings.json` contains a focused custom amp, custom IR, custom support, or preset. Then open a DAW project saved by 1.0.0, 1.0.1, or 1.1.0, whose chunk legitimately has no 1.2 ID tail.

**Impact:** The old project can load a custom IR/support amp it never referenced; on editor open it can re-focus the machine-global custom main/preset. Results depend on the current machine's unrelated standalone history, so the same project restores differently across machines.

**Fix sketch:** At the start of host-state restoration, clear all fields not representable by the source chunk version to version-defined neutral defaults. In plugin formats, do not use `volum-settings.json` as the baseline for project-owned scenes/selections.

**Proposed regression test:** `test_1_0_and_1_1_chunks_ignore_global_1_2_content_refs` — seed every global custom ID and deferred selection, load golden 1.0.0/1.0.1/1.1.0 chunks, and assert all unsupported IDs are empty and the project-selected factory amp/IR is unchanged.

### F-P2-7: MAJOR — Legacy delay/reverb values are parsed, then overwritten with current defaults

**Where:** `NeuralAmpModeler/Unserialization.cpp:543-604,641-646,741-743`; `NeuralAmpModeler/VoLumSettingsScene.inc.cpp:76-117,141-172,189-197,290-291`.

**Mechanism:** Pre-per-amp-POST releases serialized delay/reverb as ordinary global parameters. The version-specific reader extracts and migrates them, and `_UnserializeApplyConfig` applies them at line 604. When no per-amp POST block exists, the reader intentionally leaves `postValid == false`:

```cpp
// NeuralAmpModeler/Unserialization.cpp:641-646
if (hasPostPerAmpSettings)
  pos = volum::GetPostPerAmpSettings(chunk, pos, s, hasPostSnapshotPerAmpSettings);
// hasPostPerAmpSettings == false: legacy chunk pre-dating per-amp POST. Per
// user policy "we don't have to migrate, we can reset", postValid stays at the
// struct default (false), so _VolumRestoreFromSettings initializes a meaningful
// factory POST scene ...
```

The later scene restore sees `postValid == false`, replaces every POST value with current struct defaults, and writes those defaults back into the live parameters:

```cpp
// NeuralAmpModeler/VoLumSettingsScene.inc.cpp:76-98
if (!s.postValid)
{
  const volum::VoLumAmpSettings defaults;
  s.postValid = true;
  s.postDelayActive = defaults.postDelayActive;
  s.postDelayTime = defaults.postDelayTime;
  // ...
  s.postReverbActive = defaults.postReverbActive;
  s.postReverbMix = defaults.postReverbMix;
  // ...
}
```

```cpp
// NeuralAmpModeler/VoLumSettingsScene.inc.cpp:146-163,290-291
setParam(kDelayActive, s.postDelayActive ? 1.0 : 0.0);
// ...
setParam(kReverbSubMode, s.postReverbSubMode);
// ...
if (!mVolumPostLocked)
  _VolumRestorePostFromSlot(s);
```

**Trigger:** Open a legacy VoLum project from before the per-amp POST tail (for example v0.8.2) with Delay or Reverb enabled and nondefault settings.

**Impact:** The project reopens with its saved delay/reverb sound replaced by current defaults, often disabling the effects entirely. This is a migration defect, not a voicing recommendation.

**Fix sketch:** For a chunk without per-amp POST data, seed the selected amp's POST scene from the already-migrated live `config` values before `_VolumRestoreFromSettings`, mark it valid, and initialize only fields that truly did not exist in that source version.

**Proposed regression test:** `test_v0_8_2_live_post_parameters_survive_migration` — load a golden v0.8.2 chunk with Delay/Reverb active and distinctive values; assert the final live params (after per-amp restore) equal the migrated serialized values, not `VoLumAmpSettings` defaults.

### F-P2-8: MAJOR — VST3 bypass bytes make legacy per-amp chunks look extended

**Where:** `NeuralAmpModeler/VoLumChunkLayout.h:8-15,44-56`; `NeuralAmpModeler/Unserialization.cpp:617-635`; VST3 adapter at `iPlug2/IPlug/VST3/IPlugVST3_Common.h:32-44,54-73`.

**Mechanism:** `ChunkHasExtendedPerAmpSettings` treats *any* bytes beyond the exact legacy payload as proof that every amp has an extended PRE block:

```cpp
// NeuralAmpModeler/VoLumChunkLayout.h:11-13,44-56
static constexpr int kLegacyPerAmpSettingsBytes = static_cast<int>(sizeof(int) * 4 + sizeof(double) * 6);
static constexpr int kPrePedalPerAmpSettingsBytes = static_cast<int>(sizeof(int) * 9 + sizeof(double) * 24);

inline int LegacyPerAmpSettingsPayloadBytes(int ampCount)
{
  return kLegacyPerAmpSettingsBytes * ampCount;
}

inline bool ChunkHasExtendedPerAmpSettings(int remainingBytes, int ampCount)
{
  return remainingBytes > LegacyPerAmpSettingsPayloadBytes(ampCount);
}
```

The VST3 wrapper writes its bypass integer immediately after the plugin chunk, then reads the entire stream—including that integer—into the `IByteChunk` passed to VoLum:

```cpp
// iPlug2/IPlug/VST3/IPlugVST3_Common.h:32-44
pState->write(chunk.GetData(), chunk.Size());
// ...
Steinberg::int32 toSaveBypass = pPlug->GetBypassed() ? 1 : 0;
pState->write(&toSaveBypass, sizeof (Steinberg::int32));
```

```cpp
// iPlug2/IPlug/VST3/IPlugVST3_Common.h:59-69
pState->read(buffer, (Steinberg::int32) bytesPerBlock, &bytesRead);
// ...
chunk.PutBytes(buffer, bytesRead);
// ...
int pos = pPlug->UnserializeState(chunk,0);
```

Therefore an old exact-legacy payload is seen as `legacyBytes * ampCount + 4`, the `>` test returns true, and the reader consumes a nonexistent extended block after every amp:

```cpp
// NeuralAmpModeler/Unserialization.cpp:617-635
const int remainingPerAmpBytes = chunk.Size() - pos;
const bool hasPreAmpSettings =
  volum::ChunkHasExtendedPerAmpSettings(remainingPerAmpBytes, volum::kAmpCount);
// ...
pos = volum::GetLegacyPerAmpSettings(chunk, pos, s);
if (hasPreAmpSettings)
  pos = volum::GetExtendedPerAmpSettings(chunk, pos, s, version >= volum::ChunkVersion(0, 8, 1));
```

**Trigger:** In VST3, load an early VoLum state whose per-amp tail is exactly the legacy core block (VoLum 0.1.x-0.4.x or equivalent 0.7.15-era layout). AU does not add this VST3 bypass word and does not hit this specific false positive.

**Impact:** Reads become misaligned on the first amp, consume following amps as PRE fields, eventually return failure, and leave corrupted/partial scene state. Old VST3 projects fail to restore while the same logical chunk can work in another format.

**Fix sketch:** Layout detection must use exact version/layout metadata, not “more bytes than legacy.” Exclude the API wrapper's known trailing bytes and require the full version-appropriate payload (the pre-0.8.1 extended layout is smaller than the 0.8.1+ `kPrePedalPerAmpSettingsBytes` layout) before entering that reader.

**Proposed regression test:** `test_legacy_vst3_chunk_with_bypass_is_not_extended` — append a four-byte bypass word to a golden exact-legacy chunk; assert `hasPreAmpSettings == false`, the returned position lands immediately before bypass, and all legacy per-amp values restore.

### F-P2-9: MAJOR — A newer chunk is parsed with the current parameter count, corrupting all following fields

**Where:** `NeuralAmpModeler/Unserialization.cpp:525-541,606-625`; `NeuralAmpModeler/VoLumParams.h:19-137`.

**Mechanism:** The branch intended for the 1.2 layout is open-ended (`version >= 1.2.0`) and consumes this build's `kNumParams`, not the writer version's count:

```cpp
// NeuralAmpModeler/Unserialization.cpp:525-541
if (version >= volum::ChunkVersion(1, 2, 0))
{
  std::vector<std::string> paramNames;
  paramNames.reserve(kNumParams);
  for (int i = 0; i < kNumParams; ++i)
    paramNames.push_back(GetParam(i)->GetName());
  pos = _UnserializePathsAndExpectedKeys(chunk, pos, config, paramNames);
}
```

The parameter enum's own contract is to append new parameters immediately before `kNumParams`:

```cpp
// NeuralAmpModeler/VoLumParams.h:19-22,135-137
// EParams order is serialization-sensitive: never reorder or renumber existing entries.
// Append new params immediately before kNumParams.
enum EParams
{
  // ...
  kNumParams
};
```

If 1.3 appends even one parameter, 1.2.1 stops eight bytes early and treats that extra parameter's `double` bytes as `VoLumChunkSelection`; every per-amp/tail read is then shifted.

**Trigger:** Load state written by any future version that follows the documented append-only parameter policy and adds one or more EParams.

**Impact:** The older build does not cleanly reject unsupported state. It applies the known prefix, misreads the selection/scenes/locks/ID tail, and can return failure only after partially mutating the instance. Forward compatibility is therefore corrupting rather than bounded.

**Fix sketch:** Store an explicit parameter count and section lengths in the chunk, or use an exact reader branch per schema version. A build that cannot determine the writer's count must reject versions newer than its maximum supported version before applying anything.

**Proposed regression test:** `test_future_chunk_with_appended_param_is_rejected_atomically` — synthesize version `1.3.0` with one extra serialized double before an otherwise valid current tail; assert clean rejection and no live-state changes (or, after a length-delimited format exists, correct skipping and alignment).

## Voicing observations (report only)

No tone-only changes are recommended. F-P2-7 concerns saved user values being replaced during migration and is therefore a state-restoration defect, not a request to alter the intended Delay/Reverb voicing or defaults.

## Areas read and found clean

- `NeuralAmpModeler/Unserialization.cpp`: read in full. Apart from the findings above, the explicit historical parameter-name ladders and the 0.9.0/0.9.1/0.9.3 value migrations are internally ordered consistently.
- `NeuralAmpModeler/VoLumChunkCodec.h`: read in full. Current writer/reader field order matches for legacy core, extended PRE, dual amp, support polarity, live POST, POST mode snapshots, lock flags, and lock snapshots.
- `NeuralAmpModeler/VoLumChunkIdTail.h`: read in full. For valid current JSON, sentinel probing, JSON conversion, fixed per-amp array sizing, and pitch/tremolo/delay tail symmetry are consistent.
- `NeuralAmpModeler/VoLumChunkLayout.h`: read in full. All fixed byte constants match the corresponding current codec fields; the extended-layout predicate exception is F-P2-8.
- `NeuralAmpModeler/VoLumChunkVersion.h`: read in full. Semantic comparison and the historical layout predicates are consistent for valid three-integer versions; malformed input handling is F-P2-2.
- `NeuralAmpModeler/VoLumParams.h`: read in full and cross-checked against raw parameter serialization. Existing indices remain append-only; no insertion/reordering defect was found on `release/1.2.1`.
- `NeuralAmpModeler/VoLumJsonMigration.h`: read in full. Key renames, DelayMode remap, Oktaverb submode remap, and equal-power ReverbMix migration are bounded to their intended source versions.
- `NeuralAmpModeler/VoLumAmpSettingsJson.h`: read in full. The JSON codec covers core, PRE, dual/support, POST, main/support custom IR IDs, and custom support identity/cab/channel symmetrically.
- `NeuralAmpModeler/VoLumOutputMode.h`: read in full. Persisted output-mode integer bounds and gain-selection helpers showed no state round-trip defect.
- `NeuralAmpModeler/VoLumDualAmpPlan.h`: read in full. Route/pan clamping and derived latency/pan planning do not persist hidden state and showed no serialization asymmetry.
- `NeuralAmpModeler/VoLumPrePostLock.h`: read in full. Dirty comparisons cover the serialized PRE/POST blocks, including mode snapshots, pitch, tremolo, and delay sync/division.
- `NeuralAmpModeler/NeuralAmpModeler.cpp`: serialization/deserialization entry points, constructor load order, session restore, and destructor were read. Current factory-scene and lock-snapshot write order matches the current reader.
- `NeuralAmpModeler/NeuralAmpModeler.h`: parameter/state member declarations were read and cross-checked against codecs. No duplicate parameter enum or conflicting parameter-count declaration was found.
- `NeuralAmpModeler/VoLumSettingsLocks.inc.cpp`: state-related sections were read in full. Unlocked PRE/POST scenes and dedicated live-lock snapshots are deliberately separated and current chunks preserve that distinction.
- Supporting call sites followed to prove mechanisms: `VoLumSettingsScene.inc.cpp`, `VoLumSettingsPresets.inc.cpp`, `VoLumSceneRig.inc.cpp`, `VoLumCustomContentApi.h`, and the iPlug2 `IByteChunk`/VST3 state adapter. Vendored internals were not audited beyond the exact behavior reached by VoLum's state calls.
