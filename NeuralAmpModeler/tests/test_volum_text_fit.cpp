#include "third_party/doctest.h"

#include "../VoLumTextFit.h"

#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

namespace
{
std::string ReadPluginText(const char* name)
{
  const auto path = std::filesystem::path(__FILE__).parent_path().parent_path() / name;
  std::ifstream in(path, std::ios::binary);
  std::ostringstream ss;
  ss << in.rdbuf();
  return ss.str();
}

// Stand-in for MeasureText: 6 units per byte, so a width limit trims a known
// number of bytes. Counts every call.
struct CountingMeasure
{
  int calls = 0;
  float operator()(const char* s)
  {
    ++calls;
    return 6.f * static_cast<float>(std::string(s).size());
  }
};

// FitTextToWidth's loop as it was before the memo (VoLumColorHelpers.h at
// 837eb3b1), kept as the oracle for the shared helper.
template <typename Measure>
std::string OldFitTextToWidth(const char* s, float maxW, Measure&& measure)
{
  if (!s || !*s)
    return "";
  std::string str = s;
  if (measure(str.c_str()) <= maxW)
    return str;
  while (str.size() > 1)
  {
    str.pop_back();
    while (!str.empty() && (static_cast<unsigned char>(str.back()) & 0xC0) == 0x80)
      str.pop_back();
    const std::string cand = str + "\xE2\x80\xA6";
    if (measure(cand.c_str()) <= maxW)
      return cand;
  }
  return str + "\xE2\x80\xA6";
}

volum::textfit::FitStyle Style(float maxW, float size = 17.f, const char* font = "Poiret-One", float scale = 1.f)
{
  volum::textfit::FitStyle st;
  st.font = font;
  st.size = size;
  st.maxW = maxW;
  st.scale = scale;
  return st;
}
} // namespace

TEST_CASE("Text fit: shared trim loop returns what FitTextToWidth returned")
{
  const std::vector<std::string> texts = {"", "Clean", "Crunch Rhythm With The Long Lead Channel",
                                          "Caf\xC3\xA9 \xE2\x80\x94 Sm\xC3\xB6rg\xC3\xA5sbord Lead", "x"};
  for (const auto& t : texts)
    for (float w : {0.f, 6.f, 20.f, 60.f, 105.f, 400.f})
    {
      CountingMeasure a, b;
      INFO(t << " @ " << w);
      CHECK(volum::textfit::Fit(t.c_str(), w, a) == OldFitTextToWidth(t.c_str(), w, b));
      CHECK(a.calls == b.calls);
    }
  CountingMeasure m;
  CHECK(volum::textfit::Fit(nullptr, 50.f, m).empty());
  CHECK(m.calls == 0);
}

TEST_CASE("Text fit memo: same inputs do not re-measure; any key change does")
{
  const char* name = "Crunch Rhythm With The Long Lead Channel";
  volum::textfit::Memo memo;
  CountingMeasure m;

  const std::string first = memo.Get(name, Style(105.f), m);
  const int measuredOnce = m.calls;
  CHECK(measuredOnce > 1); // the name overflows, so the trim loop ran
  CHECK(first == volum::textfit::Fit(name, 105.f, CountingMeasure{}));

  // Every later frame with the same row: no measuring at all.
  for (int frame = 0; frame < 100; ++frame)
    CHECK(memo.Get(name, Style(105.f), m) == first);
  CHECK(m.calls == measuredOnce);

  // Hover narrows the amp line: new width, new fit.
  const std::string narrower = memo.Get(name, Style(61.f), m);
  CHECK(m.calls > measuredOnce);
  CHECK(narrower == volum::textfit::Fit(name, 61.f, CountingMeasure{}));

  auto remeasures = [&](const char* s, const volum::textfit::FitStyle& st) {
    const int before = m.calls;
    const std::string got = memo.Get(s, st, m);
    CHECK(got == volum::textfit::Fit(s, st.maxW, CountingMeasure{}));
    return m.calls > before;
  };
  CHECK(remeasures(name, Style(105.f))); // width back
  CHECK_FALSE(remeasures(name, Style(105.f))); // unchanged
  CHECK(remeasures("Lead Boost", Style(105.f))); // renamed in SetData
  CHECK(remeasures("Lead Boost", Style(105.f, 18.f))); // font size
  CHECK(remeasures("Lead Boost", Style(105.f, 18.f, "Michroma"))); // font face
  CHECK(remeasures("Lead Boost", Style(105.f, 18.f, "Michroma", 2.f))); // rescale
  auto aligned = Style(105.f, 18.f, "Michroma", 2.f);
  aligned.align = 2;
  CHECK(remeasures("Lead Boost", aligned));
  CHECK_FALSE(remeasures("Lead Boost", aligned));
  memo.Reset();
  CHECK(remeasures("Lead Boost", aligned)); // OnRescale reset
}

TEST_CASE("Text fit memo: PLAY rail rows draw through the memo and rescale resets it")
{
  const std::string play = ReadPluginText("VoLumPlaySurface.h");
  const std::string helpers = ReadPluginText("VoLumColorHelpers.h");
  CHECK(play.find("FitTextToWidth(memo, g, t, s, r.W())") != std::string::npos);
  CHECK(play.find("rowText.name.Reset();") != std::string::npos);
  CHECK(play.find("rowText.amp.Reset();") != std::string::npos);
  CHECK(play.find("const std::string fitted = FitTextToWidth(g, t, s, r.W());") == std::string::npos);
  // The memo key carries the backing scale, so a DPI change re-measures even
  // before OnRescale runs.
  CHECK(helpers.find("style.scale = g.GetTotalScale();") != std::string::npos);
}
