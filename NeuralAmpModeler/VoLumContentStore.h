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
#include <filesystem>
#include <fstream>
#include <map>
#include <random>
#include <string>
#include <vector>

#include "VoLumAmpSettingsJson.h"
#include "VoLumCustomModel.h" // volum::custom::CustomAmp + pure helpers
#include "VoLumSettingsFileIO.h" // WriteJsonAtomically

namespace volum
{
namespace content
{

// v3 (VoLum 1.2.1) adds per-IR shaping (trimDb / lowCutHz / highCutHz) to each
// irLibrary entry. The reader is additive/forward-tolerant (unknown keys ignored,
// missing keys defaulted), so v2 files load unchanged and v3 files load in older
// builds; the bump is only a marker of the new capability, not a migration gate.
inline constexpr int kContentSchemaVersion = 3;

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

inline double StepIrLadder(const double* ladder, int n, double cur, int dir)
{
  int best = 0;
  double bestD = 1e18;
  for (int i = 0; i < n; ++i)
  {
    const double d = std::fabs(ladder[i] - cur);
    if (d < bestD)
    {
      bestD = d;
      best = i;
    }
  }
  const int ni = std::clamp(best + (dir >= 0 ? 1 : -1), 0, n - 1);
  return ladder[ni];
}

inline double StepIrLowCutHz(double hz, int dir)
{
  // OFF at the bottom, then a musical low-cut sweep up to the low mids.
  static const double kLadder[] = {0.0, 20, 30, 40, 50, 60, 80, 100, 120, 150, 200, 300, 400, 500, 650, 800};
  const int n = static_cast<int>(sizeof(kLadder) / sizeof(kLadder[0]));
  return ClampIrLowCutHz(StepIrLadder(kLadder, n, ClampIrLowCutHz(hz), dir));
}

inline double StepIrHighCutHz(double hz, int dir)
{
  // Ascending presence/air ladder with OFF (0) as the top "fully open" rung, so
  // stepping DOWN from OFF engages a 20 kHz cut and keeps darkening.
  static const double kLadder[] = {1000, 1500,  2000,  3000,  4000,  5000, 6000,
                                   8000, 10000, 12000, 16000, 20000, 0.0};
  const int n = static_cast<int>(sizeof(kLadder) / sizeof(kLadder[0]));
  const double in = (hz > 0.0) ? ClampIrHighCutHz(hz) : 0.0;
  return ClampIrHighCutHz(StepIrLadder(kLadder, n, in, dir));
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
  std::map<std::string, VoLumAmpSettings> customScenes; // ampId -> live scene
  int nextPedalIndex = kCustomPedalIndexBase; // monotonic, never reused
};

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

  nlohmann::json scenes = nlohmann::json::object();
  for (const auto& sc : r.customScenes)
    scenes[sc.first] = AmpSettingsToJson(sc.second);
  j["customScenes"] = scenes;

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

  if (j.contains("customScenes") && j["customScenes"].is_object())
  {
    for (const auto& sc : j["customScenes"].items())
    {
      if (!sc.value().is_object())
        continue;
      VoLumAmpSettings settings;
      if (AmpSettingsFromJson(sc.value(), settings))
        h = true;
      r.customScenes[sc.key()] = settings;
    }
  }

  if (healed)
    *healed = h;
  return r;
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
  std::filesystem::path AmpsDir() const { return mBase / "amps"; }
  std::filesystem::path IrDir() const { return mBase / "ir"; }
  std::filesystem::path PedalsDir() const { return mBase / "pedals"; }

  Registry& reg() { return mReg; }
  const Registry& reg() const { return mReg; }

  // Absolute path for a registry-relative stored file ("ir/ir_x.wav").
  std::filesystem::path ResolveStored(const std::string& relPath) const
  {
    if (relPath.empty())
      return {};
    return mBase / std::filesystem::path(relPath);
  }

  // Load the registry. Missing file -> empty registry. Unparseable / wrong-shape
  // file -> moved to .bak and we start from defaults. Returns true on a clean
  // (non-healed, non-recovered) load.
  bool Load()
  {
    mReg = Registry{};
    const auto path = RegistryPath();
    std::error_code ec;
    if (mBase.empty() || !std::filesystem::exists(path, ec))
      return true;

    std::ifstream in(path, std::ios::binary);
    if (!in.good())
      return true;

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
    return !healed;
  }

  bool Save()
  {
    if (mBase.empty())
      return false;
    std::error_code ec;
    std::filesystem::create_directories(mBase, ec);
    return WriteJsonAtomically(RegistryPath(), RegistryToJson(mReg), ec);
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

    const std::string leaf = src.filename().string();
    const std::string stored = idPrefix + "__" + leaf;
    const auto dst = dstDir / stored;
    std::filesystem::copy_file(src, dst, std::filesystem::copy_options::overwrite_existing, ec);
    if (ec)
      return {};
    return subDir + "/" + stored;
  }

  void RemoveStoredFile(const std::string& relPath)
  {
    if (relPath.empty())
      return;
    std::error_code ec;
    std::filesystem::remove(ResolveStored(relPath), ec);
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
        RemoveStoredFile(it->file);
        mReg.pedals.erase(it);
        break;
      }
    }
    if (legacyIndex < 0)
      return;
    auto clearSlots = [legacyIndex](VoLumAmpSettings& s) {
      if (s.preNam1Capture == legacyIndex)
        s.preNam1Capture = 0;
      if (s.preNam2Capture == legacyIndex)
        s.preNam2Capture = 0;
    };
    for (auto& sc : mReg.customScenes)
      clearSlots(sc.second);
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
        RemoveStoredFile(it->file);
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
    for (auto& sc : mReg.customScenes)
      clearIr(sc.second);
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
        // RemoveStoredFile resolves a registry-relative path; for amp manifests that
        // is `storedPath` (e.g. "amps/amp_x__G65.nam"). `file` is only the display
        // leaf, so deleting by it would miss the copied file and orphan it on disk.
        for (const auto& f : it->files)
          RemoveStoredFile(f.storedPath);
        mReg.amps.erase(it);
        break;
      }
    }
    mReg.presetBanks.erase(id);
    mReg.customScenes.erase(id);
    auto clearSupport = [&id](VoLumAmpSettings& s) {
      if (s.supportCustomId == id)
        s.supportCustomId.clear();
    };
    for (auto& sc : mReg.customScenes)
      clearSupport(sc.second);
    for (auto& bank : mReg.presetBanks)
      for (auto& pr : bank.second)
        clearSupport(pr.settings);
  }

private:
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
