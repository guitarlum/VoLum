#include "third_party/doctest.h"

#include "VoLumAppLinuxPolicy.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

// The Linux standalone loads JACK and PulseAudio at runtime. A machine without
// libjack must lose the JACK entry and nothing else, and the Preferences combo
// then no longer numbers its entries by driver type.

using iplug::kVoLumLinuxDriverAlsa;
using iplug::kVoLumLinuxDriverJack;
using iplug::kVoLumLinuxDriverPulse;
using iplug::VoLumLinuxQuitStep;

namespace
{
std::string ReadRepoText(const std::filesystem::path& relative)
{
  const auto root = std::filesystem::path(__FILE__).parent_path().parent_path().parent_path();
  std::ifstream in(root / relative, std::ios::binary);
  REQUIRE(in.good());
  std::ostringstream ss;
  ss << in.rdbuf();
  std::string text = ss.str();
  text.erase(std::remove(text.begin(), text.end(), '\r'), text.end());
  return text;
}

std::string Labels(const std::vector<iplug::VoLumLinuxDriverChoice>& choices)
{
  std::string out;
  for (const auto& c : choices)
    out += (out.empty() ? "" : ",") + std::string(c.label);
  return out;
}
} // namespace

TEST_CASE("Linux driver list drops only the driver whose library is missing")
{
  CHECK(Labels(iplug::VoLumLinuxDriverChoices(true, true)) == "ALSA,JACK,PulseAudio");
  CHECK(Labels(iplug::VoLumLinuxDriverChoices(false, true)) == "ALSA,PulseAudio");
  CHECK(Labels(iplug::VoLumLinuxDriverChoices(true, false)) == "ALSA,JACK");
  CHECK(Labels(iplug::VoLumLinuxDriverChoices(false, false)) == "ALSA");
}

TEST_CASE("Linux driver combo positions follow the offered entries, not the driver numbers")
{
  const auto noJack = iplug::VoLumLinuxDriverChoices(false, true);
  CHECK(iplug::VoLumLinuxDriverComboIndex(noJack, kVoLumLinuxDriverAlsa) == 0);
  CHECK(iplug::VoLumLinuxDriverComboIndex(noJack, kVoLumLinuxDriverPulse) == 1);
  CHECK(iplug::VoLumLinuxDriverComboIndex(noJack, kVoLumLinuxDriverJack) == -1);
  CHECK(noJack[1].driverType == kVoLumLinuxDriverPulse);

  // settings.ini stores these numbers; they match IPlugAPP_host.h kDeviceAlsa/Jack/Pulse.
  CHECK(kVoLumLinuxDriverAlsa == 0);
  CHECK(kVoLumLinuxDriverJack == 1);
  CHECK(kVoLumLinuxDriverPulse == 2);
}

TEST_CASE("Linux startup falls back to ALSA when the stored driver's library is missing")
{
  CHECK(iplug::VoLumLinuxStartupDriver(kVoLumLinuxDriverJack, true, true) == kVoLumLinuxDriverJack);
  CHECK(iplug::VoLumLinuxStartupDriver(kVoLumLinuxDriverJack, false, true) == kVoLumLinuxDriverAlsa);
  CHECK(iplug::VoLumLinuxStartupDriver(kVoLumLinuxDriverPulse, true, false) == kVoLumLinuxDriverAlsa);
  CHECK(iplug::VoLumLinuxStartupDriver(kVoLumLinuxDriverPulse, false, true) == kVoLumLinuxDriverPulse);
  CHECK(iplug::VoLumLinuxStartupDriver(7, true, true) == kVoLumLinuxDriverAlsa);
}

TEST_CASE("Linux settings.ini folder is the library folder and ignores a relative XDG_DATA_HOME")
{
  CHECK(iplug::VoLumLinuxAppDataDir("/data", "/home/u", "VoLum") == "/data/VoLum/");
  CHECK(iplug::VoLumLinuxAppDataDir(nullptr, "/home/u", "VoLum") == "/home/u/.local/share/VoLum/");
  CHECK(iplug::VoLumLinuxAppDataDir("", "/home/u", "VoLum") == "/home/u/.local/share/VoLum/");
  CHECK(iplug::VoLumLinuxAppDataDir("relative/dir", "/home/u", "VoLum") == "/home/u/.local/share/VoLum/");
  CHECK(iplug::VoLumLinuxAppDataDir(nullptr, nullptr, "VoLum").empty());
}

TEST_CASE("Linux quit signals cancel a modal first, then close, and a second one exits")
{
  CHECK(iplug::VoLumLinuxQuitStepFor(0, false) == VoLumLinuxQuitStep::None);
  CHECK(iplug::VoLumLinuxQuitStepFor(0, true) == VoLumLinuxQuitStep::None);
  CHECK(iplug::VoLumLinuxQuitStepFor(1, true) == VoLumLinuxQuitStep::CancelModal);
  CHECK(iplug::VoLumLinuxQuitStepFor(1, false) == VoLumLinuxQuitStep::CloseMainWindow);
  CHECK(iplug::VoLumLinuxQuitStepFor(2, false) == VoLumLinuxQuitStep::ExitNow);
  CHECK(iplug::VoLumLinuxQuitStepFor(2, true) == VoLumLinuxQuitStep::ExitNow);
}

TEST_CASE("Linux Preferences offers the loadable drivers and maps the selection back to a driver type")
{
  const std::string dialog = ReadRepoText("iPlug2/IPlug/APP/IPlugAPP_dialog.cpp");
  const auto section = dialog.find("#elif defined OS_LINUX\nvoid IPlugAPPHost::PopulatePreferencesDialog");
  REQUIRE(section != std::string::npos);
  const std::string populate = dialog.substr(section, dialog.find("\n}", section) - section);
  CHECK(populate.find("IPlugAPPLinuxDriverChoices()") != std::string::npos);
  CHECK(populate.find("CB_SETITEMDATA") != std::string::npos);
  CHECK(populate.find("(LPARAM)\"JACK\"") == std::string::npos);

  const auto change = dialog.find("case IDC_COMBO_AUDIO_DRIVER:");
  REQUIRE(change != std::string::npos);
  const std::string handler = dialog.substr(change, dialog.find("case IDC_COMBO_AUDIO_IN_DEV:", change) - change);
  const std::string byItemData =
    "#if defined OS_LINUX\n"
    "            v = (int) SendDlgItemMessage(hwndDlg, IDC_COMBO_AUDIO_DRIVER, CB_GETITEMDATA, v, 0);";
  CHECK(handler.find(byItemData) != std::string::npos);
}
