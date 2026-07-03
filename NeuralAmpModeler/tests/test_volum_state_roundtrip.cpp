#include "third_party/doctest.h"

#include "../VoLumChunkCodec.h"
#include "../VoLumChunkIdTail.h"
#include "../VoLumChunkLayout.h"
#include "../VoLumParams.h" // real kNumParams (single source of truth)

#include <cstring>
#include <string>
#include <vector>

// ---------------------------------------------------------------------------
// Real DAW-chunk SerializeState -> UnserializeState round-trip.
//
// Why this test exists (1.2.0 critical bug it guards against):
//   SerializeState writes ALL `kNumParams` param doubles via iPlug's
//   SerializeParams(), then appends the VoLum per-amp block (selection, per-amp
//   scenes, PRE/POST lock flags+snapshots) and the sentinel-guarded id tail
//   (custom amp/support/preset refs + per-amp pitch/tremolo/delay-sync). The
//   current-version chunk reader MUST consume EXACTLY `kNumParams` param doubles
//   before it reaches the per-amp block; the shipped 1.2.0 reader used a frozen
//   71-name list while the writer emitted 93, so it read the selection/scene 22
//   doubles too early -> every VST3/AU reload restored garbage -> "state resets
//   to default on load". Standalone masked it (restores from volum-settings.json).
//
// How this test avoids the circularity that let the bug ship:
//   - It measures against the REAL `kNumParams` from the enum (VoLumParams.h),
//     never a hardcoded copy. Add a param and this test tracks it automatically.
//   - It parses the whole tail with the REAL shared codec templates
//     (VoLumChunkCodec.h) and the REAL layout detectors (VoLumChunkLayout.h) and
//     the REAL id-tail reader (VoLumChunkIdTail.h) that production uses. Only the
//     trivial param-double loop and the two path strings are reproduced inline
//     (the source pin in test_volum_ui_regressions.cpp locks that the production
//     reader builds its param list from a `kNumParams` live-name loop).
//   - It models iPlug's VST3 wrapper trailing 4-byte bypass int: SetState reads
//     the whole chunk, calls UnserializeState -> pos, seek(pos), reads 4 bytes;
//     if that read fails the host discards ALL state. So a correct round-trip
//     requires the reader to land at exactly `size - 4`. This test asserts it.
// ---------------------------------------------------------------------------

namespace
{
// Duck-typed stand-in for iplug::IByteChunk. The shared codec templates only
// need Put/Get; we add PutStr/GetStr (self-consistent length-prefixed form) for
// the two leading paths and Size() for the layout detectors.
struct MemoryChunk
{
  std::vector<unsigned char> bytes;

  template <typename T>
  void Put(const T* value)
  {
    const auto* first = reinterpret_cast<const unsigned char*>(value);
    bytes.insert(bytes.end(), first, first + sizeof(T));
  }

  void PutStr(const char* str)
  {
    const int len = static_cast<int>(std::strlen(str));
    Put(&len);
    bytes.insert(bytes.end(), str, str + len);
  }

  template <typename T>
  int Get(T* value, int pos) const
  {
    REQUIRE(pos >= 0);
    REQUIRE(static_cast<size_t>(pos) + sizeof(T) <= bytes.size());
    std::memcpy(value, bytes.data() + pos, sizeof(T));
    return pos + static_cast<int>(sizeof(T));
  }

  int GetStr(std::string& out, int pos) const
  {
    int len = 0;
    pos = Get(&len, pos);
    REQUIRE(len >= 0);
    REQUIRE(static_cast<size_t>(pos) + static_cast<size_t>(len) <= bytes.size());
    out.assign(reinterpret_cast<const char*>(bytes.data() + pos), static_cast<size_t>(len));
    return pos + len;
  }

