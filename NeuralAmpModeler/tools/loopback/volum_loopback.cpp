// volum_loopback.cpp - measure the true audio round trip through a physical loopback.
//
// Plug output 1 into input 1, run this, and it tells you how long a sample actually
// takes to get from VoLum's output back to its input on this machine and driver.
//
// Why a separate tool rather than a doctest: the number depends on the audio
// hardware and driver in the room, so it cannot run on a CI box. It shares the
// standalone's audio stack (the RtAudio vendored in iPlug2, built with the same ASIO
// backend), so what it measures is what VoLum experiences.
//
// It answers three questions in one run:
//   1. Round trip: emit an impulse, find it in the input, count the frames between.
//   2. Passthrough: did the signal survive the loop at a sane level, or is the cable
//      out, the input muted, or the gain at zero?
//   3. Stability: did the driver report over/underflows while we were listening?
//
// It also prints the driver's OWN reported latency next to the measured one. That
// pairing is the point: VoLum's Settings page shows the driver's figure, and this is
// how we check the driver is not lying.
//
// Output is a line-oriented report plus optional JSON, so scripts can assert on it.

#include "RtAudio.h"
#include "VoLumLoopbackDetect.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <string>
#include <thread>
#include <vector>

namespace
{

struct Options
{
  std::string api = "asio"; // asio | ds | wasapi
  // ASIO exposes one duplex device, so --device is enough there. WASAPI and
  // DirectSound expose capture and render as separate devices, which is also how
  // VoLum's own Audio Settings present them, so input and output are selectable
  // independently.
  std::string device; // substring match for a duplex device; empty = first found
  std::string inDevice;
  std::string outDevice;
  unsigned int sampleRate = 48000;
  unsigned int bufferFrames = 128;
  int pulses = 5; // median over this many bursts
  double amplitude = 0.9;
  int burstFrames = 96; // 2 ms at 48 kHz
  double threshold = 0.0; // 0 = derive from the measured noise floor
  std::string jsonPath;
  bool listOnly = false;
};

struct Shared
{
  std::vector<float> captured;
  size_t capturedWrite = 0;
  size_t framesEmitted = 0;
  // Marked in the capture buffer's own index, not in stream frames: a driver that
  // drops input blocks (WASAPI overflows) advances the two counters by different
  // amounts, and a burst timed against the wrong one is searched for in a window it
  // already passed through.
  size_t burstAtCapture = 0;
  bool burstDue = false;
  bool burstSent = false;
  size_t burstRemaining = 0;
  size_t burstPhase = 0;
  // A single-sample click barely survives a DAC's reconstruction filter and an ADC's
  // anti-aliasing filter, so the test signal is a short full-scale tone burst. Its
  // onset is still sharp enough to time to within a sample or two.
  size_t burstLength = 96;
  double burstAmplitude = 0.9;
  double burstRadiansPerFrame = 0.0;
  unsigned int inputChannels = 1;
  unsigned int outputChannels = 2;
  int overflows = 0;
  int underflows = 0;
  double peakIn = 0.0;
  size_t peakInAt = 0; // capture index of peakIn, so a mis-windowed hit is visible
};

int Callback(void* outputBuffer, void* inputBuffer, unsigned int nFrames, double, RtAudioStreamStatus status,
             void* userData)
{
  auto* s = static_cast<Shared*>(userData);
  if (status & RTAUDIO_INPUT_OVERFLOW)
    s->overflows++;
  if (status & RTAUDIO_OUTPUT_UNDERFLOW)
    s->underflows++;

  auto* out = static_cast<float*>(outputBuffer);
  auto* in = static_cast<float*>(inputBuffer);

  if (out)
    std::memset(out, 0, sizeof(float) * nFrames * s->outputChannels);

  // Start the burst on the first frame of a block, so its emit time is exact.
  if (out && s->burstDue && !s->burstSent)
  {
    s->burstAtCapture = s->capturedWrite;
    s->burstRemaining = s->burstLength;
    s->burstPhase = 0;
    s->burstSent = true;
  }
  if (out && s->burstRemaining > 0)
  {
    const unsigned int n = static_cast<unsigned int>(std::min<size_t>(s->burstRemaining, nFrames));
    for (unsigned int f = 0; f < n; ++f)
    {
      const double v = s->burstAmplitude * std::sin(s->burstRadiansPerFrame * static_cast<double>(s->burstPhase++));
      for (unsigned int c = 0; c < s->outputChannels; ++c)
        out[f * s->outputChannels + c] = static_cast<float>(v);
    }
    s->burstRemaining -= n;
  }

  if (in && s->capturedWrite + nFrames <= s->captured.size())
  {
    for (unsigned int f = 0; f < nFrames; ++f)
    {
      const float v = in[f * s->inputChannels]; // input channel 1
      s->captured[s->capturedWrite + f] = v;
      const double mag = std::fabs(static_cast<double>(v));
      if (mag > s->peakIn)
      {
        s->peakIn = mag;
        s->peakInAt = s->capturedWrite + f;
      }
    }
    s->capturedWrite += nFrames;
  }

  s->framesEmitted += nFrames;
  return 0;
}

void PrintUsage()
{
  std::printf(
    "volum_loopback - measure real audio round-trip latency through a physical loopback\n"
    "  --api asio|ds|wasapi   audio backend (default asio)\n"
    "  --device <substring>   duplex device name match (ASIO)\n"
    "  --in-device <substr>   capture device name match (WASAPI / DirectSound)\n"
    "  --out-device <substr>  render device name match (WASAPI / DirectSound)\n"
    "  --rate <hz>            sample rate (default 48000)\n"
    "  --buffer <frames>      buffer size (default 128)\n"
    "  --pulses <n>           bursts to median over (default 5)\n"
    "  --burst <frames>       burst length (default 96 = 2 ms at 48 kHz)\n"
    "  --amplitude <0..1>     burst amplitude (default 0.9)\n"
    "  --threshold <0..1>     detection threshold (default: 8x noise floor)\n"
    "  --json <path>          also write a JSON report\n"
    "  --list                 list devices and exit\n");
}

RtAudio::Api ApiFromName(const std::string& name)
{
  if (name == "asio")
    return RtAudio::WINDOWS_ASIO;
  if (name == "wasapi")
    return RtAudio::WINDOWS_WASAPI;
  if (name == "ds")
    return RtAudio::WINDOWS_DS;
  return RtAudio::UNSPECIFIED;
}

} // namespace

