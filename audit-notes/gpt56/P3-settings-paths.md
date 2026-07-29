## Summary

This subsystem has good single-process field validation and uses temp-file replacement correctly enough to prevent partial JSON at one target, but it is not release-safe under the host conditions VoLum explicitly supports. The highest-severity finding is that startup performs throwing filesystem probes outside its exception boundary; a disconnected redirected profile, UNC path, or macOS volume can throw out of the plugin constructor and make the plugin fail to instantiate. I also proved multiple data-loss paths: the all-format content registry and calibration/settings writers have no cross-process transaction, content-save failures are reported as successful, and the split main/dual settings files can be torn. Windows user-data paths are built from narrow environment strings, so non-ANSI profile names can break settings, content, and diagnostics together.

All findings below are proved from code paths; no suspicion-only findings are included.

## Findings

### F-P3-1: BLOCKER — Throwing startup filesystem probes can abort plugin instantiation

**Where:** `NeuralAmpModeler/VoLumSettingsScene.inc.cpp:422-443`; `NeuralAmpModeler/VoLumPaths.h:102-116`; constructor call sites `NeuralAmpModeler/NeuralAmpModeler.cpp:463-464,515`.

**Evidence:**

```cpp
// VoLumSettingsScene.inc.cpp:422-443
void NeuralAmpModeler::_VolumLoadSettingsFromFile()
{
  namespace fs = std::filesystem;
  const fs::path userPath = volum::VolumUserSettingsFilePath();
  // ...
  fs::path settingsPath;
  if (!userPath.empty() && fs::exists(userPath))
    settingsPath = userPath;
  else if (!legacyPath.empty() && fs::exists(legacyPath))
    settingsPath = legacyPath;
  else
    return;

  try
  {
```

```cpp
// VoLumPaths.h:102-116
#elif defined(__APPLE__)
  {
    fs::path modulePath;
    Dl_info info{};
    if (dladdr(reinterpret_cast<const void*>(&FindRigsRootDirectory), &info) != 0 && info.dli_fname)
    {
      modulePath = fs::weakly_canonical(fs::path(info.dli_fname));
    }
    // ...
      if (_NSGetExecutablePath(buf, &bufSize) == 0)
        modulePath = fs::weakly_canonical(fs::path(buf));
```

**Mechanism:** The throwing overloads of `fs::exists` run before the `try` block. The macOS `weakly_canonical` calls also use throwing overloads, and `FindRigsRootDirectory()` is called directly from the constructor. A filesystem error therefore propagates out of construction.

**Trigger:** Start a VST3/standalone with `%LOCALAPPDATA%` redirected to a disconnected UNC share or inaccessible directory; on macOS, instantiate while the module path is on an unavailable/disconnected volume or traversal is denied.

**Impact:** The standalone can terminate during startup, or a DAW can reject/disable the plugin during scan or instantiation.

**Fix sketch:** Use `error_code` overloads for every startup probe/canonicalization, keep the entire resolution path inside a no-throw boundary, log the error, and continue to the next candidate or an explicit no-settings state.

**Proposed regression test:** `startup_path_probe_errors_are_nonfatal` — inject a resolver/probe that returns `permission_denied`/`network_unreachable` and assert construction completes and the resolver returns an empty/fallback path without throwing.

### F-P3-2: MAJOR — Windows non-ANSI profile names break every VoLum-owned data path

**Where:** `NeuralAmpModeler/VoLumPaths.h:201-208,226-233,247-254,267-274`.

**Evidence:**

```cpp
// VoLumPaths.h:201-208
inline std::filesystem::path VolumUserSettingsFilePath()
{
  namespace fs = std::filesystem;
#ifdef _WIN32
  const char* la = std::getenv("LOCALAPPDATA");
  if (!la || !*la)
    return {};
  return fs::path(la) / "VoLum" / "volum-settings.json";
```

The same narrow conversion is repeated for `content`, `volum-dual-amp-settings.json`, and `volum.log`.

**Mechanism:** On Windows, constructing `std::filesystem::path` from a narrow `char*` interprets bytes through the active ANSI code page. `getenv("LOCALAPPDATA")` is also the narrow environment view. Characters not representable in that code page are lost/misconverted before filesystem access.

**Trigger:** Run under a Windows account whose profile contains Cyrillic, CJK, or other characters outside the active ANSI code page (or change the system locale away from the profile-name script).