  int Size() const { return static_cast<int>(bytes.size()); }
};

// The parsed result of one round-trip, so tests can assert survival of state.
struct ParsedState
{
  volum::VoLumChunkSelection selection;
  volum::VoLumAmpSettings amps[volum::kAmpCount]{};
  bool preLocked = false;
  bool postLocked = false;
  volum::VoLumAmpSettings preSnapshot{};
  volum::VoLumAmpSettings postSnapshot{};
  volum::ChunkIdTail idTail;
  int pos = 0; // where the reader landed (should be Size() - 4)
};

constexpr int kBypassBytes = static_cast<int>(sizeof(int)); // iPlug VST3 trailing bypass int

// Build a chunk byte-for-byte like NeuralAmpModeler::SerializeState for the
// current (>= 1.2.0) format, then append the VST3 wrapper's 4-byte bypass int.
// `paramCount` is a knob purely so the negative test can simulate a
// writer/reader param-count skew; production always uses kNumParams.
MemoryChunk BuildCurrentChunk(const volum::VoLumChunkSelection& selection,
                              const volum::VoLumAmpSettings (&amps)[volum::kAmpCount], bool preLocked, bool postLocked,
                              const volum::VoLumAmpSettings& preSnapshot, const volum::VoLumAmpSettings& postSnapshot,
                              const volum::ChunkIdTail& idTail, int paramCount = kNumParams)
{
  MemoryChunk chunk;
  chunk.PutStr("###NeuralAmpModeler###");
  chunk.PutStr("1.2.0");
  chunk.PutStr("C:/rigs/main.nam"); // NAM path
  chunk.PutStr("C:/rigs/cab.wav"); // IR path
  for (int i = 0; i < paramCount; ++i)
  {
    // Values chosen so a misread (reading a param double as a selection int)
    // can never coincide with the real selection triple.
    double v = 1000.0 + i;
    chunk.Put(&v);
  }
  volum::PutCurrentVoLumChunkState(chunk, selection, amps, volum::kAmpCount);
  volum::PutPrePostLockFlags(chunk, preLocked, postLocked);
  volum::PutPrePostLockSnapshots(chunk, preLocked, postLocked, preSnapshot, postSnapshot);
  volum::PutChunkIdTail(chunk, idTail);
  int bypass = 0; // VST3 wrapper appends this after SerializeState
  chunk.Put(&bypass);
  return chunk;
}

// Mirror of NeuralAmpModeler::UnserializeState -> _UnserializeStateWithKnownVersion
// for the current format. Uses the SAME shared codec + layout detectors +
// id-tail reader as production; the param loop consumes `readParamCount`
// doubles (production uses kNumParams via the live-name loop).
ParsedState ParseCurrentChunk(const MemoryChunk& chunk, int readParamCount = kNumParams)
{
  ParsedState out;
  int pos = 0;

  std::string header;
  pos = chunk.GetStr(header, pos);
  REQUIRE(header == "###NeuralAmpModeler###");
  std::string version;
  pos = chunk.GetStr(version, pos);

  // _UnserializePathsAndExpectedKeys: NAM path, IR path, then the param doubles.
  std::string namPath;
  std::string irPath;
  pos = chunk.GetStr(namPath, pos);
  pos = chunk.GetStr(irPath, pos);
  for (int i = 0; i < readParamCount; ++i)
  {
    double scratch = 0.0;
    pos = chunk.Get(&scratch, pos);
  }

  // Per-amp block (identical sequence + detectors to Unserialization.cpp).
  pos = volum::GetVoLumChunkSelection(chunk, pos, out.selection);
  const int remaining = chunk.Size() - pos;
  const bool hasPre = volum::ChunkHasExtendedPerAmpSettings(remaining, volum::kAmpCount);
  const bool hasDual = volum::ChunkHasDualAmpPerAmpSettings(remaining, volum::kAmpCount);
  const bool hasSupPol = volum::ChunkHasSupportPolarityPerAmpSettings(remaining, volum::kAmpCount);
  const bool hasPost = volum::ChunkHasPostPerAmpSettings(remaining, volum::kAmpCount);
  const bool hasPostSnap = volum::ChunkHasPostSnapshotPerAmpSettings(remaining, volum::kAmpCount);
  const bool hasLockFlags = volum::ChunkHasPrePostLockFlags(remaining, volum::kAmpCount);

  for (int i = 0; i < volum::kAmpCount; ++i)
  {
    auto& s = out.amps[i];
    pos = volum::GetLegacyPerAmpSettings(chunk, pos, s);
    if (hasPre)
      pos = volum::GetExtendedPerAmpSettings(chunk, pos, s, /*has081PreCompControls=*/true);
    if (hasDual)
      pos = volum::GetDualAmpPerAmpSettings(chunk, pos, s, hasSupPol);
    if (hasPost)
      pos = volum::GetPostPerAmpSettings(chunk, pos, s, hasPostSnap);
  }

  if (hasLockFlags)
    pos = volum::GetPrePostLockFlags(chunk, pos, out.preLocked, out.postLocked);

  if (volum::ChunkHasPrePostLockSnapshots(remaining, volum::kAmpCount, out.preLocked, out.postLocked))
    pos = volum::GetPrePostLockSnapshots(chunk, pos, out.preLocked, out.postLocked, out.preSnapshot, out.postSnapshot);

  int idTailPos = pos;
  const bool haveIdTail = volum::TryGetChunkIdTail(chunk, pos, chunk.Size(), out.idTail, &idTailPos);
  if (haveIdTail)
    pos = idTailPos;

  out.pos = pos;
  return out;
}

// A non-default scene on the selected amp plus a non-default id tail, so we can
// assert every category the 1.2.0 bug corrupted actually survives the trip.
constexpr int kSelectedAmp = 3;

void SeedNonDefaultState(volum::VoLumChunkSelection& selection, volum::VoLumAmpSettings (&amps)[volum::kAmpCount],
                         volum::ChunkIdTail& idTail, volum::VoLumAmpSettings& preSnapshot,
                         volum::VoLumAmpSettings& postSnapshot)
{
  selection = {kSelectedAmp, 2, 1}; // default is {0, 3, 0}

  auto& s = amps[kSelectedAmp];
  s.speakerIdx = 2;
  s.channelIdx = 1;
  s.inputLevel = 4.25;
  s.toneBass = 7.5;
  s.noiseGateActive = false;
  s.preCompActive = true;
  s.preCompAmount = 6.0;
  s.preNam1Active = true;
  s.preNam1Capture = 5;
  s.dualAmpActive = true;
  s.dualAmpRoute = 1;
  s.mainAmpPan = -0.5;
  s.supportAmpIdx = 7;
  s.supportSpeakerIdx = 1;
  s.supportPolarityInvert = true;
  s.postValid = true;
  s.postDelayActive = true;
  s.postDelayMode = volum::kVoLumDelayModeReverse;
  s.postDelayMix = 0.42;
  s.postReverbActive = true;
  s.postReverbMode = volum::kVoLumReverbModeOktaverb;
  s.postReverbSubMode = volum::kVoLumOktaverbSubModeBloom;

  idTail.customMainId = "amp_main_custom";
  idTail.customSupportId = "amp_support_custom";
  idTail.activePresetId = "preset_lead_01";
  idTail.perAmpIrId[kSelectedAmp] = "ir_custom_3";
  idTail.perAmpSupportIrId[kSelectedAmp] = "ir_support_3";
  idTail.perAmpSupportId[kSelectedAmp] = "amp_support_slotref";
  idTail.perAmpSupportSlot[kSelectedAmp] = 1;
  idTail.perAmpSupportChannel[kSelectedAmp] = 2;

  idTail.perAmpPitch[kSelectedAmp].present = true;
  idTail.perAmpPitch[kSelectedAmp].active = true;
  idTail.perAmpPitch[kSelectedAmp].mode = 1; // Octaver
  idTail.perAmpPitch[kSelectedAmp].semitones = -5.0;
  idTail.perAmpPitch[kSelectedAmp].mix = 0.8;

  idTail.perAmpTremolo[kSelectedAmp].present = true;
  idTail.perAmpTremolo[kSelectedAmp].active = true;
  idTail.perAmpTremolo[kSelectedAmp].mode = volum::kVoLumTremoloModeHarmonic;
  idTail.perAmpTremolo[kSelectedAmp].rate = 6.5;
  idTail.perAmpTremolo[kSelectedAmp].division = 6;

  idTail.perAmpDelay[kSelectedAmp].present = true;
  idTail.perAmpDelay[kSelectedAmp].sync = true;
  idTail.perAmpDelay[kSelectedAmp].division = 5;

  // Locked PRE + POST live snapshots (present iff their flag is set).
  preSnapshot.preCompActive = true;
  preSnapshot.preCompAmount = 3.5;
  preSnapshot.preNam1Active = true;
  preSnapshot.preNam1Capture = 4;
  postSnapshot.postValid = true;
  postSnapshot.postReverbActive = true;
  postSnapshot.postReverbMix = 0.31;
}
} // namespace

