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
// input-to-output delay. The driver's own figure is preferred when RtAudio can get
// one (ASIO drivers report it and it already includes their buffering); otherwise we
// estimate two buffers, and say so, rather than quietly presenting a guess as fact.
//
// In a plugin the host owns the I/O buffer and shows its own round trip, so there we
// keep reporting our PDC and name it as such.
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

inline bool DriverLatencyKnown(const LatencyReport& r)
{
  return r.driverFrames > 0;
}

// The I/O half of the round trip: the driver's figure when we have it, otherwise the
// classic two-buffer estimate (one block being captured while the previous plays).
inline int IoRoundTripFrames(const LatencyReport& r)
{
  return DriverLatencyKnown(r) ? r.driverFrames : 2 * r.bufferFrames;
}

inline int RoundTripFrames(const LatencyReport& r)
{
  return r.pluginSamples + IoRoundTripFrames(r);
}

// Standalone: lead with the number the player feels, then break it down so a large
// value points at its cause (driver vs. our own processing).
inline std::string FormatStandaloneLatency(const LatencyReport& r)
{
  char buf[192];
  const double total = LatencyMs(RoundTripFrames(r), r.sampleRate);
  const double plug = LatencyMs(r.pluginSamples, r.sampleRate);
  const double io = LatencyMs(IoRoundTripFrames(r), r.sampleRate);
  if (DriverLatencyKnown(r))
    std::snprintf(buf, sizeof(buf), "Round trip: %.1f ms (plugin %.1f + driver %.1f, buffer %d)", total, plug, io,
                  r.bufferFrames);
  else
    std::snprintf(buf, sizeof(buf), "Round trip: ~%.1f ms est. (plugin %.1f + 2 x buffer %d; driver reports none)",
                  total, plug, r.bufferFrames);
  return buf;
}

// Plugin: the host compensates our PDC and shows its own I/O figure, so claiming a
// round trip here would be double counting.
inline std::string FormatPluginLatency(const LatencyReport& r)
{
  char buf[192];
  std::snprintf(buf, sizeof(buf), "Plugin latency: %.1f ms (%d samples); your host adds its I/O buffer",
                LatencyMs(r.pluginSamples, r.sampleRate), r.pluginSamples);
  return buf;
}

inline std::string FormatLatencyLine(const LatencyReport& r, bool standalone)
{
  return standalone ? FormatStandaloneLatency(r) : FormatPluginLatency(r);
}

} // namespace volum
