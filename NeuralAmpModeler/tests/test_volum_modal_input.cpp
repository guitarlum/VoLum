// Clicks and keys that reach a surface the user did not aim at: the second click
// of a double-click that opened a modal, a dropdown left over PLAY, a native file
// panel that eats the mouse-up, and modal boxes opened from OnIdle.
//
// These controls need a live IGraphics, so most pins read the source.

#include "third_party/doctest.h"

#include "../VoLumNoticeDeferral.h"
#include "../VoLumOverlayStack.h"
#include "../VoLumSecondPress.h"

#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace
{
std::filesystem::path RepoRoot()
{
  return std::filesystem::path(__FILE__).parent_path().parent_path().parent_path();
}

std::string ReadText(const std::filesystem::path& path)
{
  std::ifstream in(path, std::ios::binary);
  REQUIRE(in);
  return std::string((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
}

std::string Source(const char* file)
{
  return ReadText(RepoRoot() / "NeuralAmpModeler" / file);
}

std::string Between(const std::string& src, const char* from, const char* to)
{
  const auto start = src.find(from);
  REQUIRE_MESSAGE(start != std::string::npos, from);
  const auto end = src.find(to, start + std::string(from).size());
  REQUIRE_MESSAGE(end != std::string::npos, to);
  return src.substr(start, end - start);
}

// From a member signature to the next closing brace at member indentation.
std::string MemberBody(const std::string& src, const char* signature)
{
  return Between(src, signature, "\n  }");
}

// A plugin member function, up to the next one.
std::string PluginFunction(const std::string& src, const char* signature)
{
  return Between(src, signature, " NeuralAmpModeler::");
}

bool Contains(const std::string& haystack, const char* needle)
{
  return haystack.find(needle) != std::string::npos;
}

size_t Count(const std::string& haystack, const char* needle)
{
  size_t n = 0;
  for (auto at = haystack.find(needle); at != std::string::npos; at = haystack.find(needle, at + 1))
    ++n;
  return n;
}

// Top-level product sources (not tests, not vendored code).
std::vector<std::filesystem::path> ProductSources()
{
  std::vector<std::filesystem::path> out;
  for (const auto& entry : std::filesystem::directory_iterator(RepoRoot() / "NeuralAmpModeler"))
  {
    const auto ext = entry.path().extension();
    if (entry.is_regular_file() && (ext == ".h" || ext == ".cpp"))
      out.push_back(entry.path());
  }
  return out;
}
} // namespace

TEST_CASE("Name dialog ignores the second click of the double-click that opened it")
{
  // "+ Add this sound" and "Save current as new..." open the dialog on a mouse-down.
  // The dialog spans the window, so the second click of that double-click arrives
  // as its OnMouseDblClick: outside the box it cancelled, over Save it saved a name
  // nobody confirmed.
  const std::string dialog = Source("VoLumNameDialog.h");
  CHECK(Contains(
    MemberBody(dialog, "void OnMouseDown(float x, float y, const IMouseMod& mod) override"), "mSecondPress.Press();"));
  const std::string dbl = MemberBody(dialog, "void OnMouseDblClick(float x, float y, const IMouseMod& mod) override");
  const auto take = dbl.find("if (mSecondPress.Take())");
  REQUIRE(take != std::string::npos);
  const auto press = dbl.find("Press(x, y, mod, true);");
  REQUIRE(press != std::string::npos);
  CHECK(take < press);
  // A press on an earlier dialog cannot pair with a click after the next open.
  CHECK(Contains(MemberBody(dialog, "  void Open("), "mSecondPress = {};"));

  volum::ui::SecondPressGate opened;
  CHECK_FALSE(opened.TakeAt(100.0));
}

TEST_CASE("P and a mode switch cannot leave a dropdown open over PLAY")
{
  // P was blocked by the preset menu only, and the mode switch hid only that menu,
  // so the Custom IR dropdown stayed up over PLAY and changed the hidden cab.
  const std::string list = Between(Source("NeuralAmpModeler.h"), "inline constexpr int kVoLumDropdownTags[] = {", "};");
  for (const char* tag :
       {"kCtrlTagVoLumPresetMenu", "kCtrlTagVoLumIrMenu", "kCtrlTagVoLumPreCaptureMenu", "kCtrlTagVoLumSupportAmpMenu"})
  {
    INFO(tag);
    CHECK(Contains(list, tag));
  }

  const std::string guard =
    Between(Source("VoLumLayoutBuild.inc.cpp"), "IsUiModeToggleKey(key.VK, key.C, key.A) && !nameDialogOpen",
            "_VolumSetUiMode(mVolumUiMode");
  CHECK(Contains(guard, "volum::ui::AnyTagOpen(kVoLumDropdownTags, isOpen)"));

  const std::string setMode =
    PluginFunction(Source("VoLumPlayRuntime.inc.cpp"), "void NeuralAmpModeler::_VolumSetUiMode(");
  CHECK(Contains(setMode, "for (int tag : kVoLumDropdownTags)"));

  constexpr int tags[] = {4, 5, 6};
  CHECK(volum::ui::AnyTagOpen(tags, [](int tag) { return tag == 6; }));
  CHECK_FALSE(volum::ui::AnyTagOpen(tags, [](int) { return false; }));
}

TEST_CASE("One list names the dropdowns")
{
  // Four hand-written copies of the list (H, the gear, Escape, the keyboard stack)
  // and two partial ones (P, the mode switch) are how the IR menu went missing.
  std::string all;
  for (const auto& path : ProductSources())
  {
    const std::string src = ReadText(path);
    INFO(path.filename().string());
    CHECK_FALSE(Contains(src, "kDropdownTags"));
    CHECK_FALSE(Contains(src, "isOpen(kCtrlTagVoLumIrMenu)"));
    if (path.filename() != "NeuralAmpModeler.h")
      CHECK_FALSE(Contains(src, "kCtrlTagVoLumPresetMenu, kCtrlTagVoLumIrMenu"));
    all += src;
  }
  CHECK(Contains(all, "stack.dropdown = volum::ui::AnyTagOpen(kVoLumDropdownTags, isOpen);"));
  // H, the gear, Escape, the main-area click and the mode switch.
  CHECK(Count(all, "for (int tag : kVoLumDropdownTags)") >= 5);
}

TEST_CASE("File pickers release the mouse before the native panel opens")
{
  // macOS runs the panel modally inside the mouse-down and swallows the mouse-up,
  // so the control that took the press stayed captured and got the next click
  // wherever it landed. iPlug releases the capture itself on Windows only.
  //
  // NeuralAmpModelerControls.h only prompts inside upstream's NAMFileBrowserControl,
  // which VoLum never attaches.
  size_t sites = 0;
  for (const auto& path : ProductSources())
  {
    const std::string src = ReadText(path);
    INFO(path.filename().string());
    CHECK_FALSE(Contains(src, "new NAMFileBrowserControl("));
    if (path.filename() == "NeuralAmpModelerControls.h")
      continue;
    for (auto at = src.find("->PromptForFile"); at != std::string::npos; at = src.find("->PromptForFile", at + 1))
    {
      ++sites;
      const auto lineStart = src.rfind('\n', at);
      const std::string before = src.substr(lineStart < 200 ? 0 : lineStart - 200, 200);
      CHECK(Contains(before, "ReleaseMouseCapture();"));
    }
  }
  CHECK(sites >= 4); // Pack export, Pack import, Manage import, builder captures
}

TEST_CASE("Recall and restore never open an IR error box")
{
  // MIDI recall and host restore select the IR non-interactively from OnIdle. A
  // modal box there pumps the timer that calls OnIdle again, mid-restore.
  const std::string rig = Source("VoLumSceneRig.inc.cpp");
  const std::string select =
    PluginFunction(rig, "void NeuralAmpModeler::_VolumSelectIR(int irIdx, bool support, bool interactive)");
  REQUIRE(Count(select, "_ShowMessageBox(") == 1);
  const auto box = select.find("_ShowMessageBox(");
  CHECK(Contains(select.substr(box - 80, 80), "if (interactive)"));
  CHECK(Contains(rig, "_VolumSelectIR(idx, support, /*interactive=*/false);"));
}

TEST_CASE("The library notice waits until no control holds the mouse")
{
  // iPlug's message box releases the mouse capture, and a captured knob's host
  // gesture is only ended by the mouse-up the box swallowed.
  const std::string idle = PluginFunction(Source("NeuralAmpModeler.cpp"), "void NeuralAmpModeler::OnIdle()");
  const std::string guard = Between(idle, "if (mVolumPendingLibraryNotice.empty())", "_ShowMessageBox(");
  CHECK(Contains(guard, "mVolumNoticeDeferral.ShouldShow(gfx->ControlIsCaptured())"));
  CHECK(guard.find("ShouldShow(gfx->ControlIsCaptured())") < guard.find("std::move(mVolumPendingLibraryNotice)"));
  CHECK(Contains(guard, "mVolumNoticeDeferral.Reset();"));
}

TEST_CASE("A control that never releases the mouse cannot hold the library notice back forever")
{
  volum::ui::NoticeDeferral wait;
  // Nothing captured: shown at once.
  CHECK(wait.ShouldShowAt(false, 0.0));

  // Captured: held back, but only for a bounded time.
  CHECK_FALSE(wait.ShouldShowAt(true, 1000.0));
  CHECK_FALSE(wait.ShouldShowAt(true, 1000.0 + volum::ui::kNoticeCaptureWaitMs - 1.0));
  CHECK(wait.ShouldShowAt(true, 1000.0 + volum::ui::kNoticeCaptureWaitMs));

  // A drag that ends restarts the wait for the next capture.
  CHECK(wait.ShouldShowAt(false, 20000.0));
  CHECK_FALSE(wait.ShouldShowAt(true, 21000.0));
  CHECK_FALSE(wait.ShouldShowAt(true, 21000.0 + volum::ui::kNoticeCaptureWaitMs - 1.0));

  // A short drag does not age into a long one.
  volum::ui::NoticeDeferral drag;
  CHECK_FALSE(drag.ShouldShowAt(true, 0.0));
  CHECK(drag.ShouldShowAt(false, 2000.0));
  CHECK_FALSE(drag.ShouldShowAt(true, 4000.0));
  CHECK_FALSE(drag.ShouldShowAt(true, 8000.0));
  CHECK(drag.ShouldShowAt(true, 9000.0));

  // The wait is short enough to be noticed as "soon", long enough for a drag.
  CHECK(volum::ui::kNoticeCaptureWaitMs >= 2000.0);
  CHECK(volum::ui::kNoticeCaptureWaitMs <= 10000.0);
}

namespace
{
// A control built the way every VoLum one is: the press arms the gate, the
// double-click replays it as a press only if the gate was armed.
struct GatedControl
{
  volum::ui::SecondPressGate gate;
  int presses = 0;
  void OnMouseDown()
  {
    const auto pressed = gate.Press();
    ++presses;
  }
  void OnMouseDblClick()
  {
    if (gate.Take())
      OnMouseDown();
  }
  // The first press went to an overlay on top: this control never saw it.
  void OverlayConsumesFirstPress() {}
};
} // namespace

TEST_CASE("A replayed double-click does not arm the gate for a later overlay-consumed press")
{
  GatedControl control;

  // Genuine double-click on the control: press, then the replay.
  control.OnMouseDown();
  control.OnMouseDblClick();
  CHECK(control.presses == 2);

  // Later an overlay takes the first press of a double-click over the same control;
  // the second click must not reach it.
  control.OverlayConsumesFirstPress();
  control.OnMouseDblClick();
  CHECK(control.presses == 2);

  // The control still takes its own double-click afterwards.
  control.OnMouseDown();
  control.OnMouseDblClick();
  CHECK(control.presses == 4);
}

TEST_CASE("A Take that replays nothing does not stop the next genuine press arming")
{
  // An own-double-click action takes the gate but never calls OnMouseDown.
  volum::ui::SecondPressGate gate;
  gate.ArmAt(1000.0);
  REQUIRE(gate.TakeAt(1100.0));
  CHECK_FALSE(gate.ConsumeReplayAt(1100.0 + volum::ui::kReplayEntryWindowMs + 1.0));

  // Straight after the Take it is the replay.
  gate.ArmAt(5000.0);
  REQUIRE(gate.TakeAt(5100.0));
  CHECK(gate.ConsumeReplayAt(5100.0));
  CHECK_FALSE(gate.ConsumeReplayAt(5100.0)); // once

  // A refused Take marks nothing.
  CHECK_FALSE(gate.TakeAt(5200.0));
  CHECK_FALSE(gate.ConsumeReplayAt(5200.0));
}
