#include "third_party/doctest.h"

#include "VoLumAppShutdown.h"

#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

// Regression coverage for the 1.2.1 report "VoLum will not start until I kill a
// dead background process". Closing the standalone window destroys the host on
// the UI thread inside WM_DESTROY; that called abortStream(), which forwards to
// RtApiAsio::stopStream() and blocks in WaitForSingleObject(condition, INFINITE)
// on a condition only the audio callback can signal. With the driver wedged the
// callback never ran again, so the window vanished and the process lived forever
// holding the audio device.
//
// The bug was not the missing timeout - a bounded fade wait was already there and
// correctly detected the dead callback. The bug was that the code then took the
// unbounded path anyway. So the assertion that matters is the negative one:
// a callback that never fades must NOT be handed to the driver's drain.

using iplug::kVoLumFadeWaitMs;
using iplug::kVoLumMaxFadeWaits;
using iplug::VoLumRunAudioTeardown;

namespace
{
struct FakeDriver
{
  int fadeAfterPolls = -1; // -1 == the callback never acknowledges the fade
  int polls = 0;
  std::vector<int> sleeps;

  bool IsCallbackFaded()
  {
    const bool faded = fadeAfterPolls >= 0 && polls >= fadeAfterPolls;
    ++polls;
    return faded;
  }

  int TotalSleptMs() const
  {
    int total = 0;
    for (const int ms : sleeps)
      total += ms;
    return total;
  }
};

iplug::VoLumAudioTeardownPlan Run(FakeDriver& driver, bool streamOpen, bool streamRunning, int maxFadeWaits = 5,
                                  int fadeWaitMs = 10)
{
  return VoLumRunAudioTeardown(
    streamOpen, streamRunning, [&driver] { return driver.IsCallbackFaded(); },
    [&driver](int ms) { driver.sleeps.push_back(ms); }, maxFadeWaits, fadeWaitMs);
}
} // namespace

TEST_CASE("A callback that never fades is never handed to the driver drain")
{
  FakeDriver driver; // fadeAfterPolls == -1: wedged driver, callback is gone

  const auto plan = Run(driver, /*streamOpen*/ true, /*streamRunning*/ true);

  // The wait is bounded...
  CHECK(plan.fadeWaits == 5);
  CHECK(driver.sleeps.size() == 5);
  CHECK(driver.TotalSleptMs() == 50);

  // ...and, the point of the fix, the drain is refused. This is the assertion
  // that fails against the pre-fix code, which always called abortStream().
  CHECK_FALSE(plan.callbackFaded);
  CHECK_FALSE(plan.drainBeforeClose);

  // Closing still happens: RtApiAsio::closeStream() stops a running stream on
  // its own and never waits on the callback, so it is the safe way out.
  CHECK(plan.closeStream);
}

TEST_CASE("A live callback still gets a clean drain")
{
  FakeDriver driver;
  driver.fadeAfterPolls = 3;

  const auto plan = Run(driver, true, true);

  CHECK(plan.fadeWaits == 3);
  CHECK(driver.TotalSleptMs() == 30);
  CHECK(plan.callbackFaded);
  CHECK(plan.drainBeforeClose);
  CHECK(plan.closeStream);
}

TEST_CASE("A callback that has already faded is not waited on at all")
{
  FakeDriver driver;
  driver.fadeAfterPolls = 0;

  const auto plan = Run(driver, true, true);

  CHECK(plan.fadeWaits == 0);
  CHECK(driver.sleeps.empty());
  CHECK(plan.drainBeforeClose);
  CHECK(plan.closeStream);
}

TEST_CASE("An open but stopped stream is closed without fading or draining")
{
  FakeDriver driver;

  const auto plan = Run(driver, /*streamOpen*/ true, /*streamRunning*/ false);

  CHECK(plan.fadeWaits == 0);
  CHECK(driver.sleeps.empty());
  CHECK(driver.polls == 0); // nothing to ask: there is no running callback
  CHECK_FALSE(plan.drainBeforeClose);
  CHECK(plan.closeStream);
}

