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

  const auto duplicates = iplug::VoLumStableMidiPortNames({"New device 0", "Controller 1", "Controller 2"}, true);
  CHECK(duplicates == std::vector<std::string>{"New device", "Controller [1]", "Controller [2]"});
  CHECK(iplug::VoLumResolveMidiPort("Controller [2]", duplicates, true, true) == 2);
  CHECK(iplug::VoLumResolveMidiPort("Controller 1", duplicates, true) == -1);
  CHECK(iplug::VoLumResolveMidiPort("Missing 7", unique, true) == -1);

  // A Preferences combo choice is already a stable name, even while the session
  // still has an unresolved legacy setting. Both duplicate ordinals and real
  // device names ending in a digit must therefore be looked up as stable.
  CHECK(iplug::VoLumResolveMidiPort("Controller [2]", duplicates, true, false) == -1);
  CHECK(iplug::VoLumResolveMidiPort("Controller [2]", duplicates, true, true) == 2);
  const std::vector<std::string> numericNames{"Komplete Audio", "Komplete Audio 6"};
  CHECK(iplug::VoLumResolveMidiPort("Komplete Audio 6", numericNames, true, false) == 0);
  CHECK(iplug::VoLumResolveMidiPort("Komplete Audio 6", numericNames, true, true) == 1);

  const std::vector<std::string> storedStable{"Other", "Controller [1]", "Controller [2]", "Komplete Audio 6"};
  const std::vector<std::string> storedLegacy{"Other 0", "Controller 1", "Controller 2", "Komplete Audio 6 3"};
  CHECK(iplug::VoLumLegacyMidiNameForStable("Komplete Audio 6", storedStable, storedLegacy, "off")
        == "Komplete Audio 6 3");
  CHECK(iplug::VoLumLegacyMidiNameForStable("Controller [2]", storedStable, storedLegacy, "off") == "off");

  const auto firstUpgrade =
    iplug::VoLumReconcileStoredMidiName("Komplete Audio 6 3", "", storedStable, storedLegacy, true);
  CHECK(firstUpgrade.selectedName == "Komplete Audio 6 3");
  CHECK_FALSE(firstUpgrade.nameIsStable);

  const auto normalUpgrade =
    iplug::VoLumReconcileStoredMidiName("Komplete Audio 6 3", "Komplete Audio 6", storedStable, storedLegacy, true);
  CHECK(normalUpgrade.selectedName == "Komplete Audio 6");
  CHECK(normalUpgrade.nameIsStable);
  CHECK_FALSE(normalUpgrade.changed);

  // A 1.2.x downgrade re-picked another port and left indev2 untouched.
  const auto downgradeRepick =
    iplug::VoLumReconcileStoredMidiName("Komplete Audio 6 3", "Other", storedStable, storedLegacy, true);
  CHECK(downgradeRepick.selectedName == "Komplete Audio 6");
  CHECK(downgradeRepick.nameIsStable);
  CHECK(downgradeRepick.changed);

  // Duplicate stable names are hidden from 1.2.x as off. If 1.2.x later
  // picks one, its exact raw spelling safely replaces the stale stable key.
  const auto collisionRepick =
    iplug::VoLumReconcileStoredMidiName("Controller 2", "Controller [1]", storedStable, storedLegacy, true);
  CHECK(collisionRepick.selectedName == "Controller [2]");
  CHECK(collisionRepick.nameIsStable);
  CHECK(collisionRepick.changed);

  const std::string host = ReadRepoText("iPlug2/IPlug/APP/IPlugAPP_host.cpp");
  const std::string probe =
    Between(host, "void IPlugAPPHost::ProbeMidiIO()", "bool IPlugAPPHost::AudioSettingsInStateAreEqual");
  CHECK(probe.find("mMidiInputDevNames.clear()") != std::string::npos);
  CHECK(probe.find("VoLumStableMidiPortNames(rawInputNames, true)") != std::string::npos);
  const std::string select = Between(host, "bool IPlugAPPHost::SelectMIDIDevice", "void IPlugAPPHost::CloseAudio");
  CHECK(select.find("if(port == -1)") != std::string::npos);
  CHECK(select.find("mState.mMidiInDev.Set(OFF_TEXT)") == std::string::npos);
  CHECK(host.find("\"indev2\"") != std::string::npos);
  CHECK(host.find("\"outdev2\"") != std::string::npos);
  CHECK(host.find("WritePrivateProfileString(\"midi\", \"namever\", NULL") != std::string::npos);

  const std::string dialog = ReadRepoText("iPlug2/IPlug/APP/IPlugAPP_dialog.cpp");
  const auto init = Between(dialog, "case WM_INITDIALOG:", "case WM_DESTROY:");
  CHECK(init.find("_this->ProbeMidiIO();") < init.find("_this->PopulatePreferencesDialog(hwndDlg);"));
  CHECK(dialog.find("SelectMIDIDevice(ERoute::kInput, mState.mMidiInDev.Get(), true)") != std::string::npos);
  CHECK(dialog.find("SelectMIDIDevice(ERoute::kOutput, mState.mMidiOutDev.Get(), true)") != std::string::npos);
  const auto inputPick = Between(dialog, "case IDC_COMBO_MIDI_IN_DEV:", "case IDC_COMBO_MIDI_OUT_DEV:");
  REQUIRE(inputPick.find("mState.mMidiInDevNameIsStable = true") != std::string::npos);
  CHECK(inputPick.find("getComboString(mState.mMidiInDev") < inputPick.find("mState.mMidiInDevNameIsStable = true"));
  CHECK(inputPick.find("mState.mMidiInDevNameIsStable = true") < inputPick.find("SelectMIDIDevice(ERoute::kInput"));
  const auto outputPick = Between(dialog, "case IDC_COMBO_MIDI_OUT_DEV:", "case IDC_COMBO_MIDI_IN_CHAN:");
  REQUIRE(outputPick.find("mState.mMidiOutDevNameIsStable = true") != std::string::npos);
  CHECK(outputPick.find("getComboString(mState.mMidiOutDev")
        < outputPick.find("mState.mMidiOutDevNameIsStable = true"));
  CHECK(outputPick.find("mState.mMidiOutDevNameIsStable = true") < outputPick.find("SelectMIDIDevice(ERoute::kOutput"));
}

