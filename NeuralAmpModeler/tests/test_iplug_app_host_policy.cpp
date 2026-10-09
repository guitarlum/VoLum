#include "third_party/doctest.h"

#include "VoLumIPlugAPPHostPolicy.h"

#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

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
  const auto first = text.find(from);
  REQUIRE(first != std::string::npos);
  const auto last = text.find(to, first + from.size());
  REQUIRE(last != std::string::npos);
  return text.substr(first, last - first);
}
} // namespace

TEST_CASE("F-02 WinMM MIDI identity survives port renumbering")
{
  const auto unique = iplug::VoLumStableMidiPortNames({"Other 0", "VoLum Loop 1"}, true);
  CHECK(unique == std::vector<std::string>{"Other", "VoLum Loop"});
  CHECK(iplug::VoLumResolveMidiPort("VoLum Loop 0", unique, true) == 1);

  const auto duplicates = iplug::VoLumStableMidiPortNames(
    {"New device 0", "Controller 1", "Controller 2"}, true);
  CHECK(duplicates == std::vector<std::string>{"New device", "Controller [1]", "Controller [2]"});
  CHECK(iplug::VoLumResolveMidiPort("Controller [2]", duplicates, true) == 2);
  CHECK(iplug::VoLumResolveMidiPort("Controller 1", duplicates, true) == -1);
  CHECK(iplug::VoLumResolveMidiPort("Missing 7", unique, true) == -1);

  const std::string host = ReadRepoText("iPlug2/IPlug/APP/IPlugAPP_host.cpp");
  const std::string probe = Between(host, "void IPlugAPPHost::ProbeMidiIO()", "bool IPlugAPPHost::AudioSettingsInStateAreEqual");
  CHECK(probe.find("mMidiInputDevNames.clear()") != std::string::npos);
  CHECK(probe.find("VoLumStableMidiPortNames(rawInputNames, true)") != std::string::npos);
  const std::string select = Between(host, "bool IPlugAPPHost::SelectMIDIDevice", "void IPlugAPPHost::CloseAudio");
  CHECK(select.find("if(port == -1)") != std::string::npos);
  CHECK(select.find("mState.mMidiInDev.Set(OFF_TEXT)") == std::string::npos);

  const std::string dialog = ReadRepoText("iPlug2/IPlug/APP/IPlugAPP_dialog.cpp");
  const auto init = Between(dialog, "case WM_INITDIALOG:", "case WM_DESTROY:");
  CHECK(init.find("_this->ProbeMidiIO();") < init.find("_this->PopulatePreferencesDialog(hwndDlg);"));
}

TEST_CASE("F-03 dialog Cancel restores MIDI without restarting unchanged audio")
{
  CHECK_FALSE(iplug::VoLumDialogNeedsAudioRestart(true));
  CHECK(iplug::VoLumDialogNeedsAudioRestart(false));

  const std::string dialog = ReadRepoText("iPlug2/IPlug/APP/IPlugAPP_dialog.cpp");
  const auto ok = Between(dialog, "case IDOK:", "case IDAPPLY:");
  CHECK(ok.find("AudioSettingsInStateAreEqual") != std::string::npos);
  CHECK(ok.find("mActiveState != mState") == std::string::npos);

  const auto cancel = Between(dialog, "case IDCANCEL:", "case IDC_COMBO_AUDIO_DRIVER:");
  CHECK(cancel.find("const bool audioNeedsRestart") != std::string::npos);
  CHECK(cancel.find("if (audioNeedsRestart)") != std::string::npos);
  CHECK(cancel.find("if (midiChanged)") != std::string::npos);
  CHECK(cancel.find("SelectMIDIDevice(ERoute::kInput") != std::string::npos);
}

