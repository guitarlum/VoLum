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
inline constexpr int kVoLumIdTailSchema = 3;

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

  ChunkIdTail()
  {
    for (int i = 0; i < kAmpCount; ++i)
    {
      perAmpSupportSlot[i] = -2;
      perAmpSupportChannel[i] = 0;
    }
  }
};

inline nlohmann::json IdTailToJson(const ChunkIdTail& t)
{
  nlohmann::json j;
  j["v"] = kVoLumIdTailSchema;
  j["customMainId"] = t.customMainId;
  j["customSupportId"] = t.customSupportId;
  j["activePresetId"] = t.activePresetId;
  nlohmann::json perAmp = nlohmann::json::array();
  for (int i = 0; i < kAmpCount; ++i)
    perAmp.push_back({{"ir", t.perAmpIrId[i]},
                      {"supIr", t.perAmpSupportIrId[i]},
                      {"sup", t.perAmpSupportId[i]},
                      {"supCab", t.perAmpSupportSlot[i]},
                      {"supCh", t.perAmpSupportChannel[i]}});
  j["perAmp"] = perAmp;
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
    }
  }
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
