#pragma once

// VoLum Android — Oboe duplex audio host (M2).
//
// Opens a low-latency duplex stream (mono guitar in -> stereo out) intended for
// a class-compliant USB audio interface, and runs VolumMobileEngine in the
// callback. Input preset is Unprocessed (no AGC/NS) so the guitar tone reaches
// the model clean.
//
// Live USB-interface validation is a device handoff; on an emulator this runs
// against the default (built-in) devices.

#include <atomic>
#include <memory>
#include <vector>

#include <oboe/Oboe.h>

#include "../engine/VolumMobileEngine.h"

namespace volum::mobile
{

class OboeAudioHost : public oboe::FullDuplexStream
{
public:
  OboeAudioHost();
  ~OboeAudioHost() override;

  VolumMobileEngine& engine() { return mEngine; }

  // Open + start streams. deviceId 0 => AAudio default. Returns true on success.
  // Named *Streams to avoid clashing with oboe::FullDuplexStream::start()/stop().
  bool startStreams(int inputDeviceId, int outputDeviceId, int sampleRate, int framesPerBurstHint);
  void stopStreams();

  bool isRunning() const { return mRunning.load(std::memory_order_relaxed); }

  // Diagnostics. Call refreshDiagnostics() off the audio thread to update.
  void refreshDiagnostics() { updateLatency(); }
  double outputLatencyMs() const { return mOutputLatencyMs.load(std::memory_order_relaxed); }
  int32_t xRunCount() const { return mXRuns.load(std::memory_order_relaxed); }
  int32_t actualSampleRate() const { return mActualSampleRate.load(std::memory_order_relaxed); }
  int32_t framesPerCallback() const { return mFramesPerCallback.load(std::memory_order_relaxed); }

  // oboe::FullDuplexStream
  oboe::DataCallbackResult onBothStreamsReady(const void* inputData, int numInputFrames, void* outputData,
                                              int numOutputFrames) override;

private:
  std::shared_ptr<oboe::AudioStream> openStream(oboe::Direction dir, int channelCount, int deviceId, int sampleRate);
  void updateLatency();

  VolumMobileEngine mEngine;

  std::shared_ptr<oboe::AudioStream> mInputStream;
  std::shared_ptr<oboe::AudioStream> mOutputStream;

  std::atomic<bool> mRunning{false};
  std::atomic<double> mOutputLatencyMs{0.0};
  std::atomic<int32_t> mXRuns{0};
  std::atomic<int32_t> mActualSampleRate{0};
  std::atomic<int32_t> mFramesPerCallback{0};

  // Pre-sized mono scratch for the callback (no RT allocation).
  std::vector<float> mMonoScratch;
};

} // namespace volum::mobile
