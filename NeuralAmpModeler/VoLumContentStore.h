#pragma once

// VoLum 1.2.0 content store (production backend for F5-F8).
//
// Replaces the in-memory VoLumCustomContent* session stores with a real,
// VoLum-owned, all-format (standalone + VST3 + AU) content library:
//
//   <base>/volum-content.json   registry: custom-amp manifests, IR library,
//                               pedal library, per-amp preset banks, and the
//                               isolated per-custom-amp scenes - all addressed
//                               by stable opaque ids (never by name/position).
//   <base>/amps/                copied custom-amp .nam captures
//   <base>/ir/                  copied custom IR .wav files
//   <base>/pedals/              copied imported PRE .nam captures
//
// The base dir is injectable so doctests run against a temp directory and the
// real path (Application Support / LOCALAPPDATA) is resolved only in the plugin.
//
// IO policy (mirrors volum-settings.json, see spec 3.6):
//   - reads are lenient: unknown keys ignored, malformed entries skipped,
//     scalars clamped; a recalled ref to missing content falls back per the
//     removal matrix (RemoveCustom* below) rather than wiping the file;
//   - if the whole file is unparseable it is renamed to .bak and we start from
//     defaults (never silently lose data);
//   - writes are atomic (temp file + rename), reusing WriteJsonAtomically.

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <limits>
#include <map>
#include <mutex>
#include <optional>
#include <random>
#include <string>
#include <thread>
#include <vector>

#include "VoLumAmpSettingsJson.h"
#include "VoLumCustomModel.h" // volum::custom::CustomAmp + pure helpers
#include "VoLumMidi.h"
#include "VoLumSettingsFileIO.h" // WriteJsonAtomically (also pulls in <windows.h> on Win32)

#ifndef _WIN32
  #include <fcntl.h>
  #include <sys/file.h>
  #include <unistd.h>
#endif

namespace volum
{
namespace content
{

// iPlug file pickers expose UTF-8 paths on every platform. On Windows,
// filesystem::path(std::string) interprets bytes in the active ANSI code page,
// so a UTF-8 username/folder resolves to a different, nonexistent path. Keep
// registry paths UTF-8 and convert explicitly at the filesystem boundary.
inline std::filesystem::path PathFromUtf8(const std::string& utf8)
{
#if defined(__cpp_char8_t)
  const auto* first = reinterpret_cast<const char8_t*>(utf8.data());
  return std::filesystem::path(std::u8string(first, first + utf8.size()));
#else
  return std::filesystem::u8path(utf8);
#endif
}

inline std::string PathToUtf8(const std::filesystem::path& path)
{
#if defined(__cpp_char8_t)
  const std::u8string utf8 = path.u8string();
  return std::string(reinterpret_cast<const char*>(utf8.data()), utf8.size());
#else
  return path.u8string();
#endif
}

// True when relPath is a registry-relative path that stays inside the content
// directory ("ir/ir_x__cab.wav", "amps/amp_y__G65.nam").
//
// Registry paths are only as trustworthy as volum-content.json, which users do
// sync, restore from backups and hand-edit. `mBase / relPath` is not a safe
// composition: an absolute relPath replaces the base outright, a drive-relative
// "C:x" does the same on Windows, and ".." walks out of the library. Resolved
// paths are handed to std::filesystem::remove on delete, so an escaping entry
// meant VoLum could delete a file that was never its to touch.
inline bool IsSafeStoredRelPath(const std::string& relPath)
{
  if (relPath.empty())
    return false;

  const auto path = PathFromUtf8(relPath);
  // has_root_name covers "C:x" and "\\\\server\\share"; has_root_directory covers
  // "/x". Checking both is wider than is_absolute(), which on Windows requires a
  // root name *and* a root directory and so accepts each half on its own.
  if (path.is_absolute() || path.has_root_name() || path.has_root_directory())
    return false;

  // A payload is always a file inside a subdirectory, never a directory itself.
  // "." and "ir" resolve to the library root and to "<base>/ir", and remove()
  // succeeds on an empty directory, so an entry like that could delete the
  // library's own folder rather than a capture.
  // A payload is always a file inside a subdirectory, never a directory itself.
  // "." and "ir" resolve to the library root and to "<base>/ir", and remove()
  // succeeds on an empty directory, so an entry like that could delete the
  // library's own folder rather than a capture.
  if (!path.has_parent_path() || !path.has_filename())
    return false;

  for (const auto& part : path)
  {
    if (part == ".." || part == ".")
      return false;
  }
  return true;
}

// v3 (VoLum 1.2.1) adds per-IR shaping (trimDb / lowCutHz / highCutHz) to each
// irLibrary entry. The reader is additive/forward-tolerant (unknown keys ignored,
// missing keys defaulted), so v2 files load unchanged and v3 files load in older
// builds; the bump is only a marker of the new capability, not a migration gate.
//
// v4 (VoLum 1.3.0) adds "midiSoundMap" -- the machine-global MIDI Sound
// assignments used by MIDI and PLAY -- and stops writing "customScenes". The
// sounding rig now lives on the VoLum instance (DAW chunk / standalone settings)
// like a factory amp's, so a catalog write can never rewrite a sibling's live
// knobs. A v3 file's customScenes are still read once, as a migration source.
inline constexpr int kContentSchemaVersion = 4;

// Imported pedals get stable monotonic PRE-capture indices at/above this base
// so adding/removing a custom pedal never reshuffles an index a saved chunk or
// preset already points at (spec 3.4). The base sits well above the handful of
// factory captures (leaving 1..63 for factory growth) but stays inside the PRE
// capture param range (0..127, see VoLumPrePedalCaptures.h), so a custom index
// is always a valid param value. That caps custom pedals at (127 - 64 + 1) = 64.
inline constexpr int kCustomPedalIndexBase = 64;
inline constexpr int kCustomPedalIndexMax = 127;

// Per-IR shaping (VoLum 1.2.1). Custom IRs are convolved with a fixed -18 dB baked
// into the convolver (dsp::ImpulseResponse::_SetWeights), so they land much quieter
// than the baked stock cabs. These VoLum-side controls fix that and let a custom IR
// be tone-shaped without editing the .wav. Stored per-IR in the library, so the
// setting follows the IR wherever it is used (MAIN + SUPPORT lanes). Not a DAW
// parameter, so no EParams/chunk change.
inline constexpr double kIrTrimDbMin = -24.0;
inline constexpr double kIrTrimDbMax = 24.0;
// Cut Hz of 0 means the filter is OFF. When enabled the values are clamped to a
// musical range (low-cut below the low mids, high-cut across the presence/air band).
inline constexpr double kIrLowCutHzMax = 800.0;
inline constexpr double kIrHighCutHzMin = 1000.0;
inline constexpr double kIrHighCutHzMax = 20000.0;
// The convolver bakes a fixed -18 dB into every custom IR (see _SetWeights). Auto-
// normalize on import undoes that bake and equalizes broadband energy across IRs by
// driving the effective convolution gain toward unity for equal-power input:
// trimDb = 18 - 20*log10(L2(h)), clamped. Deterministic (unit-tested).
inline constexpr double kIrBakedReductionDb = 18.0;

inline double ClampIrTrimDb(double db)
{
  return std::clamp(db, kIrTrimDbMin, kIrTrimDbMax);
}

// Clamp a low-cut frequency; 0 (or non-positive) means OFF.
inline double ClampIrLowCutHz(double hz)
{
  if (!(hz > 0.0))
    return 0.0;
  return std::clamp(hz, 20.0, kIrLowCutHzMax);
}

// Clamp a high-cut frequency; 0 (or non-positive) means OFF.
inline double ClampIrHighCutHz(double hz)
{
  if (!(hz > 0.0))
    return 0.0;
  return std::clamp(hz, kIrHighCutHzMin, kIrHighCutHzMax);
}

inline double AutoNormalizeIrTrimDb(double l2Norm)
{
  if (!(l2Norm > 0.0))
    return 0.0;
  return ClampIrTrimDb(kIrBakedReductionDb - 20.0 * std::log10(l2Norm));
}

// Discrete UI steps for the IR panel, kept pure + shared so the popover and a unit
// test agree on one ladder. dir < 0 lowers, dir > 0 raises. The cut ladders include
// a 0 (= OFF) rung at the "open" end (low-cut off = lowest; high-cut off = highest).
inline double StepIrTrimDb(double db, int dir)
{
  const double stepped = db + (dir >= 0 ? 0.5 : -0.5);
  return ClampIrTrimDb(std::round(stepped * 2.0) / 2.0); // snap to a 0.5 dB grid
}

// Sort key for a ladder entry. OFF is stored as 0 but sits at opposite ends of the
// two ladders: a disabled low cut is the most open setting at the bottom, a disabled
// high cut is the most open setting at the top. Mapping it to +inf for the high-cut
// ladder lets one ordering rule serve both.
inline double IrLadderKey(double hz, bool offIsHighest)
{
  if (hz > 0.0)
    return hz;
  return offIsHighest ? std::numeric_limits<double>::infinity() : 0.0;
}

// Move to the adjacent rung: strictly the next one above for dir >= 0, strictly the
// next one below otherwise. Deliberately not "round to nearest, then move by one" -
// with typed entry a value can sit between rungs, and rounding first would silently
// skip the rung the user is standing next to (137 Hz stepping up to 200 rather than
// 150). At a rail there is no adjacent rung and the value is returned unchanged,
// which is also what the popover reads to gray the button out.
inline double StepIrLadder(const double* ladder, int n, double cur, int dir, bool offIsHighest)
{
  const double k = IrLadderKey(cur, offIsHighest);
  int bestIdx = -1;
  double bestKey = 0.0;
  for (int i = 0; i < n; ++i)
  {
    const double ki = IrLadderKey(ladder[i], offIsHighest);
    const bool candidate = (dir >= 0) ? (ki > k) : (ki < k);
    if (!candidate)
      continue;
    const bool better = (bestIdx < 0) || ((dir >= 0) ? (ki < bestKey) : (ki > bestKey));
    if (better)
    {
      bestIdx = i;
      bestKey = ki;
    }
  }
  return bestIdx < 0 ? cur : ladder[bestIdx];
}

inline double StepIrLowCutHz(double hz, int dir)
{
  // OFF at the bottom, then a musical low-cut sweep up to the low mids.
  static const double kLadder[] = {0.0, 20, 30, 40, 50, 60, 80, 100, 120, 150, 200, 300, 400, 500, 650, 800};
  const int n = static_cast<int>(sizeof(kLadder) / sizeof(kLadder[0]));
  return ClampIrLowCutHz(StepIrLadder(kLadder, n, ClampIrLowCutHz(hz), dir, /*offIsHighest=*/false));
}

inline double StepIrHighCutHz(double hz, int dir)
{
  // Ascending presence/air ladder with OFF (0) as the top "fully open" rung, so
  // stepping DOWN from OFF engages a 20 kHz cut and keeps darkening.
  static const double kLadder[] = {1000, 1500, 2000, 3000, 4000, 5000, 6000, 8000, 10000, 12000, 16000, 20000, 0.0};
  const int n = static_cast<int>(sizeof(kLadder) / sizeof(kLadder[0]));
  const double in = (hz > 0.0) ? ClampIrHighCutHz(hz) : 0.0;
  return ClampIrHighCutHz(StepIrLadder(kLadder, n, in, dir, /*offIsHighest=*/true));
}

// Whether each stepper direction still has somewhere to go, so the popover can gray
// out a button at its rail instead of offering a click that does nothing. Defined in
// terms of the ladders above rather than duplicating the endpoints, so the two can
// never drift apart.
struct IrStepAvail
{
  bool canDown = true;
  bool canUp = true;
};

inline IrStepAvail IrTrimStepAvail(double db)
{
  return {StepIrTrimDb(db, -1) != db, StepIrTrimDb(db, +1) != db};
}

inline IrStepAvail IrLowCutStepAvail(double hz)
{
  const double cur = ClampIrLowCutHz(hz);
  return {StepIrLowCutHz(cur, -1) != cur, StepIrLowCutHz(cur, +1) != cur};
}

inline IrStepAvail IrHighCutStepAvail(double hz)
{
  const double cur = ClampIrHighCutHz(hz);
  return {StepIrHighCutHz(cur, -1) != cur, StepIrHighCutHz(cur, +1) != cur};
}

// Typed entry for the IR shaping popover. The steppers walk a musical ladder, but a
// user who knows the number they want should be able to type it, so typed values are
// continuous and only clamped to the control's range - they do not snap to a rung.
enum class IrTypedKind
{
  Invalid, // not a number: keep whatever the control already shows
  Off, // "off", "-", or an explicit zero: disable the filter
  Number
};

struct IrTypedValue
{
  IrTypedKind kind = IrTypedKind::Invalid;
  double value = 0.0;
};

// Accepts what a user actually types: "2.5k", "2500 Hz", "-3 dB", "+3", "off".
// A trailing k/K multiplies by 1000 so "1.5k" and "1500" mean the same thing.
inline IrTypedValue ParseIrTypedValue(const std::string& text)
{
  std::string s;
  s.reserve(text.size());
  for (char c : text)
    if (!std::isspace(static_cast<unsigned char>(c)))
      s.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));

