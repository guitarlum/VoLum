#include "third_party/doctest.h"

#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

namespace
{
std::filesystem::path RepoRoot()
{
  return std::filesystem::path(__FILE__).parent_path().parent_path().parent_path();
}

std::string ReadText(const std::filesystem::path& path)
{
  std::ifstream in(path, std::ios::binary);
  REQUIRE(in.good());
  std::ostringstream ss;
  ss << in.rdbuf();
  return ss.str();
}

void RequireContains(const std::string& haystack, const char* needle)
{
  INFO(needle);
  REQUIRE(haystack.find(needle) != std::string::npos);
}
} // namespace

TEST_CASE("POST pedal cards refresh active art state from delay and reverb params")
{
  const std::string source = ReadText(RepoRoot() / "NeuralAmpModeler" / "NeuralAmpModeler.cpp");

  RequireContains(source, "card->SetActiveState(GetParam(kDelayActive)->Bool());");
  RequireContains(source, "card->SetActiveState(GetParam(kReverbActive)->Bool());");
}

TEST_CASE("PRE pedal capture menu toggles closed on second click of same pedal")
{
  const std::string source = ReadText(RepoRoot() / "NeuralAmpModeler" / "NeuralAmpModeler.cpp");
  const std::string triptych = ReadText(RepoRoot() / "NeuralAmpModeler" / "VoLumTriptych.h");

  RequireContains(triptych, "int GetSlot() const { return mSlot; }");
  RequireContains(source, "if (!rawCtrl->IsHidden() && menu && menu->GetSlot() == slot)");
  RequireContains(source, "_VolumHidePreCaptureMenu();");
}

TEST_CASE("PRE pedal capture menu closes from main-area outside click")
{
  const std::string source = ReadText(RepoRoot() / "NeuralAmpModeler" / "NeuralAmpModeler.cpp");

  RequireContains(source, "_ClearVoLumKnobSelection();");
  RequireContains(source, "_VolumHidePreCaptureMenu();");
}

TEST_CASE("Collapsed AMP strip block is taller than the PRE/POST blocks")
{
  // AMP block is 180 H (vs 140 H for PRE/POST) so it visually anchors the row
  // when the user is in PRE or POST view. If somebody changes blockH back to
  // 140 the row loses its centerpiece, so lock the value here.
  const std::string triptych = ReadText(RepoRoot() / "NeuralAmpModeler" / "VoLumTriptych.h");

  RequireContains(triptych, "void _DrawAmpStrip(IGraphics& g, const IRECT& r)");
  RequireContains(triptych, "const float blockH = 180.f;");
}

TEST_CASE("Collapsed AMP strip auto-shrinks the spine font to fit long amp names")
{
  // The amp name is rotated -90 deg and rendered as a single line. Long
  // names like "Diezel Herbert Mk1" only fit at smaller sizes, so the
  // strip MeasureText-probes a descending size table and picks the largest
  // size that fits. Locking the table prevents accidental regression to a
  // single fixed size that would clip long names.
  const std::string triptych = ReadText(RepoRoot() / "NeuralAmpModeler" / "VoLumTriptych.h");

  RequireContains(triptych, "static const float kSpineSizes[] = {16.f, 14.f, 12.f, 11.f, 10.f, 9.f, 8.f};");
  RequireContains(triptych, "g.MeasureText(probe, name, measured);");
  RequireContains(triptych, "spineText.mAngle = -90.f;");
}

TEST_CASE("Collapsed AMP strip falls back to 'AMP' label when amp name is empty")
{
  const std::string triptych = ReadText(RepoRoot() / "NeuralAmpModeler" / "VoLumTriptych.h");

  RequireContains(triptych, "const char* name = mAmpName.empty() ? \"AMP\" : mAmpName.c_str();");
}

TEST_CASE("Collapsed AMP strip hover is gated on the visible block, not the strip rect")
{
  // mAmpRect is the full 70 W x 196 H strip rect (used for click hit-testing
  // so clicks near the block still register). mAmpBlockRect is the visible
  // 70 W x 180 H block; the hover lift only fires when the cursor is over
  // the block so empty whitespace above/below does not light up.
  const std::string triptych = ReadText(RepoRoot() / "NeuralAmpModeler" / "VoLumTriptych.h");

  RequireContains(triptych, "mAmpBlockRect = block;");
  RequireContains(triptych, "&& mAmpBlockRect.W() > 0");
  RequireContains(triptych, "&& mAmpBlockRect.Contains(x, y);");
}