TEST_CASE("A stream that was never opened is left entirely alone")
{
  FakeDriver driver;

  const auto plan = Run(driver, /*streamOpen*/ false, /*streamRunning*/ false);

  CHECK(plan.fadeWaits == 0);
  CHECK(driver.sleeps.empty());
  CHECK(driver.polls == 0);
  CHECK_FALSE(plan.drainBeforeClose);
  CHECK_FALSE(plan.closeStream);
}

TEST_CASE("The shipped fade budget stays in its usable window")
{
  // Too short and a healthy driver gets its drain cut off mid-fade, which is an
  // audible click on every quit. Too long and a wedged driver looks like the
  // hang this fix exists to remove.
  const int budgetMs = kVoLumMaxFadeWaits * kVoLumFadeWaitMs;
  CHECK(budgetMs == 2000);

  // The watchdog has to outlast the fade *plus* the driver calls that follow it
  // (ASIOStop, ASIODisposeBuffers, removeCurrentDriver) on a healthy driver.
  // Comparing against the fade alone let the two constants drift into a window
  // where a slow-but-working teardown gets killed.
  const int driverAllowanceMs = 1000;
  CHECK(iplug::kVoLumShutdownWatchdogMs >= budgetMs + driverAllowanceMs);
}

TEST_CASE("A watchdog kill is distinguishable from a clean exit and can be stood down")
{
  // _Exit(0) made a wedged shutdown look successful to e2e-standalone-win.ps1 and
  // to anything else that waits on the process.
  CHECK(iplug::kVoLumShutdownWatchdogExitCode != 0);

  // Disarming makes the timer a no-op, which is what keeps the untimed work after
  // the audio teardown (the loader join, the settings write) out of its reach.
  iplug::VoLumArmShutdownWatchdog(/*timeoutMs*/ 60000);
  CHECK(iplug::VoLumShutdownWatchdogArmed().load());
  iplug::VoLumDisarmShutdownWatchdog();
  CHECK_FALSE(iplug::VoLumShutdownWatchdogArmed().load());
}

TEST_CASE("The watchdog is armed around the audio teardown only")
{
  const std::string src = [] {
    const auto path = std::filesystem::path(__FILE__).parent_path().parent_path().parent_path() / "iPlug2" / "IPlug"
                      / "APP" / "IPlugAPP_host.cpp";
    std::ifstream in(path, std::ios::binary);
    REQUIRE(in.good());
    std::ostringstream ss;
    ss << in.rdbuf();
    return ss.str();
  }();

  const auto arm = src.find("VoLumArmShutdownWatchdog();");
  const auto close = src.find("CloseAudio();");
  const auto disarm = src.find("VoLumDisarmShutdownWatchdog();");
  REQUIRE(arm != std::string::npos);
  REQUIRE(close != std::string::npos);
  REQUIRE(disarm != std::string::npos);

  // The plugin destructor - which joins the model loader and writes the settings
  // file - must not run under the audio budget: five seconds is not enough for a
  // capture load that is halfway through, and killing it there would be a kill on a
  // healthy quit. So the disarm has to happen before the plugin is released.
  CHECK(arm < close);
  CHECK(close < disarm);

  // The MIDI ports have to be closed AND released inside the armed window. mMidiIn
  // and mMidiOut are unique_ptrs declared below mDAC, so their backends - and any
  // port still open - are torn down after this body returns, which is outside the
  // watchdog. cancelCallback on its own does not close the port, so a wedged MIDI
  // input could still leave the windowless process the watchdog was added to kill.
  const auto closeIn = src.find("mMidiIn->closePort();");
  const auto releaseIn = src.find("mMidiIn = nullptr;");
  const auto releaseOut = src.find("mMidiOut = nullptr;");
  REQUIRE(closeIn != std::string::npos);
  REQUIRE(releaseIn != std::string::npos);
  REQUIRE(releaseOut != std::string::npos);
  CHECK(closeIn < disarm);
  CHECK(releaseIn < disarm);
  CHECK(releaseOut < disarm);
  CHECK(arm < closeIn);
}