  if (s.empty() || s == "off" || s == "-" || s == "none")
    return {IrTypedKind::Off, 0.0};

  const char* begin = s.c_str();
  char* end = nullptr;
  const double raw = std::strtod(begin, &end);
  if (end == begin || !std::isfinite(raw))
    return {};

  std::string suffix(end);
  double scale = 1.0;
  if (!suffix.empty() && suffix[0] == 'k')
  {
    scale = 1000.0;
    suffix.erase(0, 1);
  }
  // Anything left must be a unit we recognize; a stray "2.5 foo" is a typo, not a value.
  if (!(suffix.empty() || suffix == "hz" || suffix == "db"))
    return {};

  const double v = raw * scale;
  if (v == 0.0)
    return {IrTypedKind::Off, 0.0};
  return {IrTypedKind::Number, v};
}

// A typed level of "off" means 0 dB (unity), since a makeup gain cannot be disabled.
inline double ApplyTypedIrTrimDb(const std::string& text, double current)
{
  const IrTypedValue t = ParseIrTypedValue(text);
  switch (t.kind)
  {
    case IrTypedKind::Off: return 0.0;
    case IrTypedKind::Number: return ClampIrTrimDb(t.value);
    default: return current;
  }
}

inline double ApplyTypedIrLowCutHz(const std::string& text, double current)
{
  const IrTypedValue t = ParseIrTypedValue(text);
  switch (t.kind)
  {
    case IrTypedKind::Off: return 0.0;
    case IrTypedKind::Number: return ClampIrLowCutHz(t.value);
    default: return current;
  }
}

inline double ApplyTypedIrHighCutHz(const std::string& text, double current)
{
  const IrTypedValue t = ParseIrTypedValue(text);
  switch (t.kind)
  {
    case IrTypedKind::Off: return 0.0;
    case IrTypedKind::Number: return ClampIrHighCutHz(t.value);
    default: return current;
  }
}

struct IRItem
{
  std::string id; // ir_<rand>
  std::string name; // user display name (unique within IR library, CI)
  std::string file; // stored path relative to base, e.g. "ir/ir_x__Mesa.wav"
  double trimDb = 0.0; // VoLum-side makeup gain, dB (kIrTrimDbMin..Max)
  double lowCutHz = 0.0; // low-cut (high-pass) freq; 0 = off
  double highCutHz = 0.0; // high-cut (low-pass) freq; 0 = off
  // Runtime-only (NOT serialized): false when the entry predates the trim field
  // (no "trimDb" key on load), so the plugin knows to auto-normalize it once and
  // persist. New imports set this true when they compute the auto-normalized trim.
  bool trimCalibrated = false;
};

struct PedalItem
{
  std::string id; // pedal_<rand>
  std::string name; // display name (unique within pedal library, CI)
  std::string group; // "klon" | "tsboost" | "distortion" | "fuzz" | "other" | ""
  std::string file; // stored path relative to base
  int legacyIndex = 0; // stable PRE-capture index (>= kCustomPedalIndexBase)
};

struct Preset
{
  std::string id; // preset_<rand>
  std::string name;
  VoLumAmpSettings settings;
};

// Owner key for a preset bank or scene: factory amps use "factory:<idx>",
// custom amps use their opaque amp id.
inline std::string FactoryOwnerKey(int ampIdx)
{
  return "factory:" + std::to_string(ampIdx);
}

inline bool IsFactoryOwnerKey(const std::string& key)
{
  return key.rfind("factory:", 0) == 0;
}

