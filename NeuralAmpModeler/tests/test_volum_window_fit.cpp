#include "third_party/doctest.h"

#include "../VoLumWindowFit.h"

#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

namespace
{
std::string ReadPluginFile(const char* name)
{
  const auto path = std::filesystem::path(__FILE__).parent_path().parent_path() / name;
  std::ifstream in(path, std::ios::binary);
  REQUIRE(in.good());
  std::ostringstream ss;
  ss << in.rdbuf();
  return ss.str();
}
} // namespace

TEST_CASE("F-66 the draw-scale limit keeps the whole window inside the work area")
{
  // 900x600 UI at 100% display scale, 16 px of side borders and 39 px of title bar.
  // A 1920x1040 work area (1080 less the taskbar): height decides, (1040-39)/600.
  const float s = volum::MaxDrawScaleForWorkArea(1920.f, 1040.f, 16.f, 39.f, 900.f, 600.f, 1.f, 0.5f, 4.f);
  CHECK(s == doctest::Approx(1.668f).epsilon(0.0005));
  CHECK(900.f * s + 16.f <= 1920.f);
  CHECK(600.f * s + 39.f <= 1040.f);

  // Width decides on a narrow work area.
  const float narrow = volum::MaxDrawScaleForWorkArea(1000.f, 1040.f, 16.f, 39.f, 900.f, 600.f, 1.f, 0.5f, 4.f);
  CHECK(narrow == doctest::Approx(1.093f).epsilon(0.001));

  // A display scale of 150% makes the same UI that much larger in pixels.
  const float hidpi = volum::MaxDrawScaleForWorkArea(2560.f, 1400.f, 24.f, 58.f, 900.f, 600.f, 1.5f, 0.5f, 4.f);
  CHECK(900.f * hidpi * 1.5f + 24.f <= 2560.f);
  CHECK(600.f * hidpi * 1.5f + 58.f <= 1400.f);
  CHECK(hidpi == doctest::Approx(1.491f).epsilon(0.001));
}

TEST_CASE("F-66 the draw-scale limit never goes below the minimum or above the hard cap")
{
  // A huge monitor still stops at the hard cap.
  CHECK(volum::MaxDrawScaleForWorkArea(7680.f, 4320.f, 16.f, 39.f, 900.f, 600.f, 1.f, 0.5f, 4.f) == 4.f);
  // A work area too small for the minimum size must not push the limit under it.
  CHECK(volum::MaxDrawScaleForWorkArea(300.f, 200.f, 16.f, 39.f, 900.f, 600.f, 1.f, 0.5f, 4.f) == 0.5f);
  // Nonsense input falls back to the cap rather than a zero or negative limit.
  CHECK(volum::MaxDrawScaleForWorkArea(0.f, 0.f, 0.f, 0.f, 900.f, 600.f, 1.f, 0.5f, 4.f) == 4.f);
  CHECK(volum::MaxDrawScaleForWorkArea(1920.f, 1040.f, 0.f, 0.f, 0.f, 600.f, 1.f, 0.5f, 4.f) == 4.f);
}

TEST_CASE("F-66 the standalone applies the work-area limit on every idle")
{
  const std::string cpp = ReadPluginFile("NeuralAmpModeler.cpp");
  const auto idle = cpp.find("void NeuralAmpModeler::OnIdle()");
  REQUIRE(idle != std::string::npos);
  const auto call = cpp.find("volum::ApplyWorkAreaScaleLimit(GetUI());", idle);
  REQUIRE(call != std::string::npos);
  CHECK(call < cpp.find("_VolumConsumeUpdateResult();", idle)); // inside OnIdle, not a later function
  CHECK(cpp.find("#include \"VoLumWindowFitWin.h\"") != std::string::npos);

  const std::string win = ReadPluginFile("VoLumWindowFitWin.h");
  CHECK(win.find("MonitorFromWindow") != std::string::npos);
  CHECK(win.find("rcWork") != std::string::npos);
  CHECK(win.find("SetScaleConstraints(") != std::string::npos);
}