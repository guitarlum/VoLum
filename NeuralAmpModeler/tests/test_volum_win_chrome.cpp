#include "third_party/doctest.h"

#include "VoLumWinChromeModel.h"

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

std::string ReadRepoText(const std::filesystem::path& relative)
{
  std::ifstream in(RepoRoot() / relative, std::ios::binary);
  REQUIRE(in.good());
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

// One control statement per line, leading whitespace and CR dropped, so the .rc
// block and its swell_resgen copy compare equal however each is indented.
std::string NormalizedLines(const std::string& block)
{
  std::istringstream in(block);
  std::string line, out;
  while (std::getline(in, line))
  {
    if (!line.empty() && line.back() == '\r')
      line.pop_back();
    const auto start = line.find_first_not_of(" \t");
    if (start == std::string::npos)
      continue;
    out += line.substr(start);
    out += '\n';
  }
  return out;
}

using iplug::VoLumRgb;
} // namespace

TEST_CASE("Win chrome: each Windows build gets the dark title bar switch it understands")
{
  using iplug::VoLumDarkCaptionAttribute;
  // Windows 10 1803 and older have no dark title bar: keep the light one.
  CHECK(VoLumDarkCaptionAttribute(0) == 0u);
  CHECK(VoLumDarkCaptionAttribute(17134) == 0u);
  CHECK(VoLumDarkCaptionAttribute(17762) == 0u);
  // 1809 to 1909 only know the undocumented attribute 19.
  CHECK(VoLumDarkCaptionAttribute(17763) == 19u);
  CHECK(VoLumDarkCaptionAttribute(18363) == 19u);
  CHECK(VoLumDarkCaptionAttribute(18984) == 19u);
  // 20H1 onwards, Windows 11 included: DWMWA_USE_IMMERSIVE_DARK_MODE.
  CHECK(VoLumDarkCaptionAttribute(18985) == 20u);
  CHECK(VoLumDarkCaptionAttribute(19045) == 20u);
  CHECK(VoLumDarkCaptionAttribute(22631) == 20u);
  CHECK(VoLumDarkCaptionAttribute(26100) == 20u);

  // Only Windows 11 paints the caption in VoLum's exact background colour.
  CHECK_FALSE(iplug::VoLumSupportsCaptionColor(19045));
  CHECK_FALSE(iplug::VoLumSupportsCaptionColor(21999));
  CHECK(iplug::VoLumSupportsCaptionColor(22000));
  CHECK(iplug::VoLumSupportsCaptionColor(26100));
  CHECK(iplug::kVoLumDwmCaptionColorAttr == 35u);
  CHECK(iplug::kVoLumDwmTextColorAttr == 36u);
}

TEST_CASE("Win chrome: colours reach GDI as 0x00BBGGRR and blend like the canvas")
{
  CHECK(iplug::VoLumColorRef(VoLumRgb{0x11, 0x22, 0x33}) == 0x00332211u);
  CHECK(iplug::VoLumColorRef(iplug::volum_chrome::kBg) == 0x00181111u);

  const VoLumRgb white{255, 255, 255};
  const VoLumRgb black{0, 0, 0};
  CHECK(iplug::VoLumBlend(white, 255, black) == white);
  CHECK(iplug::VoLumBlend(white, 0, black) == black);
  CHECK(iplug::VoLumBlend(white, 128, black) == VoLumRgb{128, 128, 128});
  // VoLumColors::FRAME (brass at 72/255) over the panel.
  CHECK(iplug::volum_chrome::kFrame == VoLumRgb{73, 62, 44});
}

TEST_CASE("Win chrome: the Preferences palette is VoLum's palette")
{
  // The dialog cannot include IGraphics, so it carries copies; this keeps them honest.
  const std::string colors = ReadRepoText("NeuralAmpModeler/VoLumColorHelpers.h");
  using namespace iplug::volum_chrome;
  auto opaque = [](const char* name, VoLumRgb c) {
    return std::string("IColor ") + name + "(255, " + std::to_string(c.r) + ", " + std::to_string(c.g) + ", "
           + std::to_string(c.b) + ")";
  };
  CHECK(colors.find(opaque("BG", kBg)) != std::string::npos);
  CHECK(colors.find(opaque("PANEL_TOP", kPanel)) != std::string::npos);
  CHECK(colors.find(opaque("WELL_DARK", kWell)) != std::string::npos);
  CHECK(colors.find(opaque("TEXT_DIM", kTextDim)) != std::string::npos);
  CHECK(colors.find(opaque("TEXT_MED", kTextMed)) != std::string::npos);
  CHECK(colors.find(opaque("TEXT_BRIGHT", kTextBright)) != std::string::npos);
  CHECK(colors.find(opaque("GOLD", kGold)) != std::string::npos);
  CHECK(colors.find(opaque("CORNER", kBrass)) != std::string::npos);
  CHECK(colors.find("IColor SEL_BORDER(235, 226, 156, 112)") != std::string::npos);
  CHECK(kSelBorder == VoLumRgb{226, 156, 112});
  CHECK(colors.find("IColor FRAME(" + std::to_string(kFrameAlpha) + ", 200, 162, 78)") != std::string::npos);
  CHECK(colors.find("IColor SEL_BG(" + std::to_string(kSelBgAlpha) + ", 200, 162, 78)") != std::string::npos);
  CHECK(colors.find("IColor SEL_BG_SOFT(" + std::to_string(kHoverAlpha) + ", 200, 162, 78)") != std::string::npos);
}

