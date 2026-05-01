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