// ---------------------------------------------------------------------------
// MIDI sound map (1.3.0)
// ---------------------------------------------------------------------------
//
// One MIDI slot points at a Sound: an amp plus a preset on that amp. `ampId` is
// "factory:<idx>" or a custom-amp library id; `presetId` is a User preset library
// id or a shipped Factory preset id ("factory:<idx>:v1"). A Factory preset is not
// a library item, so it can never be found in a preset bank - it still serializes
// here, and resolution has to know the difference (see MidiSlotState).
//
// The map lives with the content library so every format (standalone, VST3, AU)
// writes it under the same lock as the catalog. The MIDI *decoder* and its Settings
// chrome are a separate effort; this is the persistence contract they build on.
struct MidiSoundAssignment
{
  std::string ampId;
  std::string presetId;
};

// A shipped Factory preset id, e.g. "factory:7:v1". Deliberately narrower than
// IsFactoryOwnerKey: "factory:7" is an amp, "factory:7:v1" is a preset.
inline bool IsFactoryPresetId(const std::string& id)
{
  if (id.rfind("factory:", 0) != 0)
    return false;
  return id.find(':', 8) != std::string::npos;
}

inline std::string FactoryPresetId(int ampIdx, int version = 1)
{
  return "factory:" + std::to_string(ampIdx) + ":v" + std::to_string(version);
}

// ---------------------------------------------------------------------------
// Custom-amp capture resolution (F6 DSP wiring)
// ---------------------------------------------------------------------------

// Registry-relative stored file for the capture at (slot, channel) within a
// custom amp's manifest, or "" when no assigned file matches. Validation forbids
// duplicate (slot, channel) cells, so at most one file ever matches.
inline std::string CaptureFileFor(const custom::CustomAmp& amp, int slot, int channel)
{
  for (const auto& f : amp.files)
    if (custom::FileAssigned(f) && f.slot == slot && f.channel == channel)
      return f.storedPath;
  return {};
}

// Pick the default (slot, channel) for a freshly-focused custom amp: the first
// populated slot (DIRECT first, per AmpSlots) and that slot's lowest channel.
// Returns false (leaving outputs untouched) when the amp has no assigned files.
inline bool DefaultCaptureSelection(const custom::CustomAmp& amp, int& slot, int& channel)
{
  const auto slots = custom::AmpSlots(amp);
  if (slots.empty())
    return false;
  const auto chans = custom::AmpSlotChannels(amp, slots.front());
  if (chans.empty())
    return false;
  slot = slots.front();
  channel = chans.front();
  return true;
}

struct Registry
{
  std::vector<custom::CustomAmp> amps; // manifests (inline)
  std::vector<IRItem> irs; // global IR library
  std::vector<PedalItem> pedals; // global pedal library
  std::map<std::string, std::vector<Preset>> presetBanks; // ownerKey -> presets
  std::map<int, MidiSoundAssignment> midiSoundMap; // MIDI slot -> Sound
  int nextPedalIndex = kCustomPedalIndexBase; // monotonic, never reused

  // Read from a pre-1.3.0 file's "customScenes" and never written back. The
  // sounding rig belongs to the instance now, so this is a one-way migration
  // source the plugin drains into its own per-instance scene map.
  std::map<std::string, VoLumAmpSettings> legacyCustomScenes;
};

struct ResolvedMidiSound
{
  std::string ampId;
  std::string presetId;
  VoLumAmpSettings settings;
};

inline const MidiSoundAssignment* MidiSoundAtSlot(const Registry& r, int slot)
{
  if (slot < 0 || slot >= kMidiSoundSlotCount)
    return nullptr;
  const auto it = r.midiSoundMap.find(slot);
  return it == r.midiSoundMap.end() ? nullptr : &it->second;
}

// Pure/headless lookup used by OnIdle, PLAY, and tests. Missing amps, missing
// User presets, unassigned slots, and mismatched Factory ids stay no-op. Shipped
// Ready presets (`factory:<idx>:v1`) resolve even though they are not stored in
// the user presetBanks.
inline std::optional<ResolvedMidiSound> ResolveMidiSound(const Registry& r, int slot)
{
  const MidiSoundAssignment* sound = MidiSoundAtSlot(r, slot);
  if (!sound)
    return std::nullopt;

  const int factoryIdx = FactoryAmpIndexFromId(sound->ampId);
  bool ampKnown = factoryIdx >= 0 && factoryIdx < kAmpCount;
  if (!ampKnown)
  {
    for (const auto& amp : r.amps)
      if (amp.id == sound->ampId)
      {
        ampKnown = true;
        break;
      }
  }
  if (!ampKnown)
    return std::nullopt;

  if (factoryIdx >= 0 && factoryIdx < kAmpCount
      && sound->presetId == FactoryOwnerKey(factoryIdx) + ":v1")
    return ResolvedMidiSound{sound->ampId, sound->presetId, VoLumAmpSettings{}};

  const auto bank = r.presetBanks.find(sound->ampId);
  if (bank == r.presetBanks.end())
    return std::nullopt;
  for (const auto& preset : bank->second)
    if (preset.id == sound->presetId)
      return ResolvedMidiSound{sound->ampId, preset.id, preset.settings};
  return std::nullopt;
}

inline int FirstFreeMidiSoundSlot(const Registry& r)
{
  for (int slot = 0; slot < kMidiSoundSlotCount; ++slot)
    if (!MidiSoundAtSlot(r, slot))
      return slot;
  return -1;
}

inline bool AssignMidiSound(Registry& r, int slot, const std::string& ampId, const std::string& presetId)
{
  if (slot < 0 || slot >= kMidiSoundSlotCount || ampId.empty() || presetId.empty())
    return false;
  r.midiSoundMap[slot] = MidiSoundAssignment{ampId, presetId};
  return true;
}

inline bool ClearMidiSound(Registry& r, int slot)
{
  return r.midiSoundMap.erase(slot) > 0;
}

// ---------------------------------------------------------------------------
// id minting
// ---------------------------------------------------------------------------

inline std::string MintRawId(const char* prefix)
{
  static thread_local std::mt19937_64 rng{std::random_device{}()};
  std::uniform_int_distribution<uint64_t> dist;
  const uint64_t v = dist(rng);
  static const char* kHex = "0123456789abcdef";
  std::string s = prefix;
  s += '_';
  for (int i = 0; i < 8; ++i)
    s += kHex[(v >> (i * 4)) & 0xF];
  return s;
}

inline bool IdInUse(const Registry& r, const std::string& id)
{
  for (const auto& a : r.amps)
    if (a.id == id)
      return true;
  for (const auto& ir : r.irs)
    if (ir.id == id)
      return true;
  for (const auto& p : r.pedals)
    if (p.id == id)
      return true;
  for (const auto& bank : r.presetBanks)
    for (const auto& pr : bank.second)
      if (pr.id == id)
        return true;
  return false;
}

inline std::string MintId(const Registry& r, const char* prefix)
{
  for (int attempt = 0; attempt < 64; ++attempt)
  {
    std::string id = MintRawId(prefix);
    if (!IdInUse(r, id))
      return id;
  }
  return MintRawId(prefix); // astronomically unlikely to reach here
}

// ---------------------------------------------------------------------------
// JSON (de)serialization
// ---------------------------------------------------------------------------

inline nlohmann::json CustomAmpToJson(const custom::CustomAmp& a)
{
  nlohmann::json j;
  j["id"] = a.id;
  j["name"] = a.name;
  j["art"] = a.art;
  j["cabNames"] = {a.cabNames[0], a.cabNames[1], a.cabNames[2]};
  nlohmann::json files = nlohmann::json::array();
  for (const auto& f : a.files)
    files.push_back({{"file", f.file}, {"slot", f.slot}, {"channel", f.channel}, {"storedPath", f.storedPath}});
  j["files"] = files;
  return j;
}