**Impact:** VoLum resolves a nonexistent/wrong directory. Settings do not load or persist, the custom amp/IR/pedal library appears empty or cannot save, and the diagnostic log is absent.

**Fix sketch:** Resolve Local AppData with `SHGetKnownFolderPath(FOLDERID_LocalAppData)` or `_wgetenv(L"LOCALAPPDATA")` and construct from `wchar_t`. Do not pass the narrow environment value to `u8path`; those bytes are not guaranteed to be UTF-8.

**Proposed regression test:** `windows_user_data_paths_preserve_unicode_profile` — inject a wide Local AppData path containing Cyrillic+CJK characters and assert all four resolved paths exactly preserve it and support create/read/write.

### F-P3-3: MAJOR — Shared content registry is last-writer-wins across hosts

**Where:** all-format ownership declaration `NeuralAmpModeler/VoLumPaths.h:220-225`; assigned preset entry point `NeuralAmpModeler/VoLumSettingsPresets.inc.cpp:95-108`; persistence path `NeuralAmpModeler/VoLumCustomContentApi.h:486-508`; full-registry write `NeuralAmpModeler/VoLumContentStore.h:749-755`.

**Evidence:**

```cpp
// VoLumPaths.h:220-225
// VoLum-owned content store directory (1.2.0 BYO + presets). Sibling of
// volum-settings.json, but unlike that file the content store is read AND
// written by ALL formats (standalone + VST3 + AU) so a plugin instance can
// resolve a preset/project's references to custom amps / IRs / pedals by their
// stable opaque ids.
```

```cpp
// VoLumCustomContentApi.h:486-508
inline int AddPreset(int /*ampIdx*/, const std::string& name)
{
  auto& reg = Store().reg();
  auto& bank = reg.presetBanks[ActivePresetOwnerKey()];
  // ... mutate this process's in-memory registry ...
  bank.push_back(std::move(pr));
  Store().Save();
  return (int)bank.size() - 1;
}
```

```cpp
// VoLumContentStore.h:749-755
bool Save()
{
  if (mBase.empty())
    return true;
  std::error_code ec;
  std::filesystem::create_directories(mBase, ec);
  return WriteJsonAtomically(RegistryPath(), RegistryToJson(mReg), ec);
}
```

**Mechanism:** Each process loads its own in-memory copy and later atomically replaces the entire registry. `WriteJsonAtomically` prevents a torn individual file but provides no cross-process lock, reload, merge, or compare-and-swap. A later stale writer deletes changes committed by another process.

**Trigger:** Open VoLum in two DAWs, or standalone plus a DAW. Save a preset/import in process A, then cause process B (which opened earlier) to save its stale registry, for example by editing a focused custom amp and saving host state/closing.

**Impact:** Presets, imported-library metadata, custom scenes, or IR metadata added by process A disappear from the registry. Copied payload files can remain orphaned, while the UI presents this as a successful save.

**Fix sketch:** Serialize registry read-modify-write with an OS-level cross-process lock. After acquiring it, reload the latest registry and merge operation-level changes by stable ID; do not replace from a stale process snapshot.

**Proposed regression test:** `content_registry_concurrent_process_edits_merge` — coordinate two helper processes loaded from the same baseline, add distinct preset IDs, save in reverse order, and assert both IDs remain.

### F-P3-4: MAJOR — Calibration read-modify-write can roll back all standalone settings

**Where:** `NeuralAmpModeler/VoLumSettingsScene.inc.cpp:384-419`; full writer at `334-372`; calibration is enabled for UI edits in all formats at `NeuralAmpModeler/NeuralAmpModeler.cpp:1463-1467`.

**Evidence:**

```cpp
// VoLumSettingsScene.inc.cpp:391-418
// Multiple plugin instances in one host may edit this machine-global default.
static std::mutex calibrationSettingsMutex;
std::lock_guard<std::mutex> lock(calibrationSettingsMutex);

nlohmann::json j = nlohmann::json::object();
std::error_code ec;
if (fs::exists(settingsPath, ec))
{
  try
  {
    std::ifstream in(settingsPath);
    in >> j;
    if (!j.is_object())
      return;
  }
  catch (...)
  {
    return;
  }
}
// ...
j["CalibrateInput"] = GetParam(kCalibrateInput)->Bool();
j["InputCalibrationLevel"] = GetParam(kInputCalibrationLevel)->Value();
if (!volum::WriteJsonAtomically(settingsPath, j, ec))
```