TEST_CASE("Plugin teardown is bounded too, on a budget of its own")
{
  // Unbounded plugin teardown produces the same end state as a wedged driver: a
  // VoLum process with no window, still holding the audio device, which the next
  // launch cannot get past. It just takes a different route there.
  CHECK(iplug::kVoLumPluginTeardownWatchdogMs > iplug::kVoLumShutdownWatchdogMs);

  // Loading a large capture takes seconds, and the join waits for it. A budget that
  // could expire during one would turn a healthy quit into a kill.
  CHECK(iplug::kVoLumPluginTeardownWatchdogMs >= 15000);

  const std::string src = [] {
    const auto path = std::filesystem::path(__FILE__).parent_path().parent_path().parent_path() / "iPlug2" / "IPlug"
                      / "APP" / "IPlugAPP_host.cpp";
    std::ifstream in(path, std::ios::binary);
    REQUIRE(in.good());
    std::ostringstream ss;
    ss << in.rdbuf();
    return ss.str();
  }();

  // mIPlug is declared before mDAC, so member destruction would release it after the
  // destructor body has returned - outside any watchdog. It has to be released
  // explicitly, inside the second armed window.
  const auto armPlugin = src.find("VoLumArmShutdownWatchdog(kVoLumPluginTeardownWatchdogMs);");
  const auto releasePlugin = src.find("mIPlug = nullptr;");
  REQUIRE(armPlugin != std::string::npos);
  REQUIRE(releasePlugin != std::string::npos);
  CHECK(armPlugin < releasePlugin);

  const auto disarmAfterPlugin = src.find("VoLumDisarmShutdownWatchdog();", releasePlugin);
  REQUIRE(disarmAfterPlugin != std::string::npos);
}

// ---------------------------------------------------------------------------
// Startup: a saved MIDI port that cannot be opened
//
// This one cannot be tested through the policy, because there is no policy to
// extract: the fix is that a throwing RtMidi call is caught at all. Opening a MIDI
// port throws RtMidiError when the device is listed but held by another
// application, and IPlugAPPHost::Init() calls SelectMIDIDevice before the window
// exists, with only WinMain's handler above it - so the process exited with no
// window, no message, and the same port name still in settings.ini, failing
// identically on every launch. A source guard is what is available here, and it
// does fail if the unguarded call comes back.
// ---------------------------------------------------------------------------

namespace
{
std::string ReadForkAppHostSource()
{
  const auto path = std::filesystem::path(__FILE__).parent_path().parent_path().parent_path() / "iPlug2" / "IPlug"
                    / "APP" / "IPlugAPP_host.cpp";
  std::ifstream in(path, std::ios::binary);
  REQUIRE(in.good());
  std::ostringstream ss;
  ss << in.rdbuf();
  return ss.str();
}

std::string ReadForkAppDialogSource()
{
  const auto path = std::filesystem::path(__FILE__).parent_path().parent_path().parent_path() / "iPlug2" / "IPlug"
                    / "APP" / "IPlugAPP_dialog.cpp";
  std::ifstream in(path, std::ios::binary);
  REQUIRE(in.good());
  std::ostringstream ss;
  ss << in.rdbuf();
  return ss.str();
}

std::size_t CountOccurrences(const std::string& haystack, const std::string& needle)
{
  std::size_t count = 0;
  for (std::size_t pos = haystack.find(needle); pos != std::string::npos;
       pos = haystack.find(needle, pos + needle.size()))
    ++count;
  return count;
}
} // namespace