// Returns false (and leaves `out` untouched) when the entry is too malformed to
// use (no id). Otherwise fills `out`, clamping/skipping bad sub-fields.
inline bool CustomAmpFromJson(const nlohmann::json& j, custom::CustomAmp& out)
{
  if (!j.is_object() || !j.contains("id") || !j["id"].is_string())
    return false;
  out = custom::CustomAmp{};
  out.id = j["id"].get<std::string>();
  if (out.id.empty())
    return false;
  if (j.contains("name") && j["name"].is_string())
    out.name = j["name"].get<std::string>();
  if (j.contains("art") && j["art"].is_number_integer())
    out.art = ((j["art"].get<int>() % custom::kNumCustomArts) + custom::kNumCustomArts) % custom::kNumCustomArts;
  if (j.contains("cabNames") && j["cabNames"].is_array())
  {
    const auto& cn = j["cabNames"];
    for (int i = 0; i < custom::kNumCabSlots && i < static_cast<int>(cn.size()); ++i)
      if (cn[i].is_string())
        out.cabNames[(size_t)i] = cn[i].get<std::string>();
  }
  // Lenient-read clamp: enforce the 3-char cab-name rule on load so a
  // hand-edited or migrated registry can never overflow the cabinet row.
  custom::NormalizeAmpCabNames(out);
  if (j.contains("files") && j["files"].is_array())
  {
    for (const auto& f : j["files"])
    {
      if (!f.is_object() || !f.contains("file") || !f["file"].is_string())
        continue;
      custom::CustomNamFile cf;
      cf.file = f["file"].get<std::string>();
      if (f.contains("slot") && f["slot"].is_number_integer())
        cf.slot = f["slot"].get<int>();
      if (f.contains("channel") && f["channel"].is_number_integer())
        cf.channel = f["channel"].get<int>();
      if (f.contains("storedPath") && f["storedPath"].is_string())
        cf.storedPath = f["storedPath"].get<std::string>();
      out.files.push_back(cf);
    }
  }
  return true;
}

inline nlohmann::json RegistryToJson(const Registry& r)
{
  nlohmann::json j;
  j["schemaVersion"] = kContentSchemaVersion;
  j["nextPedalIndex"] = r.nextPedalIndex;

  nlohmann::json amps = nlohmann::json::array();
  for (const auto& a : r.amps)
    amps.push_back(CustomAmpToJson(a));
  j["customAmps"] = amps;

  nlohmann::json irs = nlohmann::json::array();
  for (const auto& ir : r.irs)
    irs.push_back({{"id", ir.id},
                   {"name", ir.name},
                   {"path", ir.file},
                   {"trimDb", ir.trimDb},
                   {"lowCutHz", ir.lowCutHz},
                   {"highCutHz", ir.highCutHz}});
  j["irLibrary"] = irs;

  nlohmann::json peds = nlohmann::json::array();
  for (const auto& p : r.pedals)
    peds.push_back(
      {{"id", p.id}, {"name", p.name}, {"group", p.group}, {"path", p.file}, {"legacyIndex", p.legacyIndex}});
  j["customPedals"] = peds;

  nlohmann::json banks = nlohmann::json::object();
  for (const auto& bank : r.presetBanks)
  {
    nlohmann::json arr = nlohmann::json::array();
    for (const auto& pr : bank.second)
      arr.push_back({{"id", pr.id}, {"name", pr.name}, {"settings", AmpSettingsToJson(pr.settings)}});
    banks[bank.first] = arr;
  }
  j["presetBanks"] = banks;

  // Deliberately no "customScenes": see kContentSchemaVersion v4. A downgrade to
  // 1.2.x finds the key missing and falls back to per-amp defaults on first focus,
  // which is the same thing it does for a custom amp it has never seen.
  nlohmann::json midi = nlohmann::json::array();
  for (const auto& slot : r.midiSoundMap)
    midi.push_back({{"slot", slot.first}, {"ampId", slot.second.ampId}, {"presetId", slot.second.presetId}});
  j["midiSoundMap"] = midi;

  return j;
}

// Tolerant reader. Returns the parsed registry; `healed` (optional) is set true
// when any entry was skipped/clamped so callers can rewrite the file.
inline Registry RegistryFromJson(const nlohmann::json& j, bool* healed = nullptr)
{
  Registry r;
  bool h = false;

  if (j.contains("nextPedalIndex") && j["nextPedalIndex"].is_number_integer())
    r.nextPedalIndex = std::max(kCustomPedalIndexBase, j["nextPedalIndex"].get<int>());

  if (j.contains("customAmps") && j["customAmps"].is_array())
  {
    for (const auto& a : j["customAmps"])
    {
      custom::CustomAmp amp;
      if (CustomAmpFromJson(a, amp))
        r.amps.push_back(std::move(amp));
      else
        h = true;
    }
  }

  if (j.contains("irLibrary") && j["irLibrary"].is_array())
  {
    for (const auto& ir : j["irLibrary"])
    {
      if (!ir.is_object() || !ir.contains("id") || !ir["id"].is_string() || ir["id"].get<std::string>().empty())
      {
        h = true;
        continue;
      }
      IRItem item;
      item.id = ir["id"].get<std::string>();
      if (ir.contains("name") && ir["name"].is_string())
        item.name = ir["name"].get<std::string>();
      if (ir.contains("path") && ir["path"].is_string())
        item.file = ir["path"].get<std::string>();
      // Per-IR shaping (v3). Presence of "trimDb" marks the entry as calibrated;
      // a v2 entry without it is left uncalibrated so the plugin auto-normalizes
      // it once from the .wav (retroactively fixing the "too quiet" complaint).
      if (ir.contains("trimDb") && ir["trimDb"].is_number())
      {
        item.trimDb = ClampIrTrimDb(ir["trimDb"].get<double>());
        item.trimCalibrated = true;
      }
      if (ir.contains("lowCutHz") && ir["lowCutHz"].is_number())
        item.lowCutHz = ClampIrLowCutHz(ir["lowCutHz"].get<double>());
      if (ir.contains("highCutHz") && ir["highCutHz"].is_number())
        item.highCutHz = ClampIrHighCutHz(ir["highCutHz"].get<double>());
      r.irs.push_back(std::move(item));
    }
  }

  if (j.contains("customPedals") && j["customPedals"].is_array())
  {
    for (const auto& p : j["customPedals"])
    {
      if (!p.is_object() || !p.contains("id") || !p["id"].is_string() || p["id"].get<std::string>().empty())
      {
        h = true;
        continue;
      }
      PedalItem item;
      item.id = p["id"].get<std::string>();
      if (p.contains("name") && p["name"].is_string())
        item.name = p["name"].get<std::string>();
      if (p.contains("group") && p["group"].is_string())
        item.group = p["group"].get<std::string>();
      if (p.contains("path") && p["path"].is_string())
        item.file = p["path"].get<std::string>();
      if (p.contains("legacyIndex") && p["legacyIndex"].is_number_integer())
        item.legacyIndex = p["legacyIndex"].get<int>();
      r.pedals.push_back(std::move(item));
      r.nextPedalIndex = std::max(r.nextPedalIndex, item.legacyIndex + 1);
    }
  }

  if (j.contains("presetBanks") && j["presetBanks"].is_object())
  {
    for (const auto& bank : j["presetBanks"].items())
    {
      if (!bank.value().is_array())
        continue;
      std::vector<Preset> presets;
      for (const auto& pr : bank.value())
      {
        if (!pr.is_object() || !pr.contains("id") || !pr["id"].is_string() || pr["id"].get<std::string>().empty())
        {
          h = true;
          continue;
        }
        Preset preset;
        preset.id = pr["id"].get<std::string>();
        if (pr.contains("name") && pr["name"].is_string())
          preset.name = pr["name"].get<std::string>();
        if (pr.contains("settings") && pr["settings"].is_object())
        {
          if (AmpSettingsFromJson(pr["settings"], preset.settings))
            h = true;
        }
        presets.push_back(std::move(preset));
      }
      if (!presets.empty())
        r.presetBanks[bank.key()] = std::move(presets);
    }
  }

  // Pre-1.3.0 shared scenes. Read (so the plugin can migrate them onto the
  // instance) but never written back, so the first save after an upgrade drops
  // them. Not a `healed` trigger: the key's presence is expected on an old file
  // and does not mean anything was malformed.
  if (j.contains("customScenes") && j["customScenes"].is_object())
  {
    for (const auto& sc : j["customScenes"].items())
    {
      if (!sc.value().is_object())
        continue;
      VoLumAmpSettings settings;
      if (AmpSettingsFromJson(sc.value(), settings))
        h = true;
      r.legacyCustomScenes[sc.key()] = settings;
    }
  }

  if (j.contains("midiSoundMap") && j["midiSoundMap"].is_array())
  {
    for (const auto& e : j["midiSoundMap"])
    {
      if (!e.is_object() || !e.contains("slot") || !e["slot"].is_number_integer())
      {
        h = true;
        continue;
      }
      MidiSoundAssignment a;
      if (e.contains("ampId") && e["ampId"].is_string())
        a.ampId = e["ampId"].get<std::string>();
      if (e.contains("presetId") && e["presetId"].is_string())
        a.presetId = e["presetId"].get<std::string>();
      // An entry with neither id is an unassigned slot, which is the same thing
      // as no entry at all; keeping it would only make an empty map look busy.
      if (a.ampId.empty() && a.presetId.empty())
        continue;
      r.midiSoundMap[e["slot"].get<int>()] = std::move(a);
    }
  }

  if (healed)
    *healed = h;
  return r;
}

