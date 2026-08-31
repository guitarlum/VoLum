#pragma once

// VoLum 1.2.0 DAW-chunk id tail.
//
// The legacy per-amp chunk tail (VoLumChunkCodec.h) is a fixed-size binary block
// whose presence is detected purely by counting remaining bytes
// (VoLumChunkLayout.h). The 1.2.0 BYO/preset features add *reference* state that
// is naturally string-shaped (stable opaque ids), so it cannot live in that
// byte-counted block without breaking the size detectors.
//
// Instead we append a single self-describing, sentinel-guarded section at the
// very end of the chunk:
//
//   [int sentinel = kVoLumIdTailSentinel][int len][len bytes of JSON]
//
// - Older builds read exactly the fixed tail and stop, ignoring these trailing
//   bytes (forward-compatible).
// - The byte-count detectors only ever gate blocks that are *all present* on a
//   current chunk, and use monotonic `>=` comparisons advanced by explicit
//   reads, so extra trailing bytes never cause a fixed block to be misdetected.
// - A 1.2.0 reader, after the fixed tail, probes for the sentinel; if absent
//   (older chunk) or the JSON is malformed, it leaves all refs empty (lenient,
//   never throws). The custom content itself lives in the shared content library
//   (volum-content.json); the chunk stores only ids that resolve against it.
//
// The id tail carries per (factory) amp activeIrId / supportCustomId (these are
// strings and so never travelled in the binary per-amp block), plus the focused
// custom MAIN / SUPPORT amp ids and the active preset id for the focused amp.

#include <string>

#include "VoLumAmpeteCatalog.h"

#if __has_include(<nlohmann/json.hpp>)
  #include <nlohmann/json.hpp>
#elif __has_include(<json.hpp>)
  #include <json.hpp>
#else
  #error "nlohmann json header not found (expected iPlug Dependencies/Extras layout)"
#endif