TEST_CASE("Real DAW chunk round-trips selection + scene + custom refs + effects at the true kNumParams")
{
  volum::VoLumChunkSelection selection;
  volum::VoLumAmpSettings amps[volum::kAmpCount]{};
  volum::ChunkIdTail idTail;
  volum::VoLumAmpSettings preSnapshot{};
  volum::VoLumAmpSettings postSnapshot{};
  SeedNonDefaultState(selection, amps, idTail, preSnapshot, postSnapshot);

  const MemoryChunk chunk =
    BuildCurrentChunk(selection, amps, /*preLocked=*/true, /*postLocked=*/true, preSnapshot, postSnapshot, idTail);

  const ParsedState got = ParseCurrentChunk(chunk);

  // The exact byte-alignment contract iPlug's VST3 wrapper depends on: the
  // reader must consume the whole SerializeState payload and stop right before
  // the trailing 4-byte bypass int. If this fails the host discards ALL state.
  CHECK(got.pos == chunk.Size() - kBypassBytes);

  // Selection survived (the first thing the 1.2.0 bug corrupted).
  CHECK(got.selection.ampIdx == kSelectedAmp);
  CHECK(got.selection.speakerIdx == 2);
  CHECK(got.selection.channelIdx == 1);

  // Per-amp scene survived.
  const auto& s = got.amps[kSelectedAmp];
  CHECK(s.speakerIdx == 2);
  CHECK(s.channelIdx == 1);
  CHECK(s.inputLevel == doctest::Approx(4.25));
  CHECK(s.toneBass == doctest::Approx(7.5));
  CHECK_FALSE(s.noiseGateActive);
  CHECK(s.preCompActive);
  CHECK(s.preCompAmount == doctest::Approx(6.0));
  CHECK(s.preNam1Active);
  CHECK(s.preNam1Capture == 5);
  CHECK(s.dualAmpActive);
  CHECK(s.dualAmpRoute == 1);
  CHECK(s.mainAmpPan == doctest::Approx(-0.5));
  CHECK(s.supportAmpIdx == 7);
  CHECK(s.supportSpeakerIdx == 1);
  CHECK(s.supportPolarityInvert);
  CHECK(s.postValid);
  CHECK(s.postDelayActive);
  CHECK(s.postDelayMode == volum::kVoLumDelayModeReverse);
  CHECK(s.postDelayMix == doctest::Approx(0.42));
  CHECK(s.postReverbActive);
  CHECK(s.postReverbMode == volum::kVoLumReverbModeOktaverb);
  CHECK(s.postReverbSubMode == volum::kVoLumOktaverbSubModeBloom);

  // Custom content refs (id tail) survived.
  CHECK(got.idTail.customMainId == "amp_main_custom");
  CHECK(got.idTail.customSupportId == "amp_support_custom");
  CHECK(got.idTail.activePresetId == "preset_lead_01");
  CHECK(got.idTail.perAmpIrId[kSelectedAmp] == "ir_custom_3");
  CHECK(got.idTail.perAmpSupportIrId[kSelectedAmp] == "ir_support_3");
  CHECK(got.idTail.perAmpSupportId[kSelectedAmp] == "amp_support_slotref");
  CHECK(got.idTail.perAmpSupportSlot[kSelectedAmp] == 1);
  CHECK(got.idTail.perAmpSupportChannel[kSelectedAmp] == 2);

  // Per-amp PRE pitch / POST tremolo / POST delay-sync (1.2.0 additions) survived.
  CHECK(got.idTail.perAmpPitch[kSelectedAmp].present);
  CHECK(got.idTail.perAmpPitch[kSelectedAmp].active);
  CHECK(got.idTail.perAmpPitch[kSelectedAmp].mode == 1);
  CHECK(got.idTail.perAmpPitch[kSelectedAmp].semitones == doctest::Approx(-5.0));
  CHECK(got.idTail.perAmpPitch[kSelectedAmp].mix == doctest::Approx(0.8));
  CHECK(got.idTail.perAmpTremolo[kSelectedAmp].present);
  CHECK(got.idTail.perAmpTremolo[kSelectedAmp].active);
  CHECK(got.idTail.perAmpTremolo[kSelectedAmp].mode == volum::kVoLumTremoloModeHarmonic);
  CHECK(got.idTail.perAmpTremolo[kSelectedAmp].rate == doctest::Approx(6.5));
  CHECK(got.idTail.perAmpTremolo[kSelectedAmp].division == 6);
  CHECK(got.idTail.perAmpDelay[kSelectedAmp].present);
  CHECK(got.idTail.perAmpDelay[kSelectedAmp].sync);
  CHECK(got.idTail.perAmpDelay[kSelectedAmp].division == 5);

  // Lock flags + live snapshots survived.
  CHECK(got.preLocked);
  CHECK(got.postLocked);
  CHECK(got.preSnapshot.preCompActive);
  CHECK(got.preSnapshot.preCompAmount == doctest::Approx(3.5));
  CHECK(got.preSnapshot.preNam1Active);
  CHECK(got.preSnapshot.preNam1Capture == 4);
  CHECK(got.postSnapshot.postValid);
  CHECK(got.postSnapshot.postReverbActive);
  CHECK(got.postSnapshot.postReverbMix == doctest::Approx(0.31));
}