**Mechanism:** The static mutex only synchronizes plugin instances inside one process, and the full standalone writer does not take it. The calibration path reads the whole settings document, changes two keys, then replaces the whole file. A standalone write between its read and replace is silently reverted; two standalone processes also replace each other's complete snapshots.

**Trigger:** Keep a DAW/plugin and standalone open. Change calibration in the plugin while changing an amp/effect/lock setting in standalone, with the standalone replacement landing after the plugin read but before the plugin replacement.

**Impact:** The calibration write succeeds but restores stale amp scenes, effects, lock snapshots, active IDs, and Lite mode from its old JSON copy.

**Fix sketch:** Put both full and calibration writers under the same OS-level cross-process lock and perform the calibration read/modify/write after acquiring it. Consider a small dedicated calibration sidecar to remove whole-document merging.

**Proposed regression test:** `calibration_update_does_not_revert_concurrent_settings` — pause a calibration writer after read, commit a changed `lastAmpIdx` from a second process, resume calibration, then assert both the new amp index and calibration values survive.

### F-P3-5: MAJOR — Content-library write failures are reported as successful

**Where:** `NeuralAmpModeler/VoLumSettingsPresets.inc.cpp:95-108,111-120`; `NeuralAmpModeler/VoLumCustomContentApi.h:486-520`; `NeuralAmpModeler/VoLumSettingsScene.inc.cpp:379-381`.

**Evidence:**

```cpp
// VoLumSettingsPresets.inc.cpp:95-108
int NeuralAmpModeler::_VolumSavePresetAs(const std::string& name)
{
  volum::custom::SetActivePresetOwner(_VolumActiveOwnerKey());
  const int idx = volum::custom::AddPreset(mVolumAmpIdx, name);
  if (idx < 0)
    return idx;
  // The freshly saved preset becomes the active, clean recalled snapshot.
  mVolumActivePresetId = volum::custom::PresetIdAt(idx);
  // ...
  return idx;
}
```

```cpp
// VoLumCustomContentApi.h:504-508
if (PresetCaptureHook())
  pr.settings = PresetCaptureHook()();
bank.push_back(std::move(pr));
Store().Save();
return (int)bank.size() - 1;
```

```cpp
// VoLumSettingsScene.inc.cpp:379-381
// Persist the shared content library too (custom-amp scenes accumulate live
// knob edits via _VolumSaveCurrentToSettings). No-op when no base dir is set.
volum::content::GlobalContentStore().Save();
```

**Mechanism:** `ContentStore::Save()` returns `false` on directory or atomic-write failure, but the preset helpers and scene flush discard it. `AddPreset` still returns a nonnegative index, and the assigned caller marks the preset active/clean.

**Trigger:** Fill the disk, make `%LOCALAPPDATA%\VoLum\content` read-only, disconnect a redirected profile during save, or let antivirus hold the registry target beyond the short rename retry window.

**Impact:** The UI says the preset/overwrite succeeded and clears dirty state, but the preset or custom-scene edit vanishes after restart. There is no retry/error surfaced to the user.

**Fix sketch:** Propagate a structured save result through `AddPreset`/`OverwritePreset` and scene flushes. Only mark clean/active after persistence succeeds; retain dirty state and retry or show an actionable error.

**Proposed regression test:** `preset_save_failure_remains_dirty_and_reports_error` — point the content store below a regular file/read-only directory, save a preset, and assert the API returns failure, no clean selection is committed, and restart does not pretend it was saved.

### F-P3-6: MAJOR — Main and dual-amp settings can commit different generations

**Where:** `NeuralAmpModeler/VoLumSettingsScene.inc.cpp:334-381` and overlay load at `477-490`.

**Evidence:**

```cpp
// VoLumSettingsScene.inc.cpp:364-381
std::error_code ec;
if (!volum::WriteJsonAtomically(settingsPath, j, ec))
  return;

if (!volum::WriteJsonAtomically(dualAmpSettingsPath, dualAmpJson, ec))
  return;

volum::content::GlobalContentStore().Save();
```

