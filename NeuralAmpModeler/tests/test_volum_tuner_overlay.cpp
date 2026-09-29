#include "third_party/doctest.h"

#include "../VoLumTunerDirty.h"

#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

namespace
{
std::string ReadTunerSource(const char* name)
{
  const auto path = std::filesystem::path(__FILE__).parent_path().parent_path() / name;
  std::ifstream in(path, std::ios::binary);
  std::ostringstream ss;
  ss << in.rdbuf();
  return ss.str();
}

std::string Between(const std::string& text, const std::string& from, const std::string& to)
{
  const auto a = text.find(from);
  REQUIRE(a != std::string::npos);
  const auto b = text.find(to, a + from.size());
  REQUIRE(b != std::string::npos);
  return text.substr(a, b - a);
}

volum::TunerResult Note(int note, int octave, float hz, float cents)
{
  volum::TunerResult r;
  r.valid = true;
  r.noteIndex = note;
  r.octave = octave;
  r.frequency = hz;
  r.cents = cents;
  return r;
}
} // namespace

TEST_CASE("Tuner overlay: an unchanged reading does not repaint the panel")
{
  // No pitch: the panel reads "Play a note..." whatever the other fields hold.
  volum::TunerResult none;
  volum::TunerResult noneStale = none;
  noneStale.frequency = 81.2f;
  noneStale.cents = -3.f;
  noneStale.noteIndex = 4;
  CHECK(volum::TunerResultDrawsSame(none, none));
  CHECK(volum::TunerResultDrawsSame(none, noneStale));

  const auto e2 = Note(4, 2, 82.4f, 1.5f);
  CHECK(volum::TunerResultDrawsSame(e2, e2));
  CHECK_FALSE(volum::TunerResultDrawsSame(none, e2));
  CHECK_FALSE(volum::TunerResultDrawsSame(e2, none));
  CHECK_FALSE(volum::TunerResultDrawsSame(e2, Note(5, 2, 82.4f, 1.5f)));
  CHECK_FALSE(volum::TunerResultDrawsSame(e2, Note(4, 3, 82.4f, 1.5f)));
  CHECK_FALSE(volum::TunerResultDrawsSame(e2, Note(4, 2, 82.5f, 1.5f)));
  CHECK_FALSE(volum::TunerResultDrawsSame(e2, Note(4, 2, 82.4f, 1.25f)));

  const std::string overlay = ReadTunerSource("VoLumTunerMetronomeOverlay.h");
  const std::string set = Between(overlay, "void SetResult(const volum::TunerResult& r)", "void Show()");
  const auto gate = set.find("if (volum::TunerResultDrawsSame(mResult, r))");
  REQUIRE(gate != std::string::npos);
  CHECK(gate < set.find("SetDirty(false);"));
}

TEST_CASE("Tuner overlay: the scrim is its own control and every hide path hides it")
{
  const std::string overlay = ReadTunerSource("VoLumTunerMetronomeOverlay.h");
  const std::string layout = ReadTunerSource("VoLumLayoutBuild.inc.cpp");
  const std::string rig = ReadTunerSource("VoLumSceneRig.inc.cpp");

  // The tuner repaints only its padded panel; the full-window scrim is a static
  // control. The tuner still takes clicks and keys over the whole window.
  const std::string tuner = Between(overlay, "class VoLumTunerControl", "class VoLumMetronomeButtonControl");
  CHECK(overlay.find("class VoLumTunerScrimControl") != std::string::npos);
  CHECK(tuner.find(": IControl(PanelFor(bounds).GetPadded(kPanelPad))") != std::string::npos);
  CHECK(tuner.find("SetTargetRECT(bounds);") != std::string::npos);
  CHECK(tuner.find("g.FillRect(IColor(200, 8, 10, 14), mRECT);") == std::string::npos);
  const std::string scrim = Between(overlay, "class VoLumTunerScrimControl", "class VoLumTunerControl");
  CHECK(scrim.find("mIgnoreMouse = true;") != std::string::npos);
  CHECK(scrim.find("g.FillRect(IColor(200, 8, 10, 14), mRECT);") != std::string::npos);

  // One funnel: Hide() is virtual, and the override hides the scrim with the
  // panel. Nothing in the tuner goes around it.
  const std::string hide = Between(tuner, "void Hide(bool hide) override", "}");
  CHECK(hide.find("IControl::Hide(hide);") != std::string::npos);
  CHECK(hide.find("mScrim->Hide(hide);") != std::string::npos);
  size_t baseHides = 0;
  for (auto at = tuner.find("IControl::Hide("); at != std::string::npos; at = tuner.find("IControl::Hide(", at + 1))
    ++baseHides;
  CHECK(baseHides == 1);
  CHECK(tuner.find("mHide =") == std::string::npos);

  // Every path that shows or hides the tuner, all through Hide():
  //  - Show() (T key / tuner button)            -> Hide(false)
  //  - _Dismiss(): click outside the panel, Esc on the tuner, Esc via the key handler -> Hide(true)
  //  - _ToggleVoLumTuner() closing it            -> tuner->Hide(true)
  CHECK(Between(tuner, "void Show()", "}").find("Hide(false);") != std::string::npos);
  CHECK(Between(tuner, "void _Dismiss()", "}").find("Hide(true);") != std::string::npos);
  CHECK(Between(rig, "void NeuralAmpModeler::_ToggleVoLumTuner()", "void NeuralAmpModeler::").find("tuner->Hide(true);")
        != std::string::npos);
  CHECK(layout.find("tuner->As<VoLumTunerControl>()->Dismiss();") != std::string::npos);

  // Z-order: the scrim sits directly under the tuner.
  CHECK(layout.find("tunerCtrl->SetScrim(tunerScrim);") != std::string::npos);
  const auto scrimAt = layout.find("pGraphics->AttachControl(tunerScrim)->Hide(true);");
  REQUIRE(scrimAt != std::string::npos);
  const auto next = layout.find("pGraphics->AttachControl(", scrimAt + 1);
  CHECK(next == layout.find("pGraphics->AttachControl(tunerCtrl, kCtrlTagVoLumTuner)->Hide(true);"));
}
