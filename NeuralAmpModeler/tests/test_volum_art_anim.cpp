#include "third_party/doctest.h"

#include "../VoLumAmpeteCatalog.h"
#include "../VoLumPlayLight.h"
#include "../art/VoLumArtMotion.h"
#include "../art/VoLumArtRegistry.h"

#include <cctype>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <set>
#include <sstream>
#include <string>

namespace
{
std::filesystem::path PluginRoot()
{
  return std::filesystem::path(__FILE__).parent_path().parent_path();
}

std::string ReadText(const std::filesystem::path& path)
{
  std::ifstream in(path, std::ios::binary);
  REQUIRE(in.good());
  std::ostringstream ss;
  ss << in.rdbuf();
  return ss.str();
}

// "VoLumArtHkTriamp.h" -> "HkTriamp"
std::string ArtName(const char* file)
{
  std::string s(file);
  return s.substr(std::strlen("VoLumArt"), s.size() - std::strlen("VoLumArt") - 2);
}

constexpr float kTick = 1.f / 60.f;

// One PLAY tick the way VoLumPlaySurfaceControl::Tick runs it.
void Step(volum::PlayLight& light, volumart::ArtMotionState& s, float inNorm, float dt = kTick)
{
  light = volum::AdvancePlayLight(light, inNorm);
  volumart::AdvanceArtMotion(s, light.energy, light.attack, volum::PlayBloomWeight(volum::PlayGlowAmount(light)), dt);
}
} // namespace

TEST_CASE("Art anim: Art registry has one row per fractal case, matching the amp catalog")
{
  REQUIRE(volumart::kFactoryArtCount == volum::kAmpCount);
  std::set<int> amps;
  for (int c = 0; c < volumart::kFactoryArtCount; ++c)
  {
    const volumart::ArtSpec& spec = volumart::kArtSpecs[c];
    INFO("case " << c << " " << spec.amp);
    CHECK(spec.fractalCase == c);
    REQUIRE(spec.ampIdx >= 0);
    REQUIRE(spec.ampIdx < volum::kAmpCount);
    CHECK(volum::kAmpFractalCase[spec.ampIdx] == c);
    CHECK(std::string(spec.amp) == volum::kAmps[spec.ampIdx].displayName);
    CHECK(amps.insert(spec.ampIdx).second);
    CHECK(std::string(spec.file).rfind("VoLumArt", 0) == 0);
    CHECK(std::strlen(spec.motif) > 0);
  }
  CHECK_FALSE(volumart::ArtAnimates(-1));
  CHECK_FALSE(volumart::ArtAnimates(volumart::kFactoryArtCount));
}

TEST_CASE("Art anim: Phase 1 animates the two pilot arts")
{
  CHECK(volumart::ArtAnimates(4)); // H&K TriAmp Mk2
  CHECK(volumart::ArtAnimates(12)); // Soldano SLO100
  CHECK(volumart::kCustomArtAnimate);
}

TEST_CASE("Art anim: Every factory art owns one file, wired into the dispatch")
{
  const auto artDir = PluginRoot() / "art";
  const std::string dispatch = ReadText(artDir / "VoLumArtDispatch.h");
  for (const auto& spec : volumart::kArtSpecs)
  {
    INFO(spec.file);
    const std::string name = ArtName(spec.file);
    const std::string file = ReadText(artDir / spec.file);
    CHECK(file.find("inline void Draw" + name + "Hero(IGraphics& g, const IRECT& rect)") != std::string::npos);
    CHECK(file.find("inline std::unique_ptr<ArtAnimator> Make" + name + "Animator()") != std::string::npos);
    CHECK(dispatch.find("#include \"" + std::string(spec.file) + "\"") != std::string::npos);
    const std::string c = "case " + std::to_string(spec.fractalCase) + ": ";
    CHECK(dispatch.find(c + "art::Draw" + name + "Hero(g, r); break;") != std::string::npos);
    CHECK(dispatch.find(c + "return art::Make" + name + "Animator();") != std::string::npos);
  }
}

