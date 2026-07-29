#include "third_party/doctest.h"

#include "VoLumAppShutdown.h"

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
