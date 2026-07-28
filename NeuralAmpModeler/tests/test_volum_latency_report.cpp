#include "third_party/doctest.h"

#include <string>

#include "../VoLumLatencyReport.h"

// The Settings-page latency line. The 1.2.1 report was "shows 0 ms on the laptop but
// I hear way more than 21 ms" - both true at once, because the old readout only
// counted our own algorithmic delay. These pin what the new lines say, that they
// never double-count the driver's buffering, and that a total is only ever claimed
// when the driver actually reported one.

using namespace volum;

TEST_CASE("Zero plugin delay is still an honest round trip in the standalone")
{
  LatencyReport r;
  r.pluginSamples = 0; // 48 kHz model at 48 kHz, no pitch shifting
  r.bufferFrames = 512;
  r.driverFrames = 1024; // ASIO4ALL-shaped: two buffers' worth
  r.sampleRate = 48000.0;

  CHECK(RoundTripFrames(r) == 1024);
  CHECK(LatencyMs(RoundTripFrames(r), r.sampleRate) == doctest::Approx(21.333).epsilon(0.001));

  const LatencyLines lines = FormatStandaloneLatency(r);
  CHECK(lines.headline == "Round trip: 21.3 ms");
  CHECK(lines.detail.find("plugin 0.0") != std::string::npos);
  CHECK(lines.detail.find("driver 21.3") != std::string::npos);
  CHECK(lines.detail.find("buffer 512") != std::string::npos);
  // The old line was the whole complaint; it must not survive.
  CHECK(lines.headline.find("Current latency") == std::string::npos);
}

TEST_CASE("A driver figure is used as-is, never added to the buffer estimate")
{
  // ASIO drivers report a latency that already includes their own buffering. Adding
  // 2 x buffer on top would roughly double the number we show.
  LatencyReport r;
  r.bufferFrames = 128;
  r.driverFrames = 300;
  r.sampleRate = 48000.0;
  CHECK(DriverLatencyKnown(r));
  CHECK(RoundTripFrames(r) == 300);
}

TEST_CASE("Without a driver figure no round trip is claimed at all")
{
  // The two-buffer stand-in was not conservative, it was flattering: at 128 frames it
  // read ~5.3 ms while a hardware loopback measured 63.7 ms through WASAPI on that
  // same buffer. A confident small number is worse than admitting we do not know.
  LatencyReport r;
  r.pluginSamples = 0;
  r.bufferFrames = 128;
  r.driverFrames = 0; // DirectSound / WASAPI often report nothing
  r.sampleRate = 48000.0;

  CHECK_FALSE(DriverLatencyKnown(r));
  CHECK(RoundTripFrames(r) == 0);

  const LatencyLines lines = FormatStandaloneLatency(r);
  CHECK(lines.headline == "Plugin latency: 0.0 ms (0 samples)");
  CHECK(lines.headline.find("Round trip") == std::string::npos);
  // No total, estimated or otherwise, and no "~5.3 ms" style guess anywhere.
  CHECK(lines.detail.find("Round trip") == std::string::npos);
  CHECK(lines.detail.find("5.3") == std::string::npos);
  CHECK(lines.detail.find("est.") == std::string::npos);
  // It must say why, and which way the real number lies.
  CHECK(lines.detail.find("buffer 128") != std::string::npos);
  CHECK(lines.detail.find("driver reports none") != std::string::npos);
  CHECK(lines.detail.find("higher") != std::string::npos);
}

TEST_CASE("Both lines fit the Settings model-info column")
{
  // The combined single line was 75 characters and ran past the box, clipping its own
  // closing bracket - which is what made the readout look corrupted. The headline row
  // renders at 15 px and the detail row at 12 px in a column of roughly 375 px, so
  // these caps keep both inside it.
  auto CheckFits = [](const LatencyLines& lines) {
    CHECK(lines.headline.size() <= 44);
    CHECK(lines.detail.size() <= 60);
  };

  LatencyReport driverKnown;
  driverKnown.pluginSamples = 62;
  driverKnown.bufferFrames = 1024;
  driverKnown.driverFrames = 4096;
  driverKnown.sampleRate = 44100.0;
  CheckFits(FormatStandaloneLatency(driverKnown));

  LatencyReport driverSilent = driverKnown;
  driverSilent.driverFrames = 0;
  CheckFits(FormatStandaloneLatency(driverSilent));
  CheckFits(FormatPluginLatency(driverKnown));
}