TEST_CASE("F-03 dialog Cancel restores MIDI without restarting unchanged audio")
{
  const auto unchanged = iplug::VoLumPlanDialogAudio(true, true, true, false);
  CHECK_FALSE(unchanged.restartOnOK);
  CHECK_FALSE(unchanged.restartOnCancel);

  const auto changed = iplug::VoLumPlanDialogAudio(false, false, true, false);
  CHECK(changed.restartOnOK);
  CHECK(changed.restartOnCancel);

  // Start at 512, Apply 256, then choose 512 again. Both OK and Cancel must
  // reopen 512 because the active stream is still the applied 256 state.
  const auto revertedAfterApply = iplug::VoLumPlanDialogAudio(false, true, false, true);
  CHECK(revertedAfterApply.restartOnOK);
  CHECK(revertedAfterApply.restartOnCancel);

  const std::string dialog = ReadRepoText("iPlug2/IPlug/APP/IPlugAPP_dialog.cpp");
  const auto ok = Between(dialog, "case IDOK:", "case IDAPPLY:");
  CHECK(ok.find("AudioSettingsInStateAreEqual") != std::string::npos);
  CHECK(ok.find("mActiveState != mState") == std::string::npos);

  const auto cancel = Between(dialog, "case IDCANCEL:", "case IDC_COMBO_AUDIO_DRIVER:");
  CHECK(cancel.find("VoLumPlanDialogAudio") != std::string::npos);
  CHECK(cancel.find("if (audioPlan.restartOnCancel)") != std::string::npos);
  CHECK(cancel.find("if (midiChanged)") != std::string::npos);
  CHECK(cancel.find("SelectMIDIDevice(ERoute::kInput") != std::string::npos);
  CHECK(dialog.find("gAudioAppliedInPreferences = true") != std::string::npos);
}

TEST_CASE("F-04 unsupported MIDI cannot fill the standalone recall queue")
{
  CHECK(iplug::VoLumStandaloneAcceptsMidiStatus(0xB0));
  CHECK(iplug::VoLumStandaloneAcceptsMidiStatus(0xCF));
  for (const uint8_t status : {uint8_t{0x80}, uint8_t{0x90}, uint8_t{0xA0}, uint8_t{0xD0}, uint8_t{0xE0}, uint8_t{0xF0},
                               uint8_t{0xF8}, uint8_t{0xFE}})
    CHECK_FALSE(iplug::VoLumStandaloneAcceptsMidiStatus(status));

  const std::string host = ReadRepoText("iPlug2/IPlug/APP/IPlugAPP_host.cpp");
  CHECK(host.find("mMidiIn->ignoreTypes(true, true, true)") != std::string::npos);
  const auto callback = Between(host, "void IPlugAPPHost::MIDICallback", "void IPlugAPPHost::ErrorCallback");
  CHECK(callback.find("VoLumStandaloneAcceptsMidiStatus") < callback.find("mMidiMsgsFromCallback.Push"));
}