TEST_CASE("Every MIDI port open in the app host is guarded against throwing")
{
  const std::string src = ReadForkAppHostSource();

  // The fallback exists and turns the failing direction off for this session.
  CHECK(src.find("auto openOrTurnOff") != std::string::npos);
  CHECK(src.find("mState.mMidiInDev.Set(OFF_TEXT)") != std::string::npos);
  CHECK(src.find("mState.mMidiOutDev.Set(OFF_TEXT)") != std::string::npos);

  // And does NOT write that to settings.ini. The usual cause is transient - a DAW
  // still quitting, a vendor control panel, a second VoLum - and persisting "off"
  // on the first failure turned a few seconds of contention into permanent loss of
  // the user's MIDI configuration, with nothing on screen to explain it. The
  // catch block must contain no UpdateINI() call.
  const auto catchStart = src.find("catch (RtMidiError& e)");
  REQUIRE(catchStart != std::string::npos);
  const auto catchEnd = src.find("};", catchStart);
  REQUIRE(catchEnd != std::string::npos);
  CHECK(src.find("UpdateINI()", catchStart) > catchEnd);

  // The forms that shipped unguarded are now all inside the guard, on both
  // platforms' branches. Written as the guarded spelling because the bare call is
  // a substring of the guarded one, so a plain "must not appear" check would be
  // unsatisfiable rather than protective.
  CHECK(src.find("openOrTurnOff([&] { mMidiIn->openPort(port-1); })") != std::string::npos);
  CHECK(src.find("openOrTurnOff([&] { mMidiIn->openPort(port-2); })") != std::string::npos);
  CHECK(src.find("openOrTurnOff([&] { mMidiOut->openPort(port-1); })") != std::string::npos);
  CHECK(src.find("openOrTurnOff([&] { mMidiOut->openPort(port-2); })") != std::string::npos);
  CHECK(src.find("openOrTurnOff([&] { mMidiIn->openVirtualPort(virtualMidiInputName); })") != std::string::npos);
  CHECK(src.find("openOrTurnOff([&] { mMidiOut->openVirtualPort(virtualMidiOutputName); })") != std::string::npos);

  // And nothing opens a port outside a guard: this is the check that catches a
  // *new* unguarded call site, which the pins above cannot see. Six opens, six
  // guards - both counts move together or this fails.
  const std::size_t opens = CountOccurrences(src, "->openPort(") + CountOccurrences(src, "->openVirtualPort(");
  CHECK(opens == 6);
  CHECK(CountOccurrences(src, "openOrTurnOff([&] {") == 6);
}

// ---------------------------------------------------------------------------
// A saved ASIO pair naming two different drivers
//
// Only one ASIO driver can serve both directions, and TryToChangeAudio has always
// resolved the input from mAudioOutDev - but the shipped 1.2.1 configuration was
// exactly such a pair ("indev=ASIO4ALL v2", "outdev=FlexASIO"), and Preferences
// trusted it: it showed the output driver in the input combo while probing input
// channels and sample rates from the stale one. The fix belongs in the dialog
// (below), not in the stored state. These paths need a live driver to exercise, so
// source guards are what is available.
// ---------------------------------------------------------------------------

TEST_CASE("The ASIO input resolution never rewrites the saved input device")
{
  const std::string src = ReadForkAppHostSource();

  // mAudioInDev holds the DirectSound/CoreAudio input choice too. Copying the ASIO
  // driver name over it - which is what resolving input from mAudioOutDev produces
  // - destroyed that choice the moment the user switched back, because the ASIO
  // name is not in the DirectSound device list and the existing fallback then reset
  // it to the OS default. The dialog resolves by output device id instead, so
  // nothing needs this writeback.
  CHECK(src.find("mState.mAudioInDev.Set(openedInputName") == std::string::npos);
  CHECK(src.find("correctedMismatchedAsioPair") == std::string::npos);

  // The one INI write left in TryToChangeAudio is the pre-existing
  // device-disappeared reset, which genuinely has new state to record.
  const std::string signature = "bool IPlugAPPHost::TryToChangeAudio()";
  const auto tryToChange = src.find(signature);
  REQUIRE(tryToChange != std::string::npos);
  // The next member definition, whichever one it is - anchoring on a particular
  // neighbour made this count whatever was inserted between them.
  const auto nextMember = src.find("IPlugAPPHost::", tryToChange + signature.size());
  REQUIRE(nextMember != std::string::npos);
  std::size_t iniWrites = 0;
  for (auto pos = src.find("UpdateINI()", tryToChange); pos != std::string::npos && pos < nextMember;
       pos = src.find("UpdateINI()", pos + 1))
    ++iniWrites;
  CHECK(iniWrites == 1);
}