TEST_CASE("In a plugin we report our own delay and name the host's share")
{
  LatencyReport r;
  r.pluginSamples = 64;
  r.bufferFrames = 512;
  r.driverFrames = 1024;
  r.sampleRate = 44100.0;

  const LatencyLines lines = FormatPluginLatency(r);
  CHECK(lines.headline == "Plugin latency: 1.5 ms (64 samples)");
  CHECK(lines.detail.find("host adds its own I/O buffer") != std::string::npos);
  // The host already shows its own round trip; claiming one here would double count.
  CHECK(lines.headline.find("Round trip") == std::string::npos);
  CHECK(lines.detail.find("Round trip") == std::string::npos);

  CHECK(FormatLatencyLines(r, /*standalone=*/false).headline == lines.headline);
  CHECK(FormatLatencyLines(r, /*standalone=*/true).headline == FormatStandaloneLatency(r).headline);
}

TEST_CASE("A report is only re-pushed to the UI when a number actually moved")
{
  // OnIdle polls this every frame; without equality it would rebuild two labels
  // forever. The pre-openStream report differs from the live one in exactly the two
  // fields that arrive late, which is what makes the poll self-correcting.
  LatencyReport beforeStreamOpen;
  beforeStreamOpen.pluginSamples = 0;
  beforeStreamOpen.bufferFrames = 128; // requested; openStream may renegotiate
  beforeStreamOpen.driverFrames = 0; // isStreamOpen() is still false
  beforeStreamOpen.sampleRate = 48000.0;

  LatencyReport live = beforeStreamOpen;
  CHECK(live == beforeStreamOpen);

  live.driverFrames = 1018; // what ASIO4ALL reports once the stream is running
  CHECK(live != beforeStreamOpen);
  CHECK(FormatStandaloneLatency(live).headline == "Round trip: 21.2 ms");

  LatencyReport renegotiated = live;
  renegotiated.bufferFrames = 256;
  CHECK(renegotiated != live);
}

TEST_CASE("A sample rate of zero cannot divide by zero")
{
  LatencyReport r;
  r.pluginSamples = 64;
  r.sampleRate = 0.0; // before the first OnReset
  CHECK(LatencyMs(r.pluginSamples, r.sampleRate) == doctest::Approx(0.0));
  CHECK(FormatPluginLatency(r).headline.find("0.0 ms") != std::string::npos);
}

TEST_CASE("The desktop's 1.4 ms and the laptop's 0.0 ms are the same correct answer")
{
  // The user saw 1.4 ms on a 44.1 kHz desktop and 0.0 ms on a 48 kHz laptop and
  // suspected a bug. Both were the resampler's PDC around a 48 kHz model: real at
  // 44.1 kHz, genuinely absent at 48 kHz. NAM inference itself is causal - it
  // convolves over past samples and needs no lookahead - so three NAM blocks at a
  // matched rate legitimately add nothing. What changes is that neither number is
  // presented as the latency the player hears any more.
  LatencyReport desktop;
  desktop.pluginSamples = 62;
  desktop.bufferFrames = 512;
  desktop.driverFrames = 1024;
  desktop.sampleRate = 44100.0;
  CHECK(LatencyMs(desktop.pluginSamples, desktop.sampleRate) == doctest::Approx(1.4).epsilon(0.02));
  CHECK(FormatStandaloneLatency(desktop).detail.find("plugin 1.4") != std::string::npos);

  LatencyReport laptop = desktop;
  laptop.pluginSamples = 0;
  laptop.sampleRate = 48000.0;
  CHECK(FormatStandaloneLatency(laptop).detail.find("plugin 0.0") != std::string::npos);
  // Same driver, same buffer: the round trip they hear is what dominates both.
  CHECK(RoundTripFrames(laptop) == 1024);
}