TEST_CASE("F-05 automatic audio fallback is runtime-only")
{
  const auto runtimeFallback = iplug::VoLumPlanFailureRestore(true, false, true);
  CHECK(runtimeFallback.restoreActiveState);
  CHECK_FALSE(runtimeFallback.persistActiveState);
  CHECK(runtimeFallback.restoreSavedFallbackRequest);

  const auto normalActiveState = iplug::VoLumPlanFailureRestore(true, false, false);
  CHECK(normalActiveState.restoreActiveState);
  CHECK(normalActiveState.persistActiveState);
  CHECK_FALSE(normalActiveState.restoreSavedFallbackRequest);

  const std::string host = ReadRepoText("iPlug2/IPlug/APP/IPlugAPP_host.cpp");
  const auto change =
    Between(host, "bool IPlugAPPHost::TryToChangeAudio", "bool IPlugAPPHost::TakeSampleRateSubstitution");
  CHECK(change.find("const AppState requestedState = mState") != std::string::npos);
  CHECK(change.find("mSuppressAudioStatePersistence = runtimeFallback") != std::string::npos);
  CHECK(change.find("mState = requestedState") != std::string::npos);
  const auto opened = change.find("if (opened)");
  REQUIRE(opened != std::string::npos);
  CHECK(change.find("UpdateINI()", change.find("const bool persistFallback")) > opened);
  CHECK(host.find("mActiveAudioIsRuntimeFallback = mSuppressAudioStatePersistence") != std::string::npos);
  CHECK(host.find("mRuntimeFallbackRequestedState = requestedState") != std::string::npos);
  CHECK(host.find("restorePlan.restoreSavedFallbackRequest") != std::string::npos);
  CHECK(host.find("VoLumPlanFailureRestore") != std::string::npos);
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
  const auto failedProbe = iplug::VoLumPlanProbeChannel(7, 0);
  CHECK(failedProbe.runtimeChannel == 1);
  CHECK(failedProbe.savedChannel == 7);
  CHECK_FALSE(failedProbe.corrected);

  const auto clampedProbe = iplug::VoLumPlanProbeChannel(7, 2);
  CHECK(clampedProbe.runtimeChannel == 2);
  CHECK(clampedProbe.savedChannel == 2);
  CHECK(clampedProbe.corrected);

  const auto choices = iplug::VoLumBufferSizeChoices(480);
  CHECK(std::find(choices.begin(), choices.end(), 480u) != choices.end());
  CHECK(std::is_sorted(choices.begin(), choices.end()));
  CHECK(iplug::VoLumBufferSizeChoices(512).size() == 10);
  CHECK(iplug::VoLumClampStoredBufferSize(-1) == 48);
  CHECK(iplug::VoLumClampStoredBufferSize(480) == 480);
  CHECK(iplug::VoLumClampStoredBufferSize(99999) == 8192);

  const std::string host = ReadRepoText("iPlug2/IPlug/APP/IPlugAPP_host.cpp");
  CHECK(host.find("VoLumPlanProbeChannel") != std::string::npos);
  CHECK(host.find("VoLumClampStoredBufferSize(storedBufferSize)") != std::string::npos);
  CHECK(host.find("NormalizeAPPBufferSize") == std::string::npos);
  CHECK(host.find("mState.mBufferSize = mBufferSize") != std::string::npos);
  const std::string dialog = ReadRepoText("iPlug2/IPlug/APP/IPlugAPP_dialog.cpp");
  CHECK(dialog.find("VoLumBufferSizeChoices(mState.mBufferSize)") != std::string::npos);
  CHECK(dialog.find("CB_GETITEMDATA, iovsidx") != std::string::npos);
}

TEST_CASE("APP Preferences safely reads combo text and reopens returned MIDI")
{
  const std::string dialog = ReadRepoText("iPlug2/IPlug/APP/IPlugAPP_dialog.cpp");
  CHECK(dialog.find("std::string tempString(static_cast<std::size_t>(len), '\\0')") != std::string::npos);
  const auto ok = Between(dialog, "case IDOK:", "case IDAPPLY:");
  CHECK(ok.find("mMidiIn->isPortOpen()") != std::string::npos);
  CHECK(ok.find("SelectMIDIDevice(ERoute::kInput") != std::string::npos);
}