TEST_CASE("A driver-renegotiated buffer size is adopted only when the UI can represent it")
{
  const std::string src = ReadForkAppHostSource();

  const auto writeback = src.find("mState.mBufferSize = mBufferSize;");
  REQUIRE(writeback != std::string::npos);

  // Guarded on an exact round-trip through the dialog's size list, so a driver that
  // negotiates e.g. 480 leaves the stored request alone rather than having it
  // rounded up to a size nothing ever offered.
  CHECK(src.find("if (mBufferSize != iovs && NormalizeAPPBufferSize(mBufferSize) == mBufferSize)") < writeback);

  // And it lands before the active-state snapshot, or mActiveState would keep
  // restoring the refused request on the next failure rollback.
  CHECK(writeback < src.find("mActiveState = mState;"));
}

TEST_CASE("Preferences describes the ASIO driver it will actually open, and probes it once")
{
  const std::string src = ReadForkAppDialogSource();

  // Input selection is resolved by device id, not by reusing the output list index.
  // The two lists are filtered independently and need not agree on order or length,
  // so under ASIO - one driver serving both directions - the shared position could
  // name a third device, or nothing at all when the input list is shorter.
  CHECK(src.find("if (mAudioInputDevs[i] == mAudioOutputDevs[outdevidx])") != std::string::npos);
  CHECK(src.find("if (driverType == kDeviceASIO && mAudioOutputDevs.size())") != std::string::npos);

  // The input channel list describes the input device, never the output list's
  // position applied to the input list - that is the mismatch this fixes, and it is
  // wrong under every driver type, not just ASIO.
  CHECK(src.find("mDAC->getDeviceInfo(mAudioInputDevs[indevidx])") != std::string::npos);
  CHECK(src.find("mAudioInputDevs[outdevidx]") == std::string::npos);

  // One probe per device: when both directions name the same device the second
  // getDeviceInfo is the double-init this fork blames for heap corruption.
  CHECK(src.find("inputDevInfo = outputDevInfo;") != std::string::npos);
  CHECK(CountOccurrences(src, "mDAC->getDeviceInfo(") == 2);

  // And neither probe dereferences a null device handle, which is reachable when an
  // ASIO driver fails to load and mDAC is left null.
  CHECK(src.find("if (mDAC && mAudioOutputDevs.size())") != std::string::npos);
  CHECK(src.find("if (mDAC && mAudioInputDevs.size())") != std::string::npos);
}

TEST_CASE("The first-run settings file is named with a separator POSIX understands")
{
  const std::string src = ReadForkAppHostSource();

  // Upstream appended "\\settings.ini" in the OS_MAC branch of InitState - the branch
  // taken only when the settings directory does not exist yet, i.e. first run on a
  // clean mac. On POSIX a backslash is an ordinary filename character, not a
  // separator, so that produced a file literally named "\settings.ini" inside the
  // new directory. The second launch found the directory, looked for "settings.ini",
  // did not find it and wrote defaults - so the first session's audio device, buffer
  // size, sample rate and MIDI selection were lost, exactly once, on every new mac.
  //
  // Pinned by source because InitState resolves a real HOME, creates a real
  // directory and is platform-gated, so the mac branch cannot be reached from a
  // Windows-hosted unit test at all. The check is the negative one: no append of a
  // backslash-prefixed filename anywhere in the file.
  CHECK(src.find("Append(\"\\\\settings.ini\")") == std::string::npos);

  // All three appends - the shared existing-directory path plus one per platform's
  // create-directory branch - name the file the same way.
  CHECK(CountOccurrences(src, "Append(\"settings.ini\")") == 3);
}