TEST_CASE("Art anim: Art files keep to the animator contract")
{
  // Each art file sees only the art vocabulary (the colour helpers and the
  // fractal umbrella include each other), builds layers only through
  // ArtLayer::Build in Prepare, and draws from (rect, motion) alone.
  namespace fs = std::filesystem;
  int files = 0;
  for (const auto& entry : fs::directory_iterator(PluginRoot() / "art"))
  {
    const std::string leaf = entry.path().filename().string();
    if (entry.path().extension() != ".h")
      continue;
    ++files;
    INFO(leaf);
    const std::string text = ReadText(entry.path());
    CHECK(text.find("VoLumFractalArt.h\"") == std::string::npos);
    CHECK(text.find("VoLumColorHelpers.h\"") == std::string::npos);
    for (auto at = text.find("rand("); at != std::string::npos; at = text.find("rand(", at + 1))
    {
      const char before = at > 0 ? text[at - 1] : ' ';
      CHECK_MESSAGE((std::isalnum(static_cast<unsigned char>(before)) || before == '_'), "rand( at " << at);
    }
    CHECK(text.find("steady_clock") == std::string::npos);
    CHECK(text.find("system_clock") == std::string::npos);
    if (leaf != "VoLumArtAnimator.h")
    {
      CHECK(text.find("StartLayer") == std::string::npos);
      CHECK(text.find("ResumeLayer") == std::string::npos);
    }
  }
  CHECK(files >= volumart::kFactoryArtCount + 5);
}

TEST_CASE("Art anim: BUILD and the static PLAY path draw factory art through the dispatch")
{
  const std::string fractal = ReadText(PluginRoot() / "VoLumFractalArt.h");
  CHECK(fractal.find("volumart::DrawFactoryHeroArt(g, rect, ampIdx % 15);") != std::string::npos);
  CHECK(fractal.find("#include \"art/VoLumArtDispatch.h\"") != std::string::npos);
  // The per-case bodies left this file.
  CHECK(fractal.find("Burning Ship \"sea\"") == std::string::npos);

  const std::string play = ReadText(PluginRoot() / "VoLumPlaySurface.h");
  const auto fn = play.find("bool DrawAnimatedStageArt(");
  REQUIRE(fn != std::string::npos);
  const auto prepare = play.find("->Prepare(g, this, paint)");
  const auto clip = play.find("g.PathClipRegion(paint);", fn);
  const auto draw = play.find("slot.anim->Draw(g, paint, m);", fn);
  REQUIRE(prepare != std::string::npos);
  REQUIRE(clip != std::string::npos);
  REQUIRE(draw != std::string::npos);
  // Layers are built with no clip set, then the art draws inside its clip.
  CHECK(fn < prepare);
  CHECK(prepare < clip);
  CHECK(clip < draw);
  CHECK(play.find("->Prepare(", prepare + 1) == std::string::npos);
  // Motion only advances from Tick (PLAY shown), never from Draw.
  CHECK(play.find("volumart::AdvanceArtMotion(") > play.find("void Tick()"));
  CHECK(play.find("volumart::AdvanceArtMotion(")
        < play.find("void Draw(IGraphics& g) override", play.find("void Tick()")));
}

TEST_CASE("Art anim: Art motion stays at rest through silence")
{
  volum::PlayLight light;
  volumart::ArtMotionState s;
  for (int i = 0; i < 600; ++i)
    Step(light, s, 0.f);
  CHECK(s.m.IsRest());
  CHECK(s.m.clock == 0.0);
  CHECK(s.m.onsets == 0u);
  CHECK(s.m.bloom == 0.f);
  CHECK(s.m.Wake() == 0.f);

  // Hum and noise under the energy floor (-62 dBFS here) are silence too.
  for (int i = 0; i < 600; ++i)
    Step(light, s, volum::MeterNormFromDb(-62.f));
  CHECK(s.m.IsRest());
  CHECK(s.m.clock == 0.0);
}

TEST_CASE("Art anim: A pick is one onset, and a held note does not repeat it")
{
  volum::PlayLight light;
  volumart::ArtMotionState s;
  for (int i = 0; i < 60; ++i)
    Step(light, s, 0.f);
  Step(light, s, 0.83f);
  CHECK(s.m.onsets == 1u);
  CHECK(s.m.onsetAge[0] == 0.f);
  CHECK(s.m.pick > 0.9f);
  CHECK_FALSE(s.m.IsRest());
  for (int i = 0; i < 180; ++i)
    Step(light, s, 0.83f);
  CHECK(s.m.onsets == 1u);
  CHECK(s.m.pick < 0.05f);
  CHECK(s.m.energy > 0.9f);
  CHECK(s.m.onsetAge[0] == doctest::Approx(180.f * kTick).epsilon(0.01));
}