TEST_CASE("F-04 unsupported MIDI cannot fill the standalone recall queue")
{
  CHECK(iplug::VoLumStandaloneAcceptsMidiStatus(0xB0));
  CHECK(iplug::VoLumStandaloneAcceptsMidiStatus(0xCF));
  for (const uint8_t status : {uint8_t{0x80}, uint8_t{0x90}, uint8_t{0xA0}, uint8_t{0xD0},
                               uint8_t{0xE0}, uint8_t{0xF0}, uint8_t{0xF8}, uint8_t{0xFE}})
    CHECK_FALSE(iplug::VoLumStandaloneAcceptsMidiStatus(status));

  const std::string host = ReadRepoText("iPlug2/IPlug/APP/IPlugAPP_host.cpp");
  CHECK(host.find("mMidiIn->ignoreTypes(true, true, true)") != std::string::npos);
  const auto callback = Between(host, "void IPlugAPPHost::MIDICallback", "void IPlugAPPHost::ErrorCallback");
  CHECK(callback.find("VoLumStandaloneAcceptsMidiStatus") < callback.find("mMidiMsgsFromCallback.Push"));
}

TEST_CASE("F-05 automatic audio fallback is runtime-only")
{
  CHECK_FALSE(iplug::VoLumShouldPersistAudioFallback(false));
  CHECK(iplug::VoLumShouldPersistAudioFallback(true));

  const std::string host = ReadRepoText("iPlug2/IPlug/APP/IPlugAPP_host.cpp");
  const auto change = Between(host, "bool IPlugAPPHost::TryToChangeAudio", "bool IPlugAPPHost::TakeSampleRateSubstitution");
  CHECK(change.find("const AppState requestedState = mState") != std::string::npos);
  CHECK(change.find("VoLumShouldPersistAudioFallback(explicitUserChange)") != std::string::npos);
  CHECK(change.find("mSuppressAudioStatePersistence = runtimeFallback") != std::string::npos);
  CHECK(change.find("mState = requestedState") != std::string::npos);
}

TEST_CASE("F-06 shared physical output receives the stereo average")
{
  const auto distinct = iplug::VoLumRouteStereoSample(0.75, -0.25, false);
  CHECK_FALSE(distinct.shared);
  CHECK(distinct.left == 0.75);
  CHECK(distinct.right == -0.25);

  const auto shared = iplug::VoLumRouteStereoSample(0.75, -0.25, true);
  CHECK(shared.shared);
  CHECK(shared.left == 0.25);

  const std::string host = ReadRepoText("iPlug2/IPlug/APP/IPlugAPP_host.cpp");
  const auto callback = Between(host, "int IPlugAPPHost::AudioCallback", "void IPlugAPPHost::MIDICallback");
  CHECK(callback.find("VoLumRouteStereoSample") != std::string::npos);
  CHECK(callback.find("pluginOuts >= 2 && !stereo.shared") != std::string::npos);
}

TEST_CASE("F-07 granted channels and nonstandard buffer remain representable")
{
  const auto choices = iplug::VoLumBufferSizeChoices(480);
  CHECK(std::find(choices.begin(), choices.end(), 480u) != choices.end());
  CHECK(std::is_sorted(choices.begin(), choices.end()));
  CHECK(iplug::VoLumBufferSizeChoices(512).size() == 10);

  const std::string host = ReadRepoText("iPlug2/IPlug/APP/IPlugAPP_host.cpp");
  CHECK(host.find("mState.mAudioInChanL = wantInL") != std::string::npos);
  CHECK(host.find("mState.mAudioOutChanR = wantOutR") != std::string::npos);
  CHECK(host.find("mState.mBufferSize = mBufferSize") != std::string::npos);
  const std::string dialog = ReadRepoText("iPlug2/IPlug/APP/IPlugAPP_dialog.cpp");
  CHECK(dialog.find("VoLumBufferSizeChoices(mState.mBufferSize)") != std::string::npos);
  CHECK(dialog.find("CB_GETITEMDATA, iovsidx") != std::string::npos);
}