// ---------------------------------------------------------------------------
// The sample rate the app reports, and the one the driver is actually running
//
// A 1.2.1 user with an RME Babyface Pro FS: "ich kann die Sampling-Rate meines
// Interfaces nicht umstellen, obwohl die Anzeige das behauptet" - the display
// claimed the change had happened while the interface stayed where it was. Changing
// it in RME's own panel instead stopped the audio for good, and quitting then left a
// process behind that had to be ended by hand before VoLum would start again.
//
// Every one of those paths needs a live ASIO driver, so source guards are what is
// available. The end-to-end coverage for the stored-rate half is the
// "unsupported sample rate in settings.ini" scenario in e2e-standalone-win.ps1.
// ---------------------------------------------------------------------------

namespace
{
std::string ReadForkRtAudioSource()
{
  const auto path = std::filesystem::path(__FILE__).parent_path().parent_path().parent_path() / "iPlug2"
                    / "Dependencies" / "IPlug" / "RTAudio" / "RtAudio.cpp";
  std::ifstream in(path, std::ios::binary);
  REQUIRE(in.good());
  std::ostringstream ss;
  ss << in.rdbuf();
  return ss.str();
}

std::string ReadForkAppMainSource()
{
  const auto path = std::filesystem::path(__FILE__).parent_path().parent_path().parent_path() / "iPlug2" / "IPlug"
                    / "APP" / "IPlugAPP_main.cpp";
  std::ifstream in(path, std::ios::binary);
  REQUIRE(in.good());
  std::ostringstream ss;
  ss << in.rdbuf();
  return ss.str();
}
} // namespace

TEST_CASE("RtAudio reports the rate the ASIO driver took, not the one it was asked for")
{
  const std::string src = ReadForkRtAudioSource();

  // ASIOSetSampleRate can return ASE_OK from a driver that then stays where it was -
  // most often one clocked from its own control panel or an external source. Without
  // the read-back every layer above believed the request.
  const auto setRate = src.find("result = ASIOSetSampleRate( (ASIOSampleRate) sampleRate );");
  const auto readBack = src.find("if ( ASIOGetSampleRate( &actualRate ) == ASE_OK && actualRate > 0.0 )");
  REQUIRE(setRate != std::string::npos);
  REQUIRE(readBack != std::string::npos);
  CHECK(setRate < readBack);

  // And the read-back has to land before stream_.sampleRate is written, or
  // getStreamSampleRate() keeps echoing the request.
  const auto store = src.find("stream_.sampleRate = sampleRate;", readBack);
  REQUIRE(store != std::string::npos);
  CHECK(readBack < store);
}

TEST_CASE("A driver that changes the rate on its own tells the host, by both routes")
{
  const std::string src = ReadForkRtAudioSource();

  // sampleRateChanged: RtAudio stops the stream here. Before this, nothing else
  // happened, so the stream stayed stopped and the user heard nothing more until the
  // application was restarted.
  // rfind: the signature appears twice, as a forward declaration and as the
  // definition. The declaration is first.
  const auto callback = src.rfind("static void sampleRateChanged( ASIOSampleRate sRate )");
  REQUIRE(callback != std::string::npos);
  const auto note = src.find("object->notePendingExternalSampleRate(", callback);
  const auto stop = src.find("object->stopStream();", callback);
  REQUIRE(note != std::string::npos);
  REQUIRE(stop != std::string::npos);
  CHECK(note < stop); // recorded before the stream goes away

  // kAsioResetRequest: the route several drivers take instead, RME's among them. The
  // comment there always said to defer the reset and do it later; nothing ever did.
  const auto reset = src.find("case kAsioResetRequest:");
  REQUIRE(reset != std::string::npos);
  const auto resetEnd = src.find("case kAsioResyncRequest:", reset);
  REQUIRE(resetEnd != std::string::npos);
  const auto resetNote = src.find("object->notePendingDeviceReset();", reset);
  CHECK(resetNote != std::string::npos);
  CHECK(resetNote < resetEnd);
}