TEST_CASE("Art anim: Separate picks each stamp an onset, newest first")
{
  volum::PlayLight light;
  volumart::ArtMotionState s;
  for (int pick = 0; pick < 3; ++pick)
  {
    for (int i = 0; i < 6; ++i)
      Step(light, s, 0.83f);
    for (int i = 0; i < 54; ++i)
      Step(light, s, 0.f);
  }
  CHECK(s.m.onsets == 3u);
  CHECK(s.m.onsetAge[0] < s.m.onsetAge[1]);
  CHECK(s.m.onsetAge[1] < s.m.onsetAge[2]);
  CHECK(s.m.onsetAge[1] - s.m.onsetAge[0] == doctest::Approx(1.f).epsilon(0.02));
  CHECK(s.m.onsetAge[3] >= volumart::kNoOnset);
}

TEST_CASE("Art anim: The motion clock runs only while sounding, never faster than wall time")
{
  volum::PlayLight light;
  volumart::ArtMotionState s;
  for (int i = 0; i < 120; ++i)
    Step(light, s, 0.83f);
  const double sounding = s.m.clock;
  CHECK(sounding > 2.0 * volumart::kClockRateFloor * 0.5);
  CHECK(sounding <= 2.0 + 1e-6);
  for (int i = 0; i < 600; ++i)
    Step(light, s, 0.f);
  const double settled = s.m.clock;
  CHECK(s.m.IsRest());
  for (int i = 0; i < 600; ++i)
    Step(light, s, 0.f);
  CHECK(s.m.clock == settled);
  // A stalled frame cannot fling the clock.
  Step(light, s, 0.83f, 5.f);
  CHECK(s.m.clock - settled <= volumart::kMaxMotionDt + 1e-9);
}

TEST_CASE("Art anim: Silence returns to rest within the release plus the event age")
{
  volum::PlayLight light;
  volumart::ArtMotionState s;
  for (int i = 0; i < 120; ++i)
    Step(light, s, 0.83f);
  int ticks = 0;
  while (!s.m.IsRest() && ticks < 60 * 10)
  {
    Step(light, s, 0.f);
    ++ticks;
  }
  CHECK(s.m.IsRest());
  CHECK(ticks * kTick < 3.5f);
  CHECK(s.m.bloom == 0.f);
}

TEST_CASE("Art anim: The frame PLAY draws in silence is the exact rest frame")
{
  // Silence identity: whatever the clock or pick count, a resting animator is
  // handed RestMotion, so it can only draw its static art.
  volum::PlayLight light;
  volumart::ArtMotionState s;
  for (int i = 0; i < 120; ++i)
    Step(light, s, 0.83f);
  for (int i = 0; i < 600; ++i)
    Step(light, s, 0.f);
  const volumart::ArtMotion m = volumart::LiveArtMotion(s, 12u);
  CHECK(m.IsRest());
  CHECK(m.energy == 0.f);
  CHECK(m.pick == 0.f);
  CHECK(m.bloom == 0.f);
  CHECK(m.seed == 12u);
  CHECK(m.Wake() == 0.f);

  // Silence with a stale lift (a light that has not quite reached 0 in float) is
  // still drawn as the rest frame.
  volumart::ArtMotionState stale;
  stale.m.bloom = 0.02f;
  REQUIRE(stale.m.IsRest());
  CHECK(volumart::LiveArtMotion(stale, 3u).bloom == 0.f);

  // Sounding: the live values pass through with this panel's seed.
  Step(light, s, 0.83f);
  const volumart::ArtMotion live = volumart::LiveArtMotion(s, 4u);
  CHECK_FALSE(live.IsRest());
  CHECK(live.seed == 4u);
  CHECK(live.bloom > 0.f);
}

