#pragma once

// VoLumLatencyReport.h: what the Settings page says about latency.
//
// Through 1.2.0 the readout was "Current latency: X ms (N samples)", where N is the
// plugin's own algorithmic delay (PDC) - the number the host compensates for. That
// is an honest number and also a useless one for the question people actually ask,
// which is "why do I hear my playing late?". On a 48 kHz machine with no pitch
// shifting it reads 0.0 ms while the player hears well over 20 ms of ASIO round trip.
//
// So in the standalone we report the round trip: our PDC plus the audio interface's
// input-to-output delay, but only when the driver actually reports that delay (ASIO
// drivers do, and their figure already includes their own buffering).
//
// When the driver reports nothing we state no total at all. The obvious stand-in -
// two buffers - is not a conservative estimate, it is a flattering one: at 128 frames
// it claims 5.3 ms, while a hardware loopback on the same machine and buffer measured
// 63.7 ms through WASAPI and 21.2 ms through ASIO4ALL. A wrong small number reads as
// authoritative, which is worse than the 0.0 ms it was meant to replace.
//
// In a plugin the host owns the I/O buffer and shows its own round trip, so there we
// keep reporting our PDC and name it as such.
//
// Two lines, because one line of this does not fit the Settings box and silently
// clipped its own closing bracket. The headline carries the number; the detail line
// carries the caveat.
//
// Pure and header-only so the wording and the arithmetic are doctested.

#include <cmath>
#include <cstdio>
#include <string>

namespace volum
{

struct LatencyReport
{
  int pluginSamples = 0; // our PDC, what SetLatency() reports to the host
  int bufferFrames = 0; // audio I/O block size
  int driverFrames = 0; // device-reported input+output latency; 0 = unknown
  double sampleRate = 0.0;
};

inline double LatencyMs(int samples, double sampleRate)
{
  if (!(sampleRate > 0.0))
    return 0.0;
  return 1000.0 * static_cast<double>(samples) / sampleRate;
}

inline bool operator==(const LatencyReport& a, const LatencyReport& b)
{
  return a.pluginSamples == b.pluginSamples && a.bufferFrames == b.bufferFrames && a.driverFrames == b.driverFrames
         && a.sampleRate == b.sampleRate;
}

inline bool operator!=(const LatencyReport& a, const LatencyReport& b)
{
  return !(a == b);
}

inline bool DriverLatencyKnown(const LatencyReport& r)
{
  return r.driverFrames > 0;
}

// Only meaningful when the driver reports its own latency. That figure already
// includes the driver's buffering, so the buffer size is not added on top.
inline int RoundTripFrames(const LatencyReport& r)
{
  return DriverLatencyKnown(r) ? r.pluginSamples + r.driverFrames : 0;
}

// What the Settings page shows: a headline and a smaller qualifying line.
struct LatencyLines
{
  std::string headline;
  std::string detail;
};

// Standalone with a driver figure: lead with the round trip the player feels, then
// break it down so a large value points at its cause (driver vs. our processing).
// Without one: our own delay only, and an explicit statement that the total is
// unknown and larger. Never a total built on a guess.
inline LatencyLines FormatStandaloneLatency(const LatencyReport& r)
{
  char headline[128];
  char detail[128];
  const double plug = LatencyMs(r.pluginSamples, r.sampleRate);
  if (DriverLatencyKnown(r))
  {
    std::snprintf(headline, sizeof(headline), "Round trip: %.1f ms", LatencyMs(RoundTripFrames(r), r.sampleRate));
    std::snprintf(detail, sizeof(detail), "plugin %.1f + driver %.1f ms, buffer %d", plug,
                  LatencyMs(r.driverFrames, r.sampleRate), r.bufferFrames);
  }
  else
  {
    std::snprintf(headline, sizeof(headline), "Plugin latency: %.1f ms (%d samples)", plug, r.pluginSamples);
    std::snprintf(detail, sizeof(detail), "buffer %d; driver reports none, real round trip is higher", r.bufferFrames);
  }
  return {headline, detail};
}

// Plugin: the host compensates our PDC and shows its own I/O figure, so claiming a
// round trip here would be double counting.
inline LatencyLines FormatPluginLatency(const LatencyReport& r)
{
  char headline[128];
  std::snprintf(headline, sizeof(headline), "Plugin latency: %.1f ms (%d samples)",
                LatencyMs(r.pluginSamples, r.sampleRate), r.pluginSamples);
  return {headline, "your host adds its own I/O buffer on top"};
}

inline LatencyLines FormatLatencyLines(const LatencyReport& r, bool standalone)
{
  return standalone ? FormatStandaloneLatency(r) : FormatPluginLatency(r);
}

} // namespace volum