int main(int argc, char** argv)
{
  Options opt;
  for (int i = 1; i < argc; ++i)
  {
    const std::string a = argv[i];
    auto next = [&]() { return (i + 1 < argc) ? std::string(argv[++i]) : std::string(); };
    if (a == "--api")
      opt.api = next();
    else if (a == "--device")
      opt.device = next();
    else if (a == "--in-device")
      opt.inDevice = next();
    else if (a == "--out-device")
      opt.outDevice = next();
    else if (a == "--rate")
      opt.sampleRate = static_cast<unsigned int>(std::stoul(next()));
    else if (a == "--buffer")
      opt.bufferFrames = static_cast<unsigned int>(std::stoul(next()));
    else if (a == "--pulses")
      opt.pulses = std::stoi(next());
    else if (a == "--burst")
      opt.burstFrames = std::stoi(next());
    else if (a == "--amplitude")
      opt.amplitude = std::stod(next());
    else if (a == "--threshold")
      opt.threshold = std::stod(next());
    else if (a == "--json")
      opt.jsonPath = next();
    else if (a == "--list")
      opt.listOnly = true;
    else
    {
      PrintUsage();
      return 2;
    }
  }

  RtAudio audio(ApiFromName(opt.api));
  const unsigned int count = audio.getDeviceCount();
  if (count == 0)
  {
    std::printf("RESULT no-devices api=%s\n", opt.api.c_str());
    return 3;
  }

  int inIdx = -1;
  int outIdx = -1;
  std::printf("Devices on api=%s:\n", opt.api.c_str());
  for (unsigned int i = 0; i < count; ++i)
  {
    RtAudio::DeviceInfo info = audio.getDeviceInfo(i);
    if (!info.probed)
      continue;
    std::printf("  [%u] %-42s in=%u out=%u\n", i, info.name.c_str(), info.inputChannels, info.outputChannels);

    const std::string& inMatch = opt.inDevice.empty() ? opt.device : opt.inDevice;
    const std::string& outMatch = opt.outDevice.empty() ? opt.device : opt.outDevice;
    if (info.inputChannels > 0 && (inMatch.empty() ? inIdx < 0 : info.name.find(inMatch) != std::string::npos))
      if (inIdx < 0 || !inMatch.empty())
        inIdx = static_cast<int>(i);
    if (info.outputChannels > 0 && (outMatch.empty() ? outIdx < 0 : info.name.find(outMatch) != std::string::npos))
      if (outIdx < 0 || !outMatch.empty())
        outIdx = static_cast<int>(i);
  }

  if (opt.listOnly)
    return 0;
  if (inIdx < 0 || outIdx < 0)
  {
    std::printf("RESULT no-device api=%s in='%s' out='%s'\n", opt.api.c_str(),
                (opt.inDevice.empty() ? opt.device : opt.inDevice).c_str(),
                (opt.outDevice.empty() ? opt.device : opt.outDevice).c_str());
    return 4;
  }

  RtAudio::DeviceInfo inInfo = audio.getDeviceInfo(static_cast<unsigned int>(inIdx));
  RtAudio::DeviceInfo outInfo = audio.getDeviceInfo(static_cast<unsigned int>(outIdx));
  std::printf("Out [%d] %s -> In [%d] %s at %u Hz, buffer %u\n", outIdx, outInfo.name.c_str(), inIdx,
              inInfo.name.c_str(), opt.sampleRate, opt.bufferFrames);

  Shared shared;
  shared.inputChannels = std::min(2u, inInfo.inputChannels);
  shared.outputChannels = std::min(2u, outInfo.outputChannels);
  // Two seconds of headroom: far more than any plausible round trip, so a missing
  // impulse means the signal never arrived rather than that we stopped listening.
  shared.captured.assign(static_cast<size_t>(opt.sampleRate) * 2, 0.0f);
  shared.burstLength = static_cast<size_t>(opt.burstFrames);
  shared.burstAmplitude = opt.amplitude;
  shared.burstRadiansPerFrame = 2.0 * 3.14159265358979323846 * 1000.0 / static_cast<double>(opt.sampleRate);

  RtAudio::StreamParameters oParams;
  oParams.deviceId = static_cast<unsigned int>(outIdx);
  oParams.nChannels = shared.outputChannels;
  RtAudio::StreamParameters iParams;
  iParams.deviceId = static_cast<unsigned int>(inIdx);
  iParams.nChannels = shared.inputChannels;

  RtAudio::StreamOptions sopt;
  sopt.flags = RTAUDIO_MINIMIZE_LATENCY;
  sopt.streamName = "VoLumLoopback";

  unsigned int bufferFrames = opt.bufferFrames;
  try
  {
    audio.openStream(&oParams, &iParams, RTAUDIO_FLOAT32, opt.sampleRate, &bufferFrames, &Callback, &shared, &sopt);
    audio.startStream();
  }
  catch (RtAudioError& e)
  {
    std::printf("RESULT open-failed %s\n", e.getMessage().c_str());
    return 5;
  }

  const long driverLatency = audio.getStreamLatency();
  std::printf("Negotiated buffer: %u frames; driver-reported latency: %ld frames (%.2f ms)\n", bufferFrames,
              driverLatency, 1000.0 * static_cast<double>(driverLatency) / opt.sampleRate);

  // Listen to silence first. The detection threshold has to sit above whatever this
  // input is already picking up (hum, converter noise, a hot preamp) or the very
  // first sample "detects" and every measurement reads zero.
  shared.capturedWrite = 0;
  shared.peakIn = 0.0;
  std::this_thread::sleep_for(std::chrono::milliseconds(300));
  const double noiseFloor = shared.peakIn;
  const double detectThreshold = opt.threshold > 0.0 ? opt.threshold : std::max(0.01, noiseFloor * 8.0);
  std::printf("Noise floor: %.5f; detection threshold: %.5f\n", noiseFloor, detectThreshold);

  std::vector<double> measurements;
  double bestPeak = 0.0;
  for (int p = 0; p < opt.pulses; ++p)
  {
    shared.capturedWrite = 0;
    shared.burstSent = false;
    shared.peakIn = 0.0;
    shared.peakInAt = 0;
    std::fill(shared.captured.begin(), shared.captured.end(), 0.0f);
    // Let the stream settle, then arm the burst and listen for half a second.
    std::this_thread::sleep_for(std::chrono::milliseconds(150));
    shared.burstDue = true;
    std::this_thread::sleep_for(std::chrono::milliseconds(400));
    shared.burstDue = false;
    bestPeak = std::max(bestPeak, shared.peakIn);

    if (!shared.burstSent)
      continue;

    // Where in the capture buffer the burst started, then the first sample after
    // that which crosses the threshold: that gap is the round trip.
    const size_t emitIdx = shared.burstAtCapture;
    const volum::Crossing hit =
      volum::FindFirstCrossing(shared.captured.data(), shared.capturedWrite, emitIdx, detectThreshold);
    if (!hit.found)
    {
      // Report where the loudest sample sat relative to the burst. A peak above the
      // threshold that lands before it means the timing is off, not the cable.
      std::printf("  burst %d: NOT DETECTED (peak in = %.5f at %+.1f ms from emit)\n", p + 1, shared.peakIn,
                  1000.0 * (static_cast<double>(shared.peakInAt) - static_cast<double>(emitIdx)) / opt.sampleRate);
      continue;
    }
    const double frames = static_cast<double>(hit.index - emitIdx);
    measurements.push_back(frames);
    std::printf("  burst %d: %.0f frames (%.2f ms), peak in = %.4f\n", p + 1, frames, 1000.0 * frames / opt.sampleRate,
                shared.peakIn);
  }

  audio.stopStream();
  if (audio.isStreamOpen())
    audio.closeStream();

  if (measurements.empty())
  {
    std::printf("RESULT no-signal out='%s' in='%s' noise_floor=%.5f best_peak=%.5f overflows=%d underflows=%d\n",
                outInfo.name.c_str(), inInfo.name.c_str(), noiseFloor, bestPeak, shared.overflows, shared.underflows);
    std::printf(
      "  Nothing came back above %.5f. Check the loopback cable (out 1 -> in 1), the interface's input "
      "gain and output level knobs, and that the input is not muted.\n",
      detectThreshold);
    return 6;
  }

  std::sort(measurements.begin(), measurements.end());
  const double medianFrames = measurements[measurements.size() / 2];
  const double medianMs = 1000.0 * medianFrames / opt.sampleRate;
  const double spreadFrames = measurements.back() - measurements.front();
  const double driverMs = 1000.0 * static_cast<double>(driverLatency) / opt.sampleRate;

  std::printf(
    "RESULT ok out='%s' in='%s' api=%s rate=%u buffer=%u measured_frames=%.0f measured_ms=%.2f "
    "driver_frames=%ld driver_ms=%.2f spread_frames=%.0f overflows=%d underflows=%d peak_in=%.4f\n",
    outInfo.name.c_str(), inInfo.name.c_str(), opt.api.c_str(), opt.sampleRate, bufferFrames, medianFrames, medianMs,
    driverLatency, driverMs, spreadFrames, shared.overflows, shared.underflows, shared.peakIn);

  if (!opt.jsonPath.empty())
  {
    std::ofstream out(opt.jsonPath);
    out << "{\n"
        << "  \"outDevice\": \"" << outInfo.name << "\",\n"
        << "  \"inDevice\": \"" << inInfo.name << "\",\n"
        << "  \"api\": \"" << opt.api << "\",\n"
        << "  \"sampleRate\": " << opt.sampleRate << ",\n"
        << "  \"bufferFrames\": " << bufferFrames << ",\n"
        << "  \"measuredFrames\": " << medianFrames << ",\n"
        << "  \"measuredMs\": " << medianMs << ",\n"
        << "  \"driverFrames\": " << driverLatency << ",\n"
        << "  \"driverMs\": " << driverMs << ",\n"
        << "  \"spreadFrames\": " << spreadFrames << ",\n"
        << "  \"noiseFloor\": " << noiseFloor << ",\n"
        << "  \"overflows\": " << shared.overflows << ",\n"
        << "  \"underflows\": " << shared.underflows << ",\n"
        << "  \"peakIn\": " << shared.peakIn << "\n"
        << "}\n";
    std::printf("Wrote %s\n", opt.jsonPath.c_str());
  }

  return 0;
}