```cpp
// VoLumSettingsScene.inc.cpp:477-490
if (!dualAmpUserPath.empty() && fs::exists(dualAmpUserPath))
  dualAmpSettingsPath = dualAmpUserPath;
// ...
if (!dualAmpSettingsPath.empty())
{
  std::ifstream dualIn(dualAmpSettingsPath);
  nlohmann::json dualAmpJson;
  dualIn >> dualAmpJson;
  // overlay sidecar fields onto the main-file settings
  volum::VolumUserSettingsFromJson(
    dualAmpJson, mVolumAmpSettings.data(), volum::kAmpCount, nullptr, nullptr, &dualAmpSettingsHealed);
}
```

**Mechanism:** Each file is replaced atomically, but the pair has no generation ID or transaction. A kill/failure after the main replacement and before the sidecar replacement leaves a new core scene with old dual routing/support state. A sidecar failure also returns before the content registry flush.

**Trigger:** Terminate or lose power during save between the two replacements, or cause only the sidecar rename to fail (sharing violation/antivirus/disk exhaustion after the first write).

**Impact:** The next launch loads a hybrid rig that never existed: new amp/PRE/POST values with stale dual-amp route, support amp/cab/channel, pans, or polarity. Pending custom-scene persistence can also be skipped.

**Fix sketch:** Store the complete schema in one atomic file, or add a shared monotonically increasing generation to both files and reject/recover mismatches. Stage both and commit through a journal/manifest; do not gate content persistence on sidecar success.

**Proposed regression test:** `settings_pair_rejects_mismatched_generations` — write generation N main and generation N-1 sidecar, load, and assert stale dual fields are not overlaid (today they are).

### F-P3-7: MAJOR — Directory scan can throw when a rig drive changes mid-iteration

**Where:** `NeuralAmpModeler/VoLumPaths.h:163-192`.

**Evidence:**

```cpp
// VoLumPaths.h:169-180
fs::path ampDir = rigsRoot / ampFolder;
std::error_code ec;
if (!fs::is_directory(ampDir, ec))
  return result;

for (const auto& entry : fs::directory_iterator(ampDir, ec))
{
  if (!entry.is_regular_file(ec))
    continue;
  std::string name = entry.path().filename().string();
```

**Mechanism:** The `error_code` suppresses only iterator construction errors. Range-for advances with throwing `directory_iterator::operator++()`, not `increment(ec)`. If enumeration fails after construction, the exception escapes `DiscoverChannels`; its callers run during construction and amp/support refresh without a local filesystem exception boundary.

**Trigger:** Put a portable rig tree on a network/removable drive and disconnect it, revoke access, or remove the directory while VoLum enumerates channels.

**Impact:** Amp/channel selection or plugin startup can throw instead of returning an empty channel list; in a host this can disable the instance or destabilize the UI/scan.

**Fix sketch:** Use an explicit iterator loop with `it.increment(ec)`, check/clear `ec` per entry, and return the channels collected so far or a controlled load error.

**Proposed regression test:** `discover_channels_mid_iteration_error_is_nonfatal` — use an injectable iterator that returns one entry then `io_error`; assert no exception and a deterministic partial/empty result.

### F-P3-8: MAJOR — Portable rigs disappear when the module path exceeds MAX_PATH

**Where:** `NeuralAmpModeler/VoLumPaths.h:84-101`.

**Evidence:**

```cpp
// VoLumPaths.h:84-101
HMODULE hMod = nullptr;
if (GetModuleHandleExW(/* ... */, &hMod) && hMod != nullptr)
{
  wchar_t module[MAX_PATH];
  const DWORD n = GetModuleFileNameW(hMod, module, MAX_PATH);
  if (n > 0 && n < MAX_PATH)
  {
    fs::path d = fs::path(module).parent_path();
    for (int depth = 0; depth < 12; ++depth)
    {
      candidates.push_back(d / "VoLumRigs");
      candidates.push_back(d / "rigs");
      d = d.parent_path();
    }
  }
}
```

**Mechanism:** `GetModuleFileNameW` reports truncation at the supplied capacity, and the code drops the module candidate entirely when `n == MAX_PATH`. It then searches only installer registry candidates and CWD-relative folders.

**Trigger:** Run the portable standalone/VST3 from a path longer than 259 characters without the installer registry key, while the host/current working directory is not the VoLum package directory.

**Impact:** `FindRigsRootDirectory()` returns empty even though `VoLumRigs` is beside the module. Factory amp models/channels are unavailable, making that installation effectively unusable.