TEST_CASE("Win chrome: the standalone main window has no menu bar, macOS keeps its menu")
{
  const std::string rc = ReadRepoText("NeuralAmpModeler/resources/main.rc");
  const std::string mainDlg = Between(rc, "IDD_DIALOG_MAIN DIALOG", "END");
  CHECK(mainDlg.find("MENU IDR_MENU1") == std::string::npos);
  // macOS builds its menu bar from this resource (main.rc_mac_menu).
  CHECK(rc.find("IDR_MENU1 MENU") != std::string::npos);
  CHECK(ReadRepoText("NeuralAmpModeler/resources/main.rc_mac_menu").find("IDR_MENU1") != std::string::npos);
  // Ctrl+, still opens Preferences without a menu to carry it.
  CHECK(Between(rc, "IDR_ACCELERATOR1 ACCELERATORS", "END").find("VK_OEM_COMMA,   ID_PREFERENCES")
        != std::string::npos);

  // The release build used to strip the Debug menu off the bar; there is no bar now.
  const std::string appMain = ReadRepoText("iPlug2/IPlug/APP/IPlugAPP_main.cpp");
  CHECK(appMain.find("RemoveMenu(menu, 1, MF_BYPOSITION)") == std::string::npos);
}

TEST_CASE("Win chrome: Preferences offers the MIDI input port and nothing VoLum ignores")
{
  const std::string rc = ReadRepoText("NeuralAmpModeler/resources/main.rc");
  const std::string prefs = Between(rc, "IDD_DIALOG_PREF DIALOG", "\nEND");
  // Program Change Sound recall listens on this port: it stays.
  CHECK(prefs.find("IDC_COMBO_MIDI_IN_DEV,") != std::string::npos);
  // VoLum sends no MIDI, and MIDICallback never filters on the input channel.
  CHECK(prefs.find("IDC_COMBO_MIDI_OUT_DEV") == std::string::npos);
  CHECK(prefs.find("IDC_COMBO_MIDI_OUT_CHAN") == std::string::npos);
  CHECK(prefs.find("IDC_COMBO_MIDI_IN_CHAN") == std::string::npos);
  CHECK(prefs.find("Output Channel") == std::string::npos);
  CHECK(prefs.find("Input Channel") == std::string::npos);
  // The audio controls the smoke script drives by id are all still there.
  for (const char* id :
       {"IDC_COMBO_AUDIO_DRIVER,", "IDC_COMBO_AUDIO_IN_DEV,", "IDC_COMBO_AUDIO_OUT_DEV,", "IDC_COMBO_AUDIO_BUF_SIZE,",
        "IDC_COMBO_AUDIO_SR,", "IDC_COMBO_AUDIO_IN_L,", "IDC_COMBO_AUDIO_OUT_L,", "IDC_COMBO_AUDIO_OUT_R,",
        "IDC_BUTTON_OS_DEV_SETTINGS,", "IDOK,", "IDAPPLY,", "IDCANCEL,"})
  {
    INFO(id);
    CHECK(prefs.find(id) != std::string::npos);
  }

  // swell_resgen regenerates the macOS dialog from main.rc at build time; the
  // committed copy must already match so a Windows-only edit cannot drift.
  const std::string mac = ReadRepoText("NeuralAmpModeler/resources/main.rc_mac_dlg");
  const std::string macPrefs = Between(mac, "SWELL_DEFINE_DIALOG_RESOURCE_BEGIN(IDD_DIALOG_PREF", "\nEND");
  REQUIRE(prefs.find("\nBEGIN") != std::string::npos);
  REQUIRE(macPrefs.find("\nBEGIN") != std::string::npos);
  CHECK(NormalizedLines(prefs.substr(prefs.find("\nBEGIN")))
        == NormalizedLines(macPrefs.substr(macPrefs.find("\nBEGIN"))));
  CHECK(macPrefs.find("\"Preferences\",223,264,") != std::string::npos);
  CHECK(prefs.find("IDD_DIALOG_PREF DIALOG 0, 0, 223, 264") != std::string::npos);

  // The populate code still reaches for the removed combos; it must stop first.
  const std::string dialog = ReadRepoText("iPlug2/IPlug/APP/IPlugAPP_dialog.cpp");
  const std::string populate = Between(dialog, "bool IPlugAPPHost::PopulateMidiDialogs", "\n}");
  const auto guard = populate.find("if (!GetDlgItem(hwndDlg, IDC_COMBO_MIDI_OUT_DEV))");
  REQUIRE(guard != std::string::npos);
  CHECK(guard < populate.find("IDC_COMBO_MIDI_OUT_DEV,CB_ADDSTRING"));
  CHECK(guard < populate.find("IDC_COMBO_MIDI_IN_CHAN,CB_ADDSTRING"));
  CHECK(guard > populate.find("IDC_COMBO_MIDI_IN_DEV,CB_SETCURSEL"));
}

