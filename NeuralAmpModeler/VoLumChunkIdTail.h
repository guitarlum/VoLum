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
inline constexpr int kVoLumIdTailSchema = 4;

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
  int transChar = 0; // 0=Drop, 1=Fast, 2=Instant (transpose engine character)
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
  return nlohmann::json{{"active", p.active}, {"mode", p.mode},      {"semi", p.semitones}, {"mix", p.mix},
                        {"octDn", p.octDown}, {"octUp", p.octUp},    {"dry", p.dry},        {"voice", p.voicing},
                        {"level", p.level},   {"tchar", p.transChar}};
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
  p.present = true;
  return p;
}

inline nlohmann::json TremoloTailToJson(const TremoloTail& t)
{
  return nlohmann::json{{"active", t.active},   {"mode", t.mode},   {"rate", t.rate},
                        {"depth", t.depth},     {"shape", t.shape}, {"mix", t.mix},
                        {"xover", t.crossover}, {"sync", t.sync},   {"div", t.division}};
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
  t.present = true;
  return t;
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
    perAmp.push_back(entry);
  }
  j["perAmp"] = perAmp;
  if (t.lockedPrePitch.present)
    j["lockedPrePitch"] = PitchTailToJson(t.lockedPrePitch);
  if (t.lockedPostTremolo.present)
    j["lockedPostTremolo"] = TremoloTailToJson(t.lockedPostTremolo);
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
    }
  }
  if (j.contains("lockedPrePitch"))
    t.lockedPrePitch = PitchTailFromJson(j["lockedPrePitch"]);
  if (j.contains("lockedPostTremolo"))
    t.lockedPostTremolo = TremoloTailFromJson(j["lockedPostTremolo"]);
  return t;
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