// ---------------------------------------------------------------------------
// MIDI slot resolution
// ---------------------------------------------------------------------------

// What the allocation list shows for one slot. A slot whose Sound no longer
// exists is Invalid, not empty: the number stays with the player's pedalboard and
// the neighbour must not inherit it, so deleting content can only ever turn a row
// red - never renumber the rows below it.
enum class MidiSlotState
{
  Unassigned, // no entry: Program Change on it is ignored
  Valid,
  Invalid // assigned, but the amp or preset is gone (red in the list)
};

inline bool MidiAmpIdResolves(const Registry& r, const std::string& ampId, int factoryAmpCount)
{
  if (ampId.empty())
    return false;
  if (IsFactoryOwnerKey(ampId))
  {
    const std::string idxPart = ampId.substr(8);
    if (idxPart.empty())
      return false;
    for (char c : idxPart)
      if (!std::isdigit(static_cast<unsigned char>(c)))
        return false;
    const long idx = std::strtol(idxPart.c_str(), nullptr, 10);
    return idx >= 0 && idx < static_cast<long>(factoryAmpCount);
  }
  for (const auto& a : r.amps)
    if (a.id == ampId)
      return true;
  return false;
}

inline bool MidiPresetIdResolves(const Registry& r, const std::string& ampId, const std::string& presetId)
{
  if (presetId.empty())
    return true; // amp-only Sound: nothing to resolve
  // A Factory preset is shipped, not a library item, so it can never be looked up
  // in a bank. It is valid as long as it belongs to the slot's amp.
  if (IsFactoryPresetId(presetId))
    return presetId.rfind(ampId + ":", 0) == 0;
  const auto bank = r.presetBanks.find(ampId);
  if (bank == r.presetBanks.end())
    return false;
  for (const auto& pr : bank->second)
    if (pr.id == presetId)
      return true;
  return false;
}

inline MidiSlotState ResolveMidiSlot(const Registry& r, int slot, int factoryAmpCount)
{
  const auto it = r.midiSoundMap.find(slot);
  if (it == r.midiSoundMap.end())
    return MidiSlotState::Unassigned;
  if (!MidiAmpIdResolves(r, it->second.ampId, factoryAmpCount))
    return MidiSlotState::Invalid;
  return MidiPresetIdResolves(r, it->second.ampId, it->second.presetId) ? MidiSlotState::Valid
                                                                       : MidiSlotState::Invalid;
}

// ---------------------------------------------------------------------------
// Three-way merge by stable id
// ---------------------------------------------------------------------------
//
// Two VoLums - a DAW instance and the standalone, or two DAW tracks - are two
// writers of one `volum-content.json`. Serializing them with a lock is only half
// the answer: whoever writes second still has a whole catalog in memory that was
// read before the first one saved, so a plain write drops the other's work. This
// is where an IR imported in standalone used to vanish when the DAW next saved a
// preset.
//
// So a save is a merge, not a write. Three inputs:
//
//   disk      - what is on disk right now, read under the lock;
//   baseline  - what this writer last read or wrote (its idea of "unchanged");
//   current   - what this writer has in memory.
//
// `current` vs `baseline` says what *this* writer changed, by stable id. Only
// those changes are replayed onto `disk`. An item this writer never touched is
// left exactly as `disk` has it, even if `disk` is newer - so a sibling's edit is
// not reverted by a writer that merely happened to have the item loaded. Where
// both edited the same id, this writer wins (same-id last-writer-wins, as locked
// in the map ticket).
//
// Comparison is by serialized JSON: it is exactly the state that reaches disk, so
// two items compare equal iff writing either produces the same file. That also
// spares every content struct an operator== that would silently rot when a field
// is added.

inline bool SameContentItem(const custom::CustomAmp& a, const custom::CustomAmp& b)
{
  return CustomAmpToJson(a) == CustomAmpToJson(b);
}

inline bool SameContentItem(const IRItem& a, const IRItem& b)
{
  return a.id == b.id && a.name == b.name && a.file == b.file && a.trimDb == b.trimDb && a.lowCutHz == b.lowCutHz
         && a.highCutHz == b.highCutHz;
}

inline bool SameContentItem(const PedalItem& a, const PedalItem& b)
{
  return a.id == b.id && a.name == b.name && a.group == b.group && a.file == b.file && a.legacyIndex == b.legacyIndex;
}

inline bool SameContentItem(const Preset& a, const Preset& b)
{
  return a.id == b.id && a.name == b.name && AmpSettingsToJson(a.settings) == AmpSettingsToJson(b.settings);
}

inline bool SameContentItem(const MidiSoundAssignment& a, const MidiSoundAssignment& b)
{
  return a.ampId == b.ampId && a.presetId == b.presetId;
}

// Replay this writer's id-level changes to one library collection onto `target`.
// Adds land at the end in `current` order; an item already in `target` keeps its
// position so a merge never reshuffles a list the user is looking at.
template <typename T>
void MergeContentVector(std::vector<T>& target, const std::vector<T>& baseline, const std::vector<T>& current)
{
  auto findById = [](std::vector<T>& v, const std::string& id) -> T* {
    for (auto& e : v)
      if (e.id == id)
        return &e;
    return nullptr;
  };
  auto findConstById = [](const std::vector<T>& v, const std::string& id) -> const T* {
    for (const auto& e : v)
      if (e.id == id)
        return &e;
    return nullptr;
  };

  for (const auto& was : baseline)
  {
    if (findConstById(current, was.id) != nullptr)
      continue; // still ours; not a removal
    target.erase(std::remove_if(target.begin(), target.end(), [&was](const T& e) { return e.id == was.id; }),
                 target.end());
  }

  for (const auto& mine : current)
  {
    const T* was = findConstById(baseline, mine.id);
    if (was != nullptr && SameContentItem(*was, mine))
      continue; // we did not touch it: whatever disk says stands
    if (T* existing = findById(target, mine.id))
      *existing = mine;
    else
      target.push_back(mine);
  }
}

inline void MergeContentPresetBanks(std::map<std::string, std::vector<Preset>>& target,
                                    const std::map<std::string, std::vector<Preset>>& baseline,
                                    const std::map<std::string, std::vector<Preset>>& current)
{
  static const std::vector<Preset> kEmpty;
  auto bankOf = [](const std::map<std::string, std::vector<Preset>>& m, const std::string& key)
    -> const std::vector<Preset>& {
    const auto it = m.find(key);
    return it == m.end() ? kEmpty : it->second;
  };

  // Owner keys only `target` knows are a sibling's banks; leave them alone.
  std::vector<std::string> keys;
  for (const auto& e : baseline)
    keys.push_back(e.first);
  for (const auto& e : current)
    if (baseline.find(e.first) == baseline.end())
      keys.push_back(e.first);

  for (const auto& key : keys)
  {
    auto& bank = target[key];
    MergeContentVector(bank, bankOf(baseline, key), bankOf(current, key));
    // An empty bank is how "this amp has no presets" is spelled everywhere else
    // (see DeletePreset), so do not leave an empty array behind.
    if (bank.empty())
      target.erase(key);
  }
}

inline void MergeContentMidiMap(std::map<int, MidiSoundAssignment>& target,
                                const std::map<int, MidiSoundAssignment>& baseline,
                                const std::map<int, MidiSoundAssignment>& current)
{
  for (const auto& was : baseline)
    if (current.find(was.first) == current.end())
      target.erase(was.first);

  for (const auto& mine : current)
  {
    const auto was = baseline.find(mine.first);
    if (was != baseline.end() && SameContentItem(was->second, mine.second))
      continue;
    target[mine.first] = mine.second;
  }
}