TEST_CASE("Art anim: Debug frames: energy 0 is rest at any clock, energy 1 lights like playing")
{
  for (const char* spec : {"13:0:7.3", "5:0:0", "c3:0:99"})
  {
    INFO(spec);
    const auto d = volumart::ParseArtAnimDebug(spec);
    REQUIRE(d.on);
    const volumart::ArtMotion m = volumart::DebugArtMotion(d, 1u, 42.0);
    CHECK(m.IsRest());
    CHECK(m.bloom == 0.f);
    CHECK(volum::PlayGlowAmount(volumart::DebugPlayLight(d)) == 0.f);
  }
  const auto d = volumart::ParseArtAnimDebug("5:1:2.5:0.8");
  const volumart::ArtMotion m = volumart::DebugArtMotion(d, 4u, 0.0);
  CHECK(m.energy == 1.f);
  CHECK(m.pick == doctest::Approx(0.8f));
  CHECK(m.clock == doctest::Approx(2.5));
  CHECK(m.onsets == 1u);
  CHECK(m.onsetAge[0] < volumart::kMaxEventAge);
  CHECK(m.bloom == doctest::Approx(volum::PlayBloomWeight(volum::PlayGlowAmount(volumart::DebugPlayLight(d)))));
  CHECK(volumart::DebugPlayLight(d).energy == 1.f);
  const auto run = volumart::ParseArtAnimDebug("3:1:run");
  CHECK(volumart::DebugArtMotion(run, 3u, 12.5).clock == doctest::Approx(12.5));
}

TEST_CASE("Art anim: VOLUM_ART_ANIM_DEBUG grammar")
{
  auto p = volumart::ParseArtAnimDebug;
  CHECK_FALSE(p(nullptr).on);
  CHECK_FALSE(p("").on);

  auto a = p("5");
  CHECK(a.on);
  CHECK(a.art == 5);
  CHECK_FALSE(a.custom);
  CHECK_FALSE(a.legacy);
  CHECK(a.energy == 0.f);
  CHECK(a.clock == 0.0);

  auto off = p("13:off");
  CHECK(off.on);
  CHECK(off.legacy);
  CHECK(off.art == 13);

  auto full = p("13:1:2.5:0.8");
  CHECK(full.on);
  CHECK(full.energy == 1.f);
  CHECK(full.clock == doctest::Approx(2.5));
  CHECK(full.pick == doctest::Approx(0.8f));
  CHECK_FALSE(full.runClock);

  auto custom = p("c3:0.5:run");
  CHECK(custom.on);
  CHECK(custom.custom);
  CHECK(custom.art == 3);
  CHECK(custom.energy == doctest::Approx(0.5f));
  CHECK(custom.runClock);

  // Out of range clamps; malformed stays off.
  CHECK(p("20").art == 14);
  CHECK(p("c9").art == 5);
  CHECK(p("5:2").energy == 1.f);
  CHECK(p("5:0.5:1:7").pick == 1.f);
  for (const char* bad : {"x", "-1", "5.5", "5:abc", "5:1:soon", "5:1:-2", "5:1:2:3:4", "c", ":1", "5:1:2:x"})
  {
    INFO(std::string(bad));
    CHECK_FALSE(p(bad).on);
  }
}

TEST_CASE("Art anim: Motion noise is deterministic, bounded and smooth")
{
  for (uint32_t i = 0; i < 2000; ++i)
  {
    const float h = volumart::Hash01(i * 2654435761u);
    CHECK(h >= 0.f);
    CHECK(h < 1.f);
  }
  CHECK(volumart::Hash01(7u) == volumart::Hash01(7u));
  CHECK(volumart::Hash01(7u) != volumart::Hash01(8u));
  float prev = volumart::Noise1(0.f, 3u);
  for (int i = 1; i <= 4000; ++i)
  {
    const float x = i * 0.005f;
    const float n = volumart::Noise1(x, 3u);
    CHECK(n >= -0.5f);
    CHECK(n <= 0.5f);
    CHECK(std::abs(n - prev) < 0.02f);
    prev = n;
  }
  CHECK(volumart::Noise1(1.25f, 3u) != volumart::Noise1(1.25f, 4u));
  CHECK(volumart::Smoothstep(0.f, 1.f, -1.f) == 0.f);
  CHECK(volumart::Smoothstep(0.f, 1.f, 0.5f) == doctest::Approx(0.5f));
  CHECK(volumart::Smoothstep(0.f, 1.f, 2.f) == 1.f);
}