namespace volum
{

// 'V''L''I''D' - distinctive enough that a stray trailing int won't collide.
inline constexpr int kVoLumIdTailSentinel = 0x564C4944;
// Schema 3 (1.2.0): added per-amp support-lane custom IR id ("supIr", schema 2)
// and the custom SUPPORT partner cab/channel ("supCab"/"supCh", schema 3). The
// tail is a self-describing JSON object, so the bump is informational - both old
// and new readers tolerate missing/extra keys; nothing branches on the version.
// Schema 4 (post-tremolo): added per-amp POST Tremolo ("trem") and the
// live-locked POST tremolo snapshot ("lockedPostTremolo"). Informational only;
// readers tolerate missing/extra keys and never branch on the version.
// Schema 5 (delay-sync): added per-amp POST Delay tempo sync ("dly": sync +
// division) and the live-locked POST delay snapshot ("lockedPostDelay"). The
// delay's other params still travel in the binary per-amp block; only the
// appended sync/division pair needs the tail. Informational only.
// Schema 6 (post-chorus): added per-amp POST Chorus ("cho") and the live-locked
// POST chorus snapshot ("lockedPostChorus"). Chorus is the first feature whose
// EParams sit past the frozen 1.2.2 chunk prefix, so the tail is the ONLY place
// its saved values travel - never as extra prefix doubles. Informational only.
inline constexpr int kVoLumIdTailSchema = 6;

// PRE Pitch pedal per-amp settings. Carried in the JSON id tail (not the binary
// per-amp block) so the byte-counted size detectors stay untouched. `present`
// distinguishes "written by a pitch-aware build" from "absent" (older chunk ->
// pitch defaults = bypassed).
struct PitchTail
{
  bool present = false;
  bool active = false;
  int mode = 0; // 0=Transpose, 1=Octaver
  double semitones = 0.0;
  double mix = 1.0;
  double octDown = 0.0;
  double octUp = 0.0;
  double dry = 1.0;
  int voicing = 1; // 0=Vintage, 1=Modern
  double level = 0.0;
  int transChar = 1; // 0=Drop, 1=Instant (default; transpose engine character)
  // Per-mode knob memory (Transpose / Octaver). Default-constructed
  // PitchModeSnapshot already carries the ship defaults.
  PitchModeSnapshot modes[kVoLumPitchModeCount];
};

// POST Tremolo pedal per-amp settings. Carried in the JSON id tail alongside the
// pitch tail. `present` distinguishes "written by a tremolo-aware build" from
// "absent" (older chunk -> tremolo defaults = bypassed).
struct TremoloTail
{
  bool present = false;
  bool active = false;
  int mode = kVoLumTremoloModeBias;
  double rate = 5.0;
  double depth = 0.85;
  double shape = 0.0;
  double mix = 1.0;
  double crossover = 800.0;
  bool sync = false;
  int division = kVoLumTremoloDivisionDefault;
  // Per-mode knob memory (Optical / Bias / Harmonic). Default-constructed
  // TremoloModeSnapshot already carries the ship defaults.
  TremoloModeSnapshot modes[kVoLumTremoloModeCount];
};

// POST Delay tempo-sync per-amp settings. Only the sync toggle + division are
// carried here; the rest of the delay state already travels in the binary
// per-amp block. `present` distinguishes "written by a sync-aware build" from
// "absent" (older chunk -> sync defaults off).
struct DelayTail
{
  bool present = false;
  bool sync = false;
  int division = kVoLumTremoloDivisionDefault;
};

// POST Chorus pedal per-amp settings. Chorus params live PAST the frozen 1.2.2
// prefix, so unlike Delay/Reverb nothing about this pedal is in the binary block:
// the whole knob row travels here. `present` distinguishes "written by a
// chorus-aware build" from "absent" (older chunk -> chorus defaults = bypassed).
struct ChorusTail
{
  bool present = false;
  bool active = false;
  int mode = kVoLumChorusModeDefault;
  double rate = 0.35;
  double depth = 0.45;
  double tone = 0.40;
  double width = 0.70;
  double mix = 0.50;
  // Per-mode knob memory (Classic / Warped / Clear / Ensemble).
  ChorusModeSnapshot modes[kVoLumChorusModeCount] = {
    kVoLumChorusModeDefaults[0],
    kVoLumChorusModeDefaults[1],
    kVoLumChorusModeDefaults[2],
    kVoLumChorusModeDefaults[3],
  };
};

struct ChunkIdTail
{
  std::string customMainId; // focused custom MAIN amp id ("" = factory main)
  std::string customSupportId; // custom dual SUPPORT partner id ("" = factory/none)
  std::string activePresetId; // recalled preset id for the focused amp ("" = none)
  std::string perAmpIrId[kAmpCount]; // factory amp -> active custom IR cab id
  std::string perAmpSupportIrId[kAmpCount]; // factory amp -> SUPPORT lane custom IR id
  std::string perAmpSupportId[kAmpCount]; // factory amp -> custom support partner id
  int perAmpSupportSlot[kAmpCount]; // factory amp -> custom support cab slot (-2 = unset)
  int perAmpSupportChannel[kAmpCount]; // factory amp -> custom support gain stage (0 = unset)
  PitchTail perAmpPitch[kAmpCount]; // factory amp -> PRE Pitch pedal settings
  PitchTail lockedPrePitch; // live-locked PRE pitch snapshot (present iff PRE locked + written)
  TremoloTail perAmpTremolo[kAmpCount]; // factory amp -> POST Tremolo pedal settings
  TremoloTail lockedPostTremolo; // live-locked POST tremolo snapshot (present iff POST locked + written)
  DelayTail perAmpDelay[kAmpCount]; // factory amp -> POST Delay tempo-sync settings
  DelayTail lockedPostDelay; // live-locked POST delay sync snapshot (present iff POST locked + written)
  ChorusTail perAmpChorus[kAmpCount]; // factory amp -> POST Chorus pedal settings
  ChorusTail lockedPostChorus; // live-locked POST chorus snapshot (present iff POST locked + written)