// The whole registry merge. `disk` is consumed as the base so a sibling's items
// survive; the result is what gets written and what this writer keeps in memory.
inline Registry MergeRegistries(const Registry& disk, const Registry& baseline, const Registry& current)
{
  Registry out = disk;
  MergeContentVector(out.amps, baseline.amps, current.amps);
  MergeContentVector(out.irs, baseline.irs, current.irs);
  MergeContentVector(out.pedals, baseline.pedals, current.pedals);
  MergeContentPresetBanks(out.presetBanks, baseline.presetBanks, current.presetBanks);
  MergeContentMidiMap(out.midiSoundMap, baseline.midiSoundMap, current.midiSoundMap);
  // Monotonic and never reused: the high-water mark of everyone who ever wrote,
  // or two writers importing a pedal each would alias one PRE capture index.
  out.nextPedalIndex = std::max({disk.nextPedalIndex, current.nextPedalIndex, kCustomPedalIndexBase});
  for (const auto& p : out.pedals)
    out.nextPedalIndex = std::max(out.nextPedalIndex, p.legacyIndex + 1);
  // A migration source, not shared state: keep whatever this writer still has to
  // drain so a save does not lose scenes it has not migrated yet.
  out.legacyCustomScenes = current.legacyCustomScenes;
  return out;
}

// ---------------------------------------------------------------------------
// Cross-process advisory lock
// ---------------------------------------------------------------------------
//
// Held on a sibling lock file, never on `volum-content.json` itself: the registry
// is replaced by rename on every write, so a lock on it would be a lock on a file
// that no longer exists.
//
// The lock has to die with the process. A cookie or a pid file does not - a VoLum
// that crashes or is force-quit while holding it leaves the library unwritable for
// everyone until someone deletes a stale file. `LockFileEx` and `flock` are both
// released by the kernel when the handle closes, and every handle closes when the
// process ends, however it ends.
class RegistryFileLock
{
public:
  RegistryFileLock() = default;
  ~RegistryFileLock() { Release(); }
  RegistryFileLock(const RegistryFileLock&) = delete;
  RegistryFileLock& operator=(const RegistryFileLock&) = delete;

  // Blocking with a ceiling. A lock is held only across one read-merge-write, so
  // waiting seconds means the holder is wedged, not busy; a caller that gives up
  // reports a write failure rather than hanging the audio app's UI thread.
  bool Acquire(const std::filesystem::path& lockFile, int timeoutMs = 4000)
  {
    if (Held())
      return true;
    if (lockFile.empty())
      return false;
    std::error_code ec;
    const auto parent = lockFile.parent_path();
    if (!parent.empty())
      std::filesystem::create_directories(parent, ec);

    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMs);
    for (;;)
    {
      if (TryAcquireOnce(lockFile))
        return true;
      if (std::chrono::steady_clock::now() >= deadline)
        return false;
      std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
  }

  void Release()
  {
#ifdef _WIN32
    if (mHandle == nullptr)
      return;
    OVERLAPPED ov{};
    UnlockFileEx(static_cast<HANDLE>(mHandle), 0, 1, 0, &ov);
    CloseHandle(static_cast<HANDLE>(mHandle));
    mHandle = nullptr;
#else
    if (mFd < 0)
      return;
    ::flock(mFd, LOCK_UN);
    ::close(mFd);
    mFd = -1;
#endif
  }

  bool Held() const
  {
#ifdef _WIN32
    return mHandle != nullptr;
#else
    return mFd >= 0;
#endif
  }

  // Drop the OS lock the way a crash does: the handle goes away without any
  // orderly unlock or cleanup. Exists so a test can prove a killed holder does
  // not stuck-lock the library; there is no production caller.
  void SimulateProcessDeath()
  {
#ifdef _WIN32
    if (mHandle == nullptr)
      return;
    CloseHandle(static_cast<HANDLE>(mHandle));
    mHandle = nullptr;
#else
    if (mFd < 0)
      return;
    ::close(mFd);
    mFd = -1;
#endif
  }

private:
  bool TryAcquireOnce(const std::filesystem::path& lockFile)
  {
#ifdef _WIN32
    HANDLE h = CreateFileW(lockFile.c_str(), GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr,
                           OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE)
      return false;
    OVERLAPPED ov{};
    if (!LockFileEx(h, LOCKFILE_EXCLUSIVE_LOCK | LOCKFILE_FAIL_IMMEDIATELY, 0, 1, 0, &ov))
    {
      CloseHandle(h);
      return false;
    }
    mHandle = h;
    return true;
#else
    const int fd = ::open(lockFile.c_str(), O_RDWR | O_CREAT, 0644);
    if (fd < 0)
      return false;
    if (::flock(fd, LOCK_EX | LOCK_NB) != 0)
    {
      ::close(fd);
      return false;
    }
    mFd = fd;
    return true;
#endif
  }

#ifdef _WIN32
  void* mHandle = nullptr;
#else
  int mFd = -1;
#endif
};

// One mutex for every ContentStore in this process. The cross-process lock is
// per-handle and does not serialize threads inside one process (LockFileEx is
// re-entrant for the same handle, flock's semantics are per-fd), and two plugin
// instances on two host threads do reach the store at the same time.
inline std::recursive_mutex& ContentStoreMutex()
{
  static std::recursive_mutex m;
  return m;
}

// ---------------------------------------------------------------------------
// ContentStore: paths + load/save + file import + removal matrix
// ---------------------------------------------------------------------------

class ContentStore
{
public:
  ContentStore() = default;
  explicit ContentStore(std::filesystem::path baseDir)
  : mBase(std::move(baseDir))
  {
  }

  void SetBaseDir(std::filesystem::path baseDir) { mBase = std::move(baseDir); }
  const std::filesystem::path& BaseDir() const { return mBase; }

  std::filesystem::path RegistryPath() const { return mBase / "volum-content.json"; }
  std::filesystem::path BackupPath() const { return mBase / "volum-content.json.bak"; }
  // Sibling of the registry, never the registry itself: see RegistryFileLock.
  std::filesystem::path LockPath() const { return mBase / "volum-content.lock"; }
  // Pre-migration snapshot, kept separate from the corrupt-file .bak so a later
  // parse failure cannot overwrite the last known-good pre-upgrade copy.
  std::filesystem::path MigrationBackupPath(const std::string& tag) const
  {
    return mBase / ("volum-content.json.pre-" + tag + ".bak");
  }
  std::filesystem::path AmpsDir() const { return mBase / "amps"; }
  std::filesystem::path IrDir() const { return mBase / "ir"; }
  std::filesystem::path PedalsDir() const { return mBase / "pedals"; }

  Registry& reg() { return mReg; }
  const Registry& reg() const { return mReg; }

  // False only for an unconfigured store (unit tests), where imports have nowhere
  // to copy to and callers legitimately fall back to bare filenames.
  bool HasBaseDir() const { return !mBase.empty(); }

  // Absolute path for a registry-relative stored file ("ir/ir_x.wav"). Empty when
  // relPath would resolve outside the content directory, which makes every caller
  // - loads and deletes alike - treat it as missing rather than as a file to act
  // on. See IsSafeStoredRelPath.
  std::filesystem::path ResolveStored(const std::string& relPath) const
  {
    if (!IsSafeStoredRelPath(relPath))
      return {};
    return mBase / PathFromUtf8(relPath);
  }

  // Load the registry unless this process already has one in memory.
  //
  // Every plugin instance's constructor reaches the process-global store, and the
  // second one calling Load() re-read the file over a live sibling's catalog: an
  // import or a preset saved but not yet flushed simply disappeared, and the next
  // Save() persisted the version without it. A catalog already in memory is at
  // least as new as the file, so the second constructor has nothing to gain by
  // reading it again.
  //
  // Tests and the base-dir switch still call Load() directly when re-reading is
  // the point.
  bool EnsureLoaded()
  {
    std::lock_guard<std::recursive_mutex> guard(ContentStoreMutex());
    // Unflushed changes are the reason this method exists, so they veto the read
    // on their own - not merely as a side effect of some earlier Load() having
    // set the flag.
    if (mLoaded || HasUnflushedChanges())
      return true;
    return Load();
  }

  bool IsLoaded() const { return mLoaded; }

  // True when this store holds catalog changes that are not on disk yet. Used by
  // the write-failure banner and by tests; a merge makes it false again.
  bool HasUnflushedChanges() const
  {
    std::lock_guard<std::recursive_mutex> guard(ContentStoreMutex());
    return RegistryToJson(mReg) != RegistryToJson(mBaseline);
  }

