#include "third_party/doctest.h"

#include <string>

#include "../VoLumLatencyReport.h"

// The Settings-page latency line. The 1.2.1 report was "shows 0 ms on the laptop but
// I hear way more than 21 ms" - both true at once, because the old readout only
// counted our own algorithmic delay. These pin what the new line says and, just as
// importantly, that it never double-counts the driver's buffering.

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

  const std::string line = FormatStandaloneLatency(r);
  CHECK(line.find("Round trip: 21.3 ms") != std::string::npos);
  CHECK(line.find("plugin 0.0") != std::string::npos);
  CHECK(line.find("driver 21.3") != std::string::npos);
  CHECK(line.find("buffer 512") != std::string::npos);
  // The old line was the whole complaint; it must not survive.
  CHECK(line.find("Current latency") == std::string::npos);
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
  CHECK(IoRoundTripFrames(r) == 300);
  CHECK(RoundTripFrames(r) == 300);
}

TEST_CASE("Without a driver figure the estimate is two buffers and says so")
{
  LatencyReport r;
  r.pluginSamples = 64;
  r.bufferFrames = 256;
  r.driverFrames = 0; // DirectSound / WASAPI often report nothing
  r.sampleRate = 48000.0;

  CHECK_FALSE(DriverLatencyKnown(r));
  CHECK(IoRoundTripFrames(r) == 512);
  CHECK(RoundTripFrames(r) == 576);

  const std::string line = FormatStandaloneLatency(r);
  CHECK(line.find("~") != std::string::npos); // marked as an estimate
  CHECK(line.find("est.") != std::string::npos);
  CHECK(line.find("driver reports none") != std::string::npos);
}

TEST_CASE("In a plugin we report our own delay and name the host's share")
{
  LatencyReport r;
  r.pluginSamples = 64;
  r.bufferFrames = 512;
  r.driverFrames = 1024;
  r.sampleRate = 44100.0;

  const std::string line = FormatPluginLatency(r);
  CHECK(line.find("Plugin latency: 1.5 ms (64 samples)") != std::string::npos);
  CHECK(line.find("host adds its I/O buffer") != std::string::npos);
  // The host already shows its own round trip; claiming one here would double count.
  CHECK(line.find("Round trip") == std::string::npos);

  CHECK(FormatLatencyLine(r, /*standalone=*/false) == line);
  CHECK(FormatLatencyLine(r, /*standalone=*/true) == FormatStandaloneLatency(r));
}

TEST_CASE("A sample rate of zero cannot divide by zero")
{
  LatencyReport r;
  r.pluginSamples = 64;
  r.sampleRate = 0.0; // before the first OnReset
  CHECK(LatencyMs(r.pluginSamples, r.sampleRate) == doctest::Approx(0.0));
  CHECK(FormatPluginLatency(r).find("0.0 ms") != std::string::npos);
}

TEST_CASE("The desktop's 1.4 ms and the laptop's 0.0 ms are the same correct answer")
{
  // The user saw 1.4 ms on a 44.1 kHz desktop and 0.0 ms on a 48 kHz laptop and
  // suspected a bug. Both were the resampler's PDC around a 48 kHz model: real at
  // 44.1 kHz, genuinely absent at 48 kHz. What changes is that neither number is
  // presented as the latency the player hears any more.
  LatencyReport desktop;
  desktop.pluginSamples = 62;
  desktop.bufferFrames = 512;
  desktop.driverFrames = 1024;
  desktop.sampleRate = 44100.0;
  CHECK(LatencyMs(desktop.pluginSamples, desktop.sampleRate) == doctest::Approx(1.4).epsilon(0.02));
  CHECK(FormatStandaloneLatency(desktop).find("plugin 1.4") != std::string::npos);

  LatencyReport laptop = desktop;
  laptop.pluginSamples = 0;
  laptop.sampleRate = 48000.0;
  CHECK(FormatStandaloneLatency(laptop).find("plugin 0.0") != std::string::npos);
  // Same driver, same buffer: the round trip they hear is what dominates both.
  CHECK(RoundTripFrames(laptop) == 1024);
}