TEST_CASE("The app adopts the driver's rate and follows it when it moves")
{
  const std::string src = ReadForkAppHostSource();

  // Adopted at open time, the same way the renegotiated buffer size is.
  const auto open = src.find("mDAC->openStream(");
  const auto adopt = src.find("const unsigned int actualSR = mDAC->getStreamSampleRate();", open);
  const auto start = src.find("mDAC->startStream();", open);
  REQUIRE(open != std::string::npos);
  REQUIRE(adopt != std::string::npos);
  REQUIRE(start != std::string::npos);
  CHECK(open < adopt);

  // Before startStream, so the plugin is reconfigured with no callback running.
  CHECK(adopt < start);
  CHECK(src.find("mIPlug->SetSampleRate(mSampleRate);", adopt) < start);

  // Followed afterwards, from the window's timer rather than the driver's callback
  // thread: reopening a stream from inside the driver's own callback is how a
  // deadlock at quit gets written.
  CHECK(src.find("void IPlugAPPHost::PollAudioStatus()") != std::string::npos);
  CHECK(src.find("mDAC->takePendingDeviceReset();") != std::string::npos);
  CHECK(src.find("mDAC->takePendingExternalSampleRate();") != std::string::npos);

  // A driver that asks to be reopened after every open must not spin the app.
  CHECK(src.find("mFollowDriverChanges = false;") != std::string::npos);

  // A stored rate the device does not offer is corrected instead of failing to open.
  const auto initAudio = src.find("bool IPlugAPPHost::InitAudio(");
  REQUIRE(initAudio != std::string::npos);
  CHECK(src.find("NearestSupportedSampleRate(sr, (int) inId, (int) outId)", initAudio) != std::string::npos);

  // And a failure that happens before the window exists is held until it can be seen.
  // MessageBox(NULL, ...) at startup is a report nobody receives.
  CHECK(src.find("mDeferredAudioError.Set(message);") != std::string::npos);
}

TEST_CASE("The poll is wired to the window and stands down while Preferences is open")
{
  const std::string src = ReadForkAppDialogSource();

  CHECK(src.find("SetTimer(hwndDlg, kAudioStatusTimerID, 500, NULL);") != std::string::npos);
  CHECK(src.find("KillTimer(hwndDlg, kAudioStatusTimerID);") != std::string::npos);

  // Reopening the stream underneath a dialog the user is editing would take the
  // device away mid-selection. The driver's pending rate keeps until it closes.
  CHECK(src.find("if (wParam == kAudioStatusTimerID && gPreferencesHWND == NULL)") != std::string::npos);

  // Apply re-reads the state, so the combo shows the rate the driver actually took
  // rather than the one that was asked for.
  const auto apply = src.find("case IDAPPLY:");
  REQUIRE(apply != std::string::npos);
  const auto applyEnd = src.find("break;", apply);
  CHECK(src.find("_this->PopulateAudioDialogs(hwndDlg);", apply) < applyEnd);
}

TEST_CASE("A relaunch blocked by a windowless previous instance says so")
{
  const std::string src = ReadForkAppMainSource();

  // Upstream: FindWindow returns NULL, SetForegroundWindow(NULL) does nothing, and
  // the process exits 0. Double-clicking VoLum appeared to do nothing at all, with
  // no hint that a process had to be ended first.
  const auto guard = src.find("if (hWnd)");
  const auto raise = src.find("SetForegroundWindow(hWnd);");
  REQUIRE(guard != std::string::npos);
  REQUIRE(raise != std::string::npos);
  CHECK(guard < raise);
  CHECK(src.find("MB_OK | MB_ICONWARNING") != std::string::npos);

  // An orderly shutdown destroys the window before the process exits, so a launch
  // that lands in that gap must wait rather than accuse a healthy quit.
  CHECK(src.find("kZombieWaitMs") != std::string::npos);
  CHECK(src.find("hMutex = OpenMutex(MUTEX_ALL_ACCESS, 0, BUNDLE_NAME);") != std::string::npos);

  // A minimised instance is restored, not just raised.
  CHECK(src.find("if (IsIconic(hWnd))") != std::string::npos);
}