  // Load the registry. Missing file -> empty registry. Unparseable / wrong-shape
  // file -> moved to .bak and we start from defaults. Returns true on a clean
  // (non-healed, non-recovered) load.
  bool Load()
  {
    std::lock_guard<std::recursive_mutex> guard(ContentStoreMutex());
    mReg = Registry{};
    mBaseline = Registry{};
    mLoaded = true;
    mRegistryUnreadable = false;
    // Each pending delete describes a registry we are about to throw away. A
    // delete whose Save() failed is still listed on disk, so carrying the queue
    // across a reload would let the next successful save destroy a payload the
    // freshly read registry still names - the exact state deferring the delete
    // was meant to prevent. Every plugin instance's constructor calls Load() on
    // the process-global store, so this is one new track away in any DAW.
    mPendingFileDeletes.clear();
    const auto path = RegistryPath();
    std::error_code ec;
    if (mBase.empty() || !std::filesystem::exists(path, ec))
      return true;

    // Something exists at the library's path that is not a file: a directory, a
    // device, a broken sync artefact. Checked before opening because it is not
    // portable to detect afterwards - Windows refuses to open a directory, while
    // libc++ opens it and only fails on the first read, which is indistinguishable
    // from an empty file and so went down the "corrupt, back it up and rewrite"
    // path. Whatever it is, it is not ours to replace.
    if (!std::filesystem::is_regular_file(path, ec))
    {
      mRegistryUnreadable = true;
      return false;
    }

    std::ifstream in(path, std::ios::binary);
    if (!in.good())
    {
      // The library exists but something else is holding it: antivirus, a backup
      // or cloud-sync agent, another VoLum, or a permissions change. Reporting a
      // clean empty registry here was the worst possible answer, because the next
      // save serialized that emptiness over a perfectly good file and took every
      // custom amp, IR, pedal and preset bank with it. Refuse to write until a
      // load succeeds instead; the user loses the session's edits, not the library.
      mRegistryUnreadable = true;
      return false;
    }

    nlohmann::json j;
    try
    {
      in >> j;
    }
    catch (...)
    {
      in.close();
      BackupCorrupt();
      mReg = Registry{};
      return false;
    }
    in.close();

    if (!j.is_object())
    {
      BackupCorrupt();
      mReg = Registry{};
      return false;
    }

    bool healed = false;
    mReg = RegistryFromJson(j, &healed);
    mBaseline = mReg;
    return !healed;
  }

  // True when a library file exists on disk that we were unable to read, so what
  // is in memory is not what is on disk and must not be written over it.
  bool RegistryUnreadable() const { return mRegistryUnreadable; }

  // Set whenever a Save() did not reach the disk, cleared by the next one that
  // does. Most mutators are void and cannot report a failure of their own, so
  // callers that want to tell the user consult this after the operation. Taking
  // it clears it, because a banner should be shown once.
  bool TakeWriteFailure()
  {
    const bool failed = mLastWriteFailed;
    mLastWriteFailed = false;
    return failed;
  }

  // Locked read-modify-write. Under one cross-process lock: re-read the file,
  // replay this writer's id-level changes onto it (MergeRegistries), write, and
  // adopt the merged result as both the live catalog and the new baseline.
  //
  // A plain write here was the two-writer bug: an IR imported in standalone was
  // gone the next time the DAW instance saved a preset, because the DAW's copy of
  // the catalog predated the import.
  bool Save()
  {
    std::lock_guard<std::recursive_mutex> guard(ContentStoreMutex());
    if (mBase.empty())
    {
      // Intentionally in-memory (unit tests / unconfigured store). Nothing can be
      // merged, but the baseline still has to advance or every later Save would
      // replay the whole session as a change set.
      mBaseline = mReg;
      return true;
    }
    if (mRegistryUnreadable)
    {
      // See Load(): never overwrite a library we could not read.
      mLastWriteFailed = true;
      return false;
    }

    std::error_code ec;
    std::filesystem::create_directories(mBase, ec);
    if (ec)
    {
      // No library directory means no lock file either, so fail before taking a
      // lock we cannot hold and reporting a merge we cannot perform.
      mLastWriteFailed = true;
      return false;
    }

    RegistryFileLock lock;
    if (!lock.Acquire(LockPath()))
    {
      // Someone is wedged holding the lock. Refusing is the honest answer: the
      // alternative is writing without serialization, which is the defect.
      mLastWriteFailed = true;
      return false;
    }

    Registry merged = MergeRegistries(ReadRegistryFromDisk(), mBaseline, mReg);
    if (!WriteJsonAtomically(RegistryPath(), RegistryToJson(merged), ec))
    {
      mLastWriteFailed = true;
      return false;
    }
    mReg = std::move(merged);
    mBaseline = mReg;
    mLastWriteFailed = false;
    // A merged write read the file as part of doing it, so memory now matches
    // disk and a later EnsureLoaded() has nothing to fetch.
    mLoaded = true;

    // Payload files are destroyed only once the registry that no longer mentions
    // them is durable. Doing it the other way round meant a failed registry write
    // left the on-disk library pointing at files VoLum had already deleted: the
    // item came back on restart and could never load again. If this write fails
    // the deletions stay pending, and both the entry and its file survive - which
    // is the state the on-disk registry still describes.
    FlushPendingFileDeletes();
    return true;
  }

  // Snapshot the on-disk registry once, before a migration rewrites it in place.
  // A one-way migration is the one moment the user cannot get their old file back
  // by downgrading, so keep a copy. Does nothing if a snapshot for this tag already
  // exists (so it stays a true pre-upgrade copy) or if there is nothing to copy.
  // Returns true when a snapshot exists afterwards.
  bool BackupBeforeMigration(const std::string& tag)
  {
    if (mBase.empty())
      return false;
    std::error_code ec;
    const auto src = RegistryPath();
    if (!std::filesystem::exists(src, ec))
      return false;
    const auto dst = MigrationBackupPath(tag);
    if (std::filesystem::exists(dst, ec))
      return true; // already snapshotted on an earlier attempt; never overwrite
    std::filesystem::copy_file(src, dst, std::filesystem::copy_options::overwrite_existing, ec);
    return !ec;
  }

  // Copy `src` into `subDir` (one of amps/ir/pedals) under a unique stored name,
  // returning the registry-relative path ("ir/ir_x__leaf.wav"). Empty on failure.
  std::string ImportFileCopy(const std::filesystem::path& src, const std::string& subDir, const std::string& idPrefix)
  {
    std::error_code ec;
    if (mBase.empty() || src.empty() || !std::filesystem::exists(src, ec))
      return {};
    const auto dstDir = mBase / subDir;
    std::filesystem::create_directories(dstDir, ec);
    if (ec)
      return {};

    const std::string leaf = PathToUtf8(src.filename());
    const std::string stored = idPrefix + "__" + leaf;
    const auto dst = dstDir / PathFromUtf8(stored);
    std::filesystem::copy_file(src, dst, std::filesystem::copy_options::overwrite_existing, ec);
    if (ec)
      return {};
    return subDir + "/" + stored;
  }

  // Delete a file a transaction copied in but never committed to the registry.
  // Nothing on disk refers to it, so there is no ordering to respect, and waiting
  // for a Save that a failed import will never perform would only orphan it.
  //
  // Payloads the committed registry *does* reference are a different problem and
  // go through QueueStoredFileDelete instead.
  void RemoveStoredFile(const std::string& relPath)
  {
    const auto resolved = ResolveStored(relPath);
    if (resolved.empty())
      return;
    std::error_code ec;
    std::filesystem::remove(resolved, ec);
  }

  // -- Removal matrix (spec 3.7) ------------------------------------------------

  // Delete a custom pedal: drop the library entry + file, and clear every PRE
  // slot (in scenes/presets) that pointed at its legacy index back to EMPTY (0).
  // `id` is taken by value: callers commonly pass a reference to the string owned
  // by the element we erase below, which would otherwise dangle.
  void RemovePedal(std::string id)
  {
    int legacyIndex = -1;
    for (auto it = mReg.pedals.begin(); it != mReg.pedals.end(); ++it)
    {
      if (it->id == id)
      {
        legacyIndex = it->legacyIndex;
        QueueStoredFileDelete(it->file);
        mReg.pedals.erase(it);
        break;
      }
    }
    if (legacyIndex < 0)
      return;
    // Catalog only. The sounding rig belongs to the instance now, so what happens
    // to a PRE slot that is playing this pedal *right now* is the instance's
    // business (see VoLumContentRemovalPlan.h), not a scene rewrite from here.
    auto clearSlots = [legacyIndex](VoLumAmpSettings& s) {
      if (s.preNam1Capture == legacyIndex)
        s.preNam1Capture = 0;
      if (s.preNam2Capture == legacyIndex)
        s.preNam2Capture = 0;
    };
    for (auto& bank : mReg.presetBanks)
      for (auto& pr : bank.second)
        clearSlots(pr.settings);
  }