TEST_CASE("Win chrome: both standalone windows get the dark caption and Preferences the skin")
{
  const std::string dialog = ReadRepoText("iPlug2/IPlug/APP/IPlugAPP_dialog.cpp");

  const std::string mainProc = Between(dialog, "WDL_DLGRET IPlugAPPHost::MainDlgProc", "case WM_TIMER:");
  const auto caption = mainProc.find("VoLumApplyDarkCaption(hwndDlg);");
  REQUIRE(caption != std::string::npos);
  // Before the first show, so the window never flashes a light caption.
  CHECK(caption < mainProc.find("ShowWindow(hwndDlg, SW_SHOW);"));

  const std::string prefsProc = Between(dialog, "WDL_DLGRET IPlugAPPHost::PreferencesDlgProc", "case WM_COMMAND:");
  const auto skinHook = prefsProc.find("VoLumPrefsSkinMessage(hwndDlg, uMsg, wParam, lParam, skinResult)");
  REQUIRE(skinHook != std::string::npos);
  CHECK(skinHook < prefsProc.find("switch(uMsg)"));
  const auto attach = prefsProc.find("VoLumPrefsSkinAttach(hwndDlg, gHINSTANCE, JOSEFINSANS_FN, ");
  REQUIRE(attach != std::string::npos);
  CHECK(prefsProc.find("VoLumApplyDarkCaption(hwndDlg);") < attach);
  // The skin rebuilds the combos owner-drawn; populating first would fill the
  // controls it is about to destroy.
  CHECK(attach < prefsProc.find("_this->PopulatePreferencesDialog(hwndDlg);"));
  CHECK(prefsProc.find("VoLumPrefsSkinDetach(hwndDlg);") != std::string::npos);

  // The skin keeps every control id and the dialog's own text and handlers.
  const std::string chrome = ReadRepoText("iPlug2/IPlug/APP/VoLumWinChrome.h");
  CHECK(chrome.find("const int id = GetDlgCtrlID(old);") != std::string::npos);
  CHECK(chrome.find("IsWindowUnicode(old)") != std::string::npos);
  CHECK(chrome.find("GW_HWNDPREV") != std::string::npos);
  CHECK(chrome.find("CBS_OWNERDRAWFIXED | CBS_HASSTRINGS") != std::string::npos);
  // Every DWM / uxtheme entry point is resolved at run time, so older Windows starts.
  CHECK(chrome.find("LoadLibraryW(L\"dwmapi.dll\")") != std::string::npos);
  CHECK(chrome.find("LoadLibraryW(L\"uxtheme.dll\")") != std::string::npos);
  CHECK(chrome.find("#include <dwmapi.h>") == std::string::npos);
  CHECK(chrome.find("#include <uxtheme.h>") == std::string::npos);
}

TEST_CASE("Win chrome: Settings opens Preferences and the About card links the manual")
{
  const std::string controls = ReadRepoText("NeuralAmpModeler/NeuralAmpModelerControls.h");
  const std::string signal = Between(controls, "void _BuildSignalTab(const IRECT& body)", "void _BuildMidiTab(");
  const std::string app = Between(signal, "// The Windows standalone has no menu bar", "#else");
  CHECK(app.find("\"Audio & MIDI devices...\"") != std::string::npos);
  CHECK(app.find("plugin->_VolumOpenAudioPreferences();") != std::string::npos);
  CHECK(controls.find("File > Preferences") == std::string::npos);
  CHECK(ReadRepoText("NeuralAmpModeler/VoLumSettingsTabs.h").find("File > Preferences") == std::string::npos);

  const std::string plug = ReadRepoText("NeuralAmpModeler/NeuralAmpModeler.cpp");
  const std::string open = Between(plug, "void NeuralAmpModeler::_VolumOpenAudioPreferences()", "\n}");
  CHECK(open.find("#if defined(APP_API)") != std::string::npos);
  // Posted: a modal dialog opened inside the click would run under the mouse handler.
  CHECK(open.find("PostMessage(gHWND, WM_COMMAND, ID_PREFERENCES, 0);") != std::string::npos);

  const std::string about = Between(controls, "class AboutControl : public IContainerBase", "void SetUpdateInfo(");
  CHECK(about.find("\"Read the manual\", VOLUM_MANUAL_URL") != std::string::npos);
  CHECK(ReadRepoText("NeuralAmpModeler/config.h").find("#define VOLUM_MANUAL_URL \"https://") != std::string::npos);
  CHECK(ReadRepoText("iPlug2/IPlug/APP/IPlugAPP_dialog.cpp").find("kVoLumManualURL = VOLUM_MANUAL_URL;")
        != std::string::npos);
}