  ChunkIdTail()
  {
    for (int i = 0; i < kAmpCount; ++i)
    {
      perAmpSupportSlot[i] = -2;
      perAmpSupportChannel[i] = 0;
    }
  }
};

inline nlohmann::json PitchTailToJson(const PitchTail& p)
{
  nlohmann::json modes = nlohmann::json::array();
  for (int i = 0; i < kVoLumPitchModeCount; ++i)
    modes.push_back(
      {{"mix", p.modes[i].mix}, {"dry", p.modes[i].dry}, {"level", p.modes[i].level}, {"voice", p.modes[i].voicing}});
  return nlohmann::json{{"active", p.active}, {"mode", p.mode},       {"semi", p.semitones}, {"mix", p.mix},
                        {"octDn", p.octDown}, {"octUp", p.octUp},     {"dry", p.dry},        {"voice", p.voicing},
                        {"level", p.level},   {"tchar", p.transChar}, {"modes", modes}};
}

inline PitchTail PitchTailFromJson(const nlohmann::json& j)
{
  PitchTail p;
  if (!j.is_object())
    return p;
  auto num = [](const nlohmann::json& v, double d) { return v.is_number() ? v.get<double>() : d; };
  auto integer = [](const nlohmann::json& v, int d) { return v.is_number_integer() ? v.get<int>() : d; };
  auto boolean = [](const nlohmann::json& v, bool d) { return v.is_boolean() ? v.get<bool>() : d; };
  if (j.contains("active"))
    p.active = boolean(j["active"], false);
  if (j.contains("mode"))
    p.mode = integer(j["mode"], 0);
  if (j.contains("semi"))
    p.semitones = num(j["semi"], 0.0);
  if (j.contains("mix"))
    p.mix = num(j["mix"], 1.0);
  if (j.contains("octDn"))
    p.octDown = num(j["octDn"], 0.0);
  if (j.contains("octUp"))
    p.octUp = num(j["octUp"], 0.0);
  if (j.contains("dry"))
    p.dry = num(j["dry"], 1.0);
  if (j.contains("voice"))
    p.voicing = integer(j["voice"], 1);
  if (j.contains("level"))
    p.level = num(j["level"], 0.0);
  if (j.contains("tchar"))
    p.transChar = integer(j["tchar"], 0);
  if (j.contains("modes") && j["modes"].is_array())
  {
    const auto& arr = j["modes"];
    for (int i = 0; i < kVoLumPitchModeCount && i < static_cast<int>(arr.size()); ++i)
    {
      const auto& m = arr[i];
      if (!m.is_object())
        continue;
      if (m.contains("mix"))
        p.modes[i].mix = num(m["mix"], p.modes[i].mix);
      if (m.contains("dry"))
        p.modes[i].dry = num(m["dry"], p.modes[i].dry);
      if (m.contains("level"))
        p.modes[i].level = num(m["level"], p.modes[i].level);
      if (m.contains("voice"))
        p.modes[i].voicing = integer(m["voice"], p.modes[i].voicing);
    }
  }
  p.present = true;
  return p;
}

inline nlohmann::json TremoloTailToJson(const TremoloTail& t)
{
  nlohmann::json modes = nlohmann::json::array();
  for (int i = 0; i < kVoLumTremoloModeCount; ++i)
    modes.push_back({{"rate", t.modes[i].rate},
                     {"depth", t.modes[i].depth},
                     {"shape", t.modes[i].shape},
                     {"mix", t.modes[i].mix},
                     {"xover", t.modes[i].crossover}});
  return nlohmann::json{{"active", t.active}, {"mode", t.mode}, {"rate", t.rate},       {"depth", t.depth},
                        {"shape", t.shape},   {"mix", t.mix},   {"xover", t.crossover}, {"sync", t.sync},
                        {"div", t.division},  {"modes", modes}};
}

inline TremoloTail TremoloTailFromJson(const nlohmann::json& j)
{
  TremoloTail t;
  if (!j.is_object())
    return t;
  auto num = [](const nlohmann::json& v, double d) { return v.is_number() ? v.get<double>() : d; };
  auto integer = [](const nlohmann::json& v, int d) { return v.is_number_integer() ? v.get<int>() : d; };
  auto boolean = [](const nlohmann::json& v, bool d) { return v.is_boolean() ? v.get<bool>() : d; };
  if (j.contains("active"))
    t.active = boolean(j["active"], false);
  if (j.contains("mode"))
    t.mode = integer(j["mode"], kVoLumTremoloModeBias);
  if (j.contains("rate"))
    t.rate = num(j["rate"], 5.0);
  if (j.contains("depth"))
    t.depth = num(j["depth"], 0.85);
  if (j.contains("shape"))
    t.shape = num(j["shape"], 0.0);
  if (j.contains("mix"))
    t.mix = num(j["mix"], 1.0);
  if (j.contains("xover"))
    t.crossover = num(j["xover"], 800.0);
  if (j.contains("sync"))
    t.sync = boolean(j["sync"], false);
  if (j.contains("div"))
    t.division = integer(j["div"], kVoLumTremoloDivisionDefault);
  if (j.contains("modes") && j["modes"].is_array())
  {
    const auto& arr = j["modes"];
    for (int i = 0; i < kVoLumTremoloModeCount && i < static_cast<int>(arr.size()); ++i)
    {
      const auto& m = arr[i];
      if (!m.is_object())
        continue;
      if (m.contains("rate"))
        t.modes[i].rate = num(m["rate"], t.modes[i].rate);
      if (m.contains("depth"))
        t.modes[i].depth = num(m["depth"], t.modes[i].depth);
      if (m.contains("shape"))
        t.modes[i].shape = num(m["shape"], t.modes[i].shape);
      if (m.contains("mix"))
        t.modes[i].mix = num(m["mix"], t.modes[i].mix);
      if (m.contains("xover"))
        t.modes[i].crossover = num(m["xover"], t.modes[i].crossover);
    }
  }
  t.present = true;
  return t;
}

inline nlohmann::json ChorusTailToJson(const ChorusTail& c)
{
  nlohmann::json modes = nlohmann::json::array();
  for (int i = 0; i < kVoLumChorusModeCount; ++i)
    modes.push_back({{"rate", c.modes[i].rate},
                     {"depth", c.modes[i].depth},
                     {"tone", c.modes[i].tone},
                     {"width", c.modes[i].width},
                     {"mix", c.modes[i].mix}});
  return nlohmann::json{{"active", c.active}, {"mode", c.mode},   {"rate", c.rate}, {"depth", c.depth},
                        {"tone", c.tone},     {"width", c.width}, {"mix", c.mix},   {"modes", modes}};
}

inline ChorusTail ChorusTailFromJson(const nlohmann::json& j)
{
  ChorusTail c;
  if (!j.is_object())
    return c;
  auto num = [](const nlohmann::json& v, double d) { return v.is_number() ? v.get<double>() : d; };
  auto integer = [](const nlohmann::json& v, int d) { return v.is_number_integer() ? v.get<int>() : d; };
  auto boolean = [](const nlohmann::json& v, bool d) { return v.is_boolean() ? v.get<bool>() : d; };
  if (j.contains("active"))
    c.active = boolean(j["active"], false);
  if (j.contains("mode"))
    c.mode = integer(j["mode"], kVoLumChorusModeDefault);
  if (j.contains("rate"))
    c.rate = num(j["rate"], c.rate);
  if (j.contains("depth"))
    c.depth = num(j["depth"], c.depth);
  if (j.contains("tone"))
    c.tone = num(j["tone"], c.tone);
  if (j.contains("width"))
    c.width = num(j["width"], c.width);
  if (j.contains("mix"))
    c.mix = num(j["mix"], c.mix);
  if (j.contains("modes") && j["modes"].is_array())
  {
    const auto& arr = j["modes"];
    for (int i = 0; i < kVoLumChorusModeCount && i < static_cast<int>(arr.size()); ++i)
    {
      const auto& m = arr[i];
      if (!m.is_object())
        continue;
      if (m.contains("rate"))
        c.modes[i].rate = num(m["rate"], c.modes[i].rate);
      if (m.contains("depth"))
        c.modes[i].depth = num(m["depth"], c.modes[i].depth);
      if (m.contains("tone"))
        c.modes[i].tone = num(m["tone"], c.modes[i].tone);
      if (m.contains("width"))
        c.modes[i].width = num(m["width"], c.modes[i].width);
      if (m.contains("mix"))
        c.modes[i].mix = num(m["mix"], c.modes[i].mix);
    }
  }
  c.present = true;
  return c;
}

inline nlohmann::json DelayTailToJson(const DelayTail& d)
{
  return nlohmann::json{{"sync", d.sync}, {"div", d.division}};
}

inline DelayTail DelayTailFromJson(const nlohmann::json& j)
{
  DelayTail d;
  if (!j.is_object())
    return d;
  if (j.contains("sync") && j["sync"].is_boolean())
    d.sync = j["sync"].get<bool>();
  if (j.contains("div") && j["div"].is_number_integer())
    d.division = j["div"].get<int>();
  d.present = true;
  return d;
}

inline nlohmann::json IdTailToJson(const ChunkIdTail& t)
{
  nlohmann::json j;
  j["v"] = kVoLumIdTailSchema;
  j["customMainId"] = t.customMainId;
  j["customSupportId"] = t.customSupportId;
  j["activePresetId"] = t.activePresetId;
  nlohmann::json perAmp = nlohmann::json::array();
  for (int i = 0; i < kAmpCount; ++i)
  {
    nlohmann::json entry = {{"ir", t.perAmpIrId[i]},
                            {"supIr", t.perAmpSupportIrId[i]},
                            {"sup", t.perAmpSupportId[i]},
                            {"supCab", t.perAmpSupportSlot[i]},
                            {"supCh", t.perAmpSupportChannel[i]}};
    if (t.perAmpPitch[i].present)
      entry["pitch"] = PitchTailToJson(t.perAmpPitch[i]);
    if (t.perAmpTremolo[i].present)
      entry["trem"] = TremoloTailToJson(t.perAmpTremolo[i]);
    if (t.perAmpDelay[i].present)
      entry["dly"] = DelayTailToJson(t.perAmpDelay[i]);
    if (t.perAmpChorus[i].present)
      entry["cho"] = ChorusTailToJson(t.perAmpChorus[i]);
    perAmp.push_back(entry);
  }
  j["perAmp"] = perAmp;
  if (t.lockedPrePitch.present)
    j["lockedPrePitch"] = PitchTailToJson(t.lockedPrePitch);
  if (t.lockedPostTremolo.present)
    j["lockedPostTremolo"] = TremoloTailToJson(t.lockedPostTremolo);
  if (t.lockedPostDelay.present)
    j["lockedPostDelay"] = DelayTailToJson(t.lockedPostDelay);
  if (t.lockedPostChorus.present)
    j["lockedPostChorus"] = ChorusTailToJson(t.lockedPostChorus);
  return j;
}

// Lenient: missing / wrong-typed fields stay empty. Never throws on a well-formed
// JSON object (the caller guards the parse itself).
inline ChunkIdTail IdTailFromJson(const nlohmann::json& j)
{
  ChunkIdTail t;
  if (!j.is_object())
    return t;
  auto str = [](const nlohmann::json& v) { return v.is_string() ? v.get<std::string>() : std::string(); };
  if (j.contains("customMainId"))
    t.customMainId = str(j["customMainId"]);
  if (j.contains("customSupportId"))
    t.customSupportId = str(j["customSupportId"]);
  if (j.contains("activePresetId"))
    t.activePresetId = str(j["activePresetId"]);
  if (j.contains("perAmp") && j["perAmp"].is_array())
  {
    const auto& arr = j["perAmp"];
    for (int i = 0; i < kAmpCount && i < static_cast<int>(arr.size()); ++i)
    {
      if (!arr[i].is_object())
        continue;
      if (arr[i].contains("ir"))
        t.perAmpIrId[i] = str(arr[i]["ir"]);
      if (arr[i].contains("supIr"))
        t.perAmpSupportIrId[i] = str(arr[i]["supIr"]);
      if (arr[i].contains("sup"))
        t.perAmpSupportId[i] = str(arr[i]["sup"]);
      if (arr[i].contains("supCab") && arr[i]["supCab"].is_number_integer())
        t.perAmpSupportSlot[i] = arr[i]["supCab"].get<int>();
      if (arr[i].contains("supCh") && arr[i]["supCh"].is_number_integer())
        t.perAmpSupportChannel[i] = arr[i]["supCh"].get<int>();
      if (arr[i].contains("pitch"))
        t.perAmpPitch[i] = PitchTailFromJson(arr[i]["pitch"]);
      if (arr[i].contains("trem"))
        t.perAmpTremolo[i] = TremoloTailFromJson(arr[i]["trem"]);
      if (arr[i].contains("dly"))
        t.perAmpDelay[i] = DelayTailFromJson(arr[i]["dly"]);
      if (arr[i].contains("cho"))
        t.perAmpChorus[i] = ChorusTailFromJson(arr[i]["cho"]);
    }
  }
  if (j.contains("lockedPrePitch"))
    t.lockedPrePitch = PitchTailFromJson(j["lockedPrePitch"]);
  if (j.contains("lockedPostTremolo"))
    t.lockedPostTremolo = TremoloTailFromJson(j["lockedPostTremolo"]);
  if (j.contains("lockedPostDelay"))
    t.lockedPostDelay = DelayTailFromJson(j["lockedPostDelay"]);
  if (j.contains("lockedPostChorus"))
    t.lockedPostChorus = ChorusTailFromJson(j["lockedPostChorus"]);
  return t;
}

// The deferred-restore references the editor consumes when it opens: which custom
// MAIN amp to re-focus and which preset to re-label. They travel together because
// a preset bank belongs to one amp.
struct RestoreSelection
{
  std::string customMainId;
  std::string activePresetId;
};

// Decide which selection to restore into the UI when the editor opens.
//
// Standalone restores its last selection from volum-settings.json (the machine-
// global pick). A plugin instance ALSO loads that same settings file in its
// constructor, but a plugin's authoritative selection is the DAW project chunk,
// not the machine-global one. So when state came from a chunk, the chunk value
// wins - INCLUDING when it is empty: a project saved on a factory amp must land
// on that factory amp, never on whatever custom amp the settings file happens to
// remember. When there is no chunk (pure standalone launch) the settings value
// is used. This is the exact precedence the 1.2.0 "VST3 reopen drops the custom
// amp" bug got wrong (the chunk selection was never propagated to editor-open).
inline RestoreSelection ResolveRestoreSelection(bool loadedFromChunk, const RestoreSelection& chunk,
                                                const RestoreSelection& settings)
{
  return loadedFromChunk ? chunk : settings;
}

// Second stage: drop references the content store cannot resolve on THIS machine.
// The caller owns the store and reports what it found. A custom amp that no longer
// exists also invalidates the preset id, because the bank belonged to that amp -
// keeping it would restore a preset label with no owner.
inline RestoreSelection ValidateRestoreSelection(const RestoreSelection& in, bool customMainIdKnown, bool presetIdKnown)
{
  RestoreSelection out = in;
  if (!out.customMainId.empty() && !customMainIdKnown)
  {
    out.customMainId.clear();
    out.activePresetId.clear();
    return out;
  }
  if (!presetIdKnown)
    out.activePresetId.clear();
  return out;
}

// Append the id tail. Uses only scalar Put<T> so it works against both iPlug's
// IByteChunk and the unit-test MemoryChunk mock.
template <typename Chunk>
void PutChunkIdTail(Chunk& chunk, const ChunkIdTail& t)
{
  const std::string s = IdTailToJson(t).dump();
  int sentinel = kVoLumIdTailSentinel;
  int len = static_cast<int>(s.size());
  chunk.Put(&sentinel);
  chunk.Put(&len);
  for (char c : s)
    chunk.Put(&c);
}

// Probe for and read an id tail starting at `pos` (chunkSize = total bytes).
// Returns true and fills `out` (+ advances *posOut) only when a valid sentinel +
// in-bounds JSON object is found; otherwise returns false and leaves `out`
// untouched so callers fall back to empty refs (older chunk / corruption).
template <typename Chunk>
bool TryGetChunkIdTail(const Chunk& chunk, int pos, int chunkSize, ChunkIdTail& out, int* posOut = nullptr)
{
  if (pos < 0 || pos + static_cast<int>(sizeof(int)) * 2 > chunkSize)
    return false;
  int sentinel = 0;
  int p = chunk.Get(&sentinel, pos);
  if (sentinel != kVoLumIdTailSentinel)
    return false;
  int len = 0;
  p = chunk.Get(&len, p);
  if (len < 0 || p + len > chunkSize)
    return false;
  std::string s;
  s.resize(static_cast<size_t>(len));
  for (int i = 0; i < len; ++i)
  {
    char c = 0;
    p = chunk.Get(&c, p);
    s[static_cast<size_t>(i)] = c;
  }
  try
  {
    const auto j = nlohmann::json::parse(s);
    if (!j.is_object())
      return false;
    out = IdTailFromJson(j);
  }
  catch (...)
  {
    return false;
  }
  if (posOut)
    *posOut = p;
  return true;
}

} // namespace volum
