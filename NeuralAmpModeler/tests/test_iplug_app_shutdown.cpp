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
  CHECK(iplug::kVoLumShutdownWatchdogMs > budgetMs);
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

  // The fallback exists and turns the failing direction off, persisting it.
  CHECK(src.find("auto openOrTurnOff") != std::string::npos);
  CHECK(src.find("mState.mMidiInDev.Set(OFF_TEXT)") != std::string::npos);
  CHECK(src.find("mState.mMidiOutDev.Set(OFF_TEXT)") != std::string::npos);

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
// resolved the input from mAudioOutDev - but nothing wrote that back, so
// settings.ini could keep claiming a pair that is never opened. The shipped 1.2.1
// configuration was exactly that ("indev=ASIO4ALL v2", "outdev=FlexASIO"), and
// Preferences trusted it: it showed the output driver in the input combo while
// probing input channels and sample rates from the stale one. These paths need a
// live driver to exercise, so source guards are what is available.
// ---------------------------------------------------------------------------

TEST_CASE("A mismatched saved ASIO pair is corrected and persisted")
{
  const std::string src = ReadForkAppHostSource();

  CHECK(src.find("correctedMismatchedAsioPair") != std::string::npos);
  // The corrected name is the device actually resolved for input, and it reaches
  // settings.ini rather than only living in memory for this session.
  CHECK(src.find("mState.mAudioInDev.Set(openedInputName.c_str())") != std::string::npos);
  CHECK(src.find("else if (correctedMismatchedAsioPair)") != std::string::npos);
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
  CHECK(src.find("if (mAudioInputDevs[i] == mAudioOutputDevs[outdevidx])") != std::string::npos);
  CHECK(src.find("CB_SETCURSEL, outdevidx, 0);\n\n  RtAudio::DeviceInfo") == std::string::npos);

  // One probe per device: when both directions name the same device the second
  // getDeviceInfo is the double-init this fork blames for heap corruption.
  CHECK(src.find("inputDevInfo = outputDevInfo;") != std::string::npos);
  CHECK(CountOccurrences(src, "mDAC->getDeviceInfo(") == 2);

  // And neither probe dereferences a null device handle, which is reachable when an
  // ASIO driver fails to load and mDAC is left null.
  CHECK(src.find("if (mDAC && mAudioOutputDevs.size())") != std::string::npos);
  CHECK(src.find("if (mDAC && mAudioInputDevs.size())") != std::string::npos);
}