**Fix sketch:** Grow a wide buffer until `GetModuleFileNameW` succeeds without truncation (and ensure the binary is long-path-aware). Keep module-relative discovery independent of CWD.

**Proposed regression test:** `rig_root_resolution_accepts_long_module_path` — pass a synthetic module path over 260 characters with a sibling `VoLumRigs` tree and assert it resolves.

### F-P3-9: MINOR — Log rotation races across processes and can erase the useful generation

**Where:** `NeuralAmpModeler/VoLumDiagLog.h:118-140`; shared-file intent `NeuralAmpModeler/NeuralAmpModeler.cpp:301-318`.

**Evidence:**

```cpp
// VoLumDiagLog.h:118-139
void Write(const char* category, const std::string& message)
{
  std::lock_guard<std::mutex> lock(mMutex);
  // ...
  const auto size = std::filesystem::file_size(mPath, ec);
  if (ShouldRotateLog(cur, line.size(), kMaxLogBytes))
  {
    std::filesystem::remove(RolledPath(mPath), rec);
    std::filesystem::rename(mPath, RolledPath(mPath), rec);
    if (rec)
      std::filesystem::remove(mPath, rec);
  }
  std::ofstream out(mPath, std::ios::app | std::ios::binary);
```

**Mechanism:** `mMutex` is process-local while standalone and plugin processes intentionally share one file. Two processes can both decide to rotate: one renames the full current log, the second deletes that new `.1`, then renames a newly-created short current log over it. The diagnostic history can collapse to one or two lines. On rename failure the code may also delete the current path.

**Trigger:** Run standalone and a DAW (or two DAWs) while the log is near 512 KiB and both emit a line.

**Impact:** The exact history needed for a bug report is silently lost or split incorrectly; the advertised two-generation bound is not a coherent two-generation history under supported multi-process use.

**Fix sketch:** Protect size-check/rotation/append with an interprocess lock, or use per-process log files and merge/collect them for support. Avoid deleting the current file as a fallback for an unclassified rename race.

**Proposed regression test:** `diagnostic_log_multiprocess_rotation_preserves_generation` — coordinate two helper processes at the rotation threshold and assert the pre-rotation marker remains in either `volum.log` or `.1`, with every record newline-complete.

### F-P3-10: MINOR — Diagnostic log records full user paths

**Where:** sink `NeuralAmpModeler/VoLumDiagLog.h:118-139`; path-bearing call sites `NeuralAmpModeler/VoLumLoader.inc.cpp:170-185,200-212,229-232`; `NeuralAmpModeler/NeuralAmpModeler.cpp:1983-1994,2087-2091`.

**Evidence:**

```cpp
// VoLumLoader.inc.cpp:170-185
if (!result.error.empty())
{
  mVolumMainLoadFailed.store(true);
  VOLUM_LOG("model", "MAIN load FAILED " + result.path + " : " + result.error);
  continue;
}
// ...
VOLUM_LOG("model", "MAIN loaded " + result.path);
```

```cpp
// NeuralAmpModeler.cpp:2087-2091
VOLUM_LOG("ir", std::string(wavState == dsp::wav::LoadReturnCode::SUCCESS ? "staged " : "load FAILED ")
                  + (support ? "[support] " : "[main] ") + irPath.Get()
                  + (wavState == dsp::wav::LoadReturnCode::SUCCESS
                       ? ""
                       : " (code " + std::to_string(static_cast<int>(wavState)) + ")"));
```

**Mechanism:** The logger writes messages verbatim. Model/IR paths are absolute runtime paths, so routine successful loads record the Windows/macOS account name and user-chosen directory names.

**Trigger:** Load any factory/custom model or IR, then attach `%LOCALAPPDATA%\VoLum\volum.log` to a public issue/support ticket.

**Impact:** The report can disclose the user's account name and private folder/project naming. Nothing in the log format warns or redacts before sharing.

**Fix sketch:** Log content-relative IDs plus basename, or redact known home/Local AppData prefixes to `%USER_DATA%`. Keep an explicit opt-in verbose mode if full paths are occasionally essential.

**Proposed regression test:** `diagnostic_log_redacts_user_home` — log a load under a synthetic Unicode home path and assert the log contains the basename/content ID but not the home prefix.