TEST_CASE("Reader that consumes fewer than kNumParams param doubles derails selection + byte alignment")
{
  // Reproduce the EXACT 1.2.0 mechanism: the writer emits kNumParams param
  // doubles but the reader stops short (the shipped bug read 71 of 93). This is
  // the anti-theater proof that the round-trip assertions above have teeth: with
  // a short read the selection reads leftover param bytes and `pos` no longer
  // lands at size - 4, so the host would discard all state.
  volum::VoLumChunkSelection selection;
  volum::VoLumAmpSettings amps[volum::kAmpCount]{};
  volum::ChunkIdTail idTail;
  volum::VoLumAmpSettings preSnapshot{};
  volum::VoLumAmpSettings postSnapshot{};
  SeedNonDefaultState(selection, amps, idTail, preSnapshot, postSnapshot);

  const MemoryChunk chunk =
    BuildCurrentChunk(selection, amps, /*preLocked=*/true, /*postLocked=*/true, preSnapshot, postSnapshot, idTail);

  // Simulate the frozen-list short read: stop 22 params early (93 -> 71 shape).
  REQUIRE(kNumParams > 22);
  const ParsedState got = ParseCurrentChunk(chunk, /*readParamCount=*/kNumParams - 22);

  const bool selectionSurvived =
    (got.selection.ampIdx == kSelectedAmp && got.selection.speakerIdx == 2 && got.selection.channelIdx == 1);
  CHECK_FALSE(selectionSurvived);
  CHECK(got.pos != chunk.Size() - kBypassBytes);
}

TEST_CASE("A full-count read is required for exact byte alignment (kNumParams is the contract)")
{
  // Belt-and-suspenders: only reading exactly kNumParams param doubles lands the
  // reader at size - 4. Reading one too few or one too many misaligns.
  volum::VoLumChunkSelection selection;
  volum::VoLumAmpSettings amps[volum::kAmpCount]{};
  volum::ChunkIdTail idTail;
  volum::VoLumAmpSettings preSnapshot{};
  volum::VoLumAmpSettings postSnapshot{};
  SeedNonDefaultState(selection, amps, idTail, preSnapshot, postSnapshot);

  const MemoryChunk chunk =
    BuildCurrentChunk(selection, amps, /*preLocked=*/false, /*postLocked=*/false, preSnapshot, postSnapshot, idTail);

  CHECK(ParseCurrentChunk(chunk, kNumParams).pos == chunk.Size() - kBypassBytes);
  CHECK(ParseCurrentChunk(chunk, kNumParams - 1).pos != chunk.Size() - kBypassBytes);
  CHECK(ParseCurrentChunk(chunk, kNumParams + 1).pos != chunk.Size() - kBypassBytes);
}