  // Delete a custom IR: drop the library entry + file, and clear activeIrId on
  // every scene/preset that referenced it (falls back to the baked cab).
  // `id` is by value: it is used after we erase the owning IRItem below, so a
  // reference (e.g. DeleteIR passing irs[idx].id) would dangle (use-after-free).
  void RemoveIR(std::string id)
  {
    for (auto it = mReg.irs.begin(); it != mReg.irs.end(); ++it)
    {
      if (it->id == id)
      {
        QueueStoredFileDelete(it->file);
        mReg.irs.erase(it);
        break;
      }
    }
    auto clearIr = [&id](VoLumAmpSettings& s) {
      if (s.activeIrId == id)
        s.activeIrId.clear();
      if (s.supportActiveIrId == id)
        s.supportActiveIrId.clear();
    };
    for (auto& bank : mReg.presetBanks)
      for (auto& pr : bank.second)
        clearIr(pr.settings);
  }

  // Delete a custom amp: drop the manifest + copied files, its preset bank, its
  // scene, and reset any support ref pointing at it to "(none)".
  // `id` is by value: it is used (presetBanks/customScenes erase, clearSupport)
  // after we erase the owning amp below, so a reference would dangle.
  void RemoveCustomAmp(std::string id)
  {
    for (auto it = mReg.amps.begin(); it != mReg.amps.end(); ++it)
    {
      if (it->id == id)
      {
        // Delete by `storedPath` (e.g. "amps/amp_x__G65.nam"), the registry-relative
        // path. `file` is only the display leaf, so deleting by it would resolve to
        // the wrong place and orphan the copied file on disk.
        for (const auto& f : it->files)
          QueueStoredFileDelete(f.storedPath);
        mReg.amps.erase(it);
        break;
      }
    }
    mReg.presetBanks.erase(id);
    mReg.legacyCustomScenes.erase(id);
    auto clearSupport = [&id](VoLumAmpSettings& s) {
      if (s.supportCustomId == id)
        s.supportCustomId.clear();
    };
    for (auto& bank : mReg.presetBanks)
      for (auto& pr : bank.second)
        clearSupport(pr.settings);
    // A MIDI slot pointing at this amp keeps its number and goes invalid (red) -
    // the locked answer in the delete-while-playing ticket. Silently deleting the
    // row would renumber the player's slots behind their back, so the assignment
    // stays and only stops resolving. See ResolveMidiSlot.
  }

  // -- MIDI sound map ------------------------------------------------------------

  // Point a slot at a Sound. Empty ampId clears the slot (unassigned), which is
  // what "clear" means to the player - as opposed to invalid, which keeps the row.
  void SetMidiSlot(int slot, const std::string& ampId, const std::string& presetId)
  {
    std::lock_guard<std::recursive_mutex> guard(ContentStoreMutex());
    if (ampId.empty() && presetId.empty())
    {
      mReg.midiSoundMap.erase(slot);
      return;
    }
    mReg.midiSoundMap[slot] = MidiSoundAssignment{ampId, presetId};
  }

  void ClearMidiSlot(int slot)
  {
    std::lock_guard<std::recursive_mutex> guard(ContentStoreMutex());
    mReg.midiSoundMap.erase(slot);
  }

private:
  // The registry exactly as it is on disk right now, for the merge in Save(). Any
  // failure yields an empty registry, which makes the merge degrade to "replay my
  // changes onto nothing" - the pre-1.3.0 behaviour, and the best available when
  // the file cannot be read. Load() is what refuses to write over a file it could
  // not read; by the time Save() runs, that verdict has already been made.
  Registry ReadRegistryFromDisk() const
  {
    std::error_code ec;
    const auto path = RegistryPath();
    if (mBase.empty() || !std::filesystem::is_regular_file(path, ec))
      return Registry{};
    std::ifstream in(path, std::ios::binary);
    if (!in.good())
      return Registry{};
    nlohmann::json j;
    try
    {
      in >> j;
    }
    catch (...)
    {
      return Registry{};
    }
    if (!j.is_object())
      return Registry{};
    return RegistryFromJson(j);
  }

  // Queue a payload the committed registry still references. It is deleted by the
  // next successful Save(), never before: see the comment there.
  void QueueStoredFileDelete(const std::string& relPath)
  {
    if (relPath.empty())
      return;
    mPendingFileDeletes.push_back(relPath);
  }

  // True when the registry about to be written still names this payload. Deleting
  // such a file would leave an entry that can never load again, which is the one
  // outcome the whole deferred-delete scheme exists to avoid, so it is checked at
  // the point of no return rather than trusted from the queue.
  // Two entries can name one payload in spellings that differ as text but resolve to
  // the same file: `ir/Foo.wav` against `ir/foo.wav` on Windows and macOS's default
  // volume, or a backslash where VoLum writes a forward slash. That survives a
  // library restored from a backup or merged from two machines. A byte comparison
  // would miss the surviving entry and delete the payload it still names, so compare
  // the way the filesystem does.
  static bool SameStoredPath(const std::string& a, const std::string& b)
  {
    if (a.size() != b.size())
      return false;
    for (std::size_t i = 0; i < a.size(); ++i)
    {
      const unsigned char ca = static_cast<unsigned char>(a[i]);
      const unsigned char cb = static_cast<unsigned char>(b[i]);
      const bool sepA = ca == '/' || ca == '\\';
      const bool sepB = cb == '/' || cb == '\\';
      if (sepA || sepB)
      {
        if (!(sepA && sepB))
          return false;
        continue;
      }
      if (std::tolower(ca) != std::tolower(cb))
        return false;
    }
    return true;
  }

  bool RegistryReferences(const std::string& relPath) const
  {
    if (relPath.empty())
      return false;
    for (const auto& ir : mReg.irs)
      if (SameStoredPath(ir.file, relPath))
        return true;
    for (const auto& p : mReg.pedals)
      if (SameStoredPath(p.file, relPath))
        return true;
    for (const auto& amp : mReg.amps)
      for (const auto& f : amp.files)
        if (SameStoredPath(f.storedPath, relPath))
          return true;
    return false;
  }

  void FlushPendingFileDeletes()
  {
    for (const auto& relPath : mPendingFileDeletes)
    {
      if (RegistryReferences(relPath))
        continue; // the registry we just wrote still names it
      const auto resolved = ResolveStored(relPath);
      if (resolved.empty())
        continue; // outside the content directory; not ours to delete
      std::error_code ec;
      std::filesystem::remove(resolved, ec);
    }
    mPendingFileDeletes.clear();
  }

  void BackupCorrupt()
  {
    std::error_code ec;
    std::filesystem::rename(RegistryPath(), BackupPath(), ec);
    if (ec)
    {
      std::filesystem::copy_file(RegistryPath(), BackupPath(), std::filesystem::copy_options::overwrite_existing, ec);
      std::filesystem::remove(RegistryPath(), ec);
    }
  }

  std::filesystem::path mBase;
  Registry mReg;
  // What this writer last read or wrote. The merge in Save() diffs mReg against
  // it to learn which ids *this* writer changed; everything else belongs to disk.
  Registry mBaseline;
  bool mLoaded = false;
  bool mRegistryUnreadable = false;
  bool mLastWriteFailed = false;
  std::vector<std::string> mPendingFileDeletes;
};

// Process-wide content store. The custom-content library (amps / IRs / pedals /
// preset banks) is shared user data, so all plugin instances in one host share
// it - exactly what we want for a global library. Per-instance/project state
// (active amp, live scene) lives in the plugin instance + DAW chunk, not here.
//
// In unit tests the base dir is left empty, so Save() is a no-op and nothing
// touches the real user content directory.
inline ContentStore& GlobalContentStore()
{
  static ContentStore store;
  return store;
}

} // namespace content
} // namespace volum