### F-P3-11: MINOR — Invalid settings are overwritten without quarantine

**Where:** parse/load `NeuralAmpModeler/VoLumSettingsScene.inc.cpp:443-503`; parser entry `NeuralAmpModeler/VoLumUserSettingsIO.h:672-680`; unconditional standalone shutdown save `NeuralAmpModeler/NeuralAmpModeler.cpp:540-545`.

**Evidence:**

```cpp
// VoLumSettingsScene.inc.cpp:443-503
try
{
  std::ifstream in(settingsPath);
  nlohmann::json j;
  in >> j;
  // No root j.is_object() validation before applying defaults.
  volum::VolumUserSettingsFromJson(j, mVolumAmpSettings.data(), volum::kAmpCount, &mVolumAmpIdx, /* ... */);
  // ...
}
catch (...)
{
  std::cerr << "Failed to read volum-settings.json" << std::endl;
}
```

```cpp
// NeuralAmpModeler.cpp:540-545
NeuralAmpModeler::~NeuralAmpModeler()
{
  _VolumStopLoader();
  _VolumSaveCurrentToSettings();
#ifdef APP_API
  _VolumSaveSettingsToFile();
```

**Mechanism:** Truncated/binary JSON is caught and only printed to stderr. A syntactically valid wrong-shape root such as `[]` is not rejected by `VolumUserSettingsFromJson`; all `contains()` probes simply miss and defaults remain. In both cases the standalone later unconditionally atomically replaces the same path on close. Unlike `ContentStore::Load`, there is no `.bak` quarantine.

**Trigger:** Launch then close standalone with an empty/truncated settings file, or a valid JSON scalar/array produced by corruption, sync conflict, or manual edit.

**Impact:** The original artifact is destroyed and cannot be recovered or inspected; any salvageable values and evidence of the corruption are lost, replaced by defaults/current session state.

**Fix sketch:** Validate a root object and required/schema marker before applying. On parse/wrong-shape failure, move/copy the file to a unique `.corrupt.<timestamp>.bak`, mark the session as not safe to overwrite unless recovery is explicit, and log the backup location.

**Proposed regression test:** `invalid_settings_are_quarantined_before_shutdown_save` — seed truncated JSON and `[]`, load and save/close, then assert the original bytes exist under a quarantine name and were not silently discarded.

## Voicing observations (report only)

None. No tone/voicing changes are proposed.

## Areas read and found clean

- `NeuralAmpModeler/VoLumUserSettingsIO.h` — every line read. Numeric and boolean settings are generally type-checked, finite-checked, and range-checked; newer positive versions are forward-tolerant; unknown keys do not drive filesystem calls. No audio-voicing finding reported.
- `NeuralAmpModeler/VoLumSettingsFileIO.h` — every line read. For one writer/one target, temp write + close + replace preserves the old target on ordinary write/rename failure and cleans the temp on handled failure. No path traversal originates here because it receives complete internal target paths.
- `NeuralAmpModeler/VoLumPaths.h` — every line read. Installer registry access uses wide Win32 APIs, validates directory type, closes the key on all observed branches, and catalog amp/speaker components are internal constants rather than user-controlled path segments.
- `NeuralAmpModeler/VoLumDiagLog.h` — every line read. Same-process writes are mutex-serialized, normal filesystem errors are swallowed, and single-process rotation is size-bounded to one rolled generation. No product `Close()` race was found.
- `NeuralAmpModeler/VoLumSettingsLocks.inc.cpp` — every line read. No filesystem path construction, recursive deletion, or external-input filename use was present; lock snapshots are stored in typed settings fields.
- `NeuralAmpModeler/VoLumSettingsScene.inc.cpp` — every line read. Main/sidecar JSON parsing is exception-contained after path selection, and stored custom/IR IDs are resolved through registry lookups rather than concatenated directly as paths.
- `NeuralAmpModeler/VoLumSettingsPresets.inc.cpp` — every line read. Preset owner keys and IDs are registry keys, not direct filesystem paths; no delete/remove path is built from a preset name here.
- `NeuralAmpModeler/VoLumSettings.inc.cpp` — every line read. It is only the include umbrella and contains no additional persistence logic.
- No recursive-delete call exists in the assigned production files. No assigned deletion path accepts an empty path, absolute user-supplied path, `..`, Windows device name, or trailing-dot/space filename.
