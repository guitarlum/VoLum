#include "OboeAudioHost.h"

#include <android/log.h>
#include <algorithm>

#define LOG_TAG "VoLumOboe"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

namespace volum::mobile
{

namespace
{
constexpr int kMaxCallbackFrames = 4096;
}

OboeAudioHost::OboeAudioHost()
{
  mMonoScratch.assign(kMaxCallbackFrames, 0.0f);
}

OboeAudioHost::~OboeAudioHost()
{
  stopStreams();
}

std::shared_ptr<oboe::AudioStream> OboeAudioHost::openStream(oboe::Direction dir, int channelCount, int deviceId,
                                                            int sampleRate)
{
  oboe::AudioStreamBuilder b;
  b.setDirection(dir)
    ->setPerformanceMode(oboe::PerformanceMode::LowLatency)
    ->setSharingMode(oboe::SharingMode::Exclusive)
    ->setFormat(oboe::AudioFormat::Float)
    ->setChannelCount(channelCount)
    ->setSampleRate(sampleRate)
    ->setSampleRateConversionQuality(oboe::SampleRateConversionQuality::Medium);

  if (deviceId != 0)
    b.setDeviceId(deviceId);

  if (dir == oboe::Direction::Input)
  {
    // Guitar DI: no automatic gain / noise suppression.
    b.setInputPreset(oboe::InputPreset::Unprocessed);
  }
  else
  {
    // FullDuplexStream drives itself from the OUTPUT stream's data callback:
    // onAudioReady() reads the input stream and invokes onBothStreamsReady().
    // Without this the output stream stays in blocking mode and never pumps.
    b.setDataCallback(this);
  }

  std::shared_ptr<oboe::AudioStream> stream;
  const oboe::Result res = b.openStream(stream);
  if (res != oboe::Result::OK)
  {
    LOGE("openStream(%s) failed: %s", dir == oboe::Direction::Input ? "in" : "out", oboe::convertToText(res));
    return nullptr;
  }
  return stream;
}

bool OboeAudioHost::startStreams(int inputDeviceId, int outputDeviceId, int sampleRate, int /*framesPerBurstHint*/)
{
  if (mRunning.load())
    return true;

  mInputStream = openStream(oboe::Direction::Input, 1, inputDeviceId, sampleRate);
  if (!mInputStream)
    return false;

  const int actualSr = mInputStream->getSampleRate();
  mOutputStream = openStream(oboe::Direction::Output, 2, outputDeviceId, actualSr);
  if (!mOutputStream)
  {
    mInputStream->close();
    mInputStream.reset();
    return false;
  }

  mActualSampleRate.store(actualSr, std::memory_order_relaxed);

  // Size the engine for the worst-case burst we might see.
  const int maxBlock = std::max(mOutputStream->getFramesPerBurst() * 4, 512);
  mEngine.prepare(static_cast<double>(actualSr), std::min(maxBlock, kMaxCallbackFrames));

  setInputStream(mInputStream.get());
  setOutputStream(mOutputStream.get());

  const oboe::Result res = FullDuplexStream::start();
  if (res != oboe::Result::OK)
  {
    LOGE("FullDuplexStream::start failed: %s", oboe::convertToText(res));
    stopStreams();
    return false;
  }

  mRunning.store(true, std::memory_order_relaxed);
  updateLatency();
  LOGI("Audio started: sr=%d inBurst=%d outBurst=%d", actualSr, mInputStream->getFramesPerBurst(),
       mOutputStream->getFramesPerBurst());
  return true;
}

void OboeAudioHost::stopStreams()
{
  if (mOutputStream || mInputStream)
    FullDuplexStream::stop();

  if (mOutputStream)
  {
    mOutputStream->close();
    mOutputStream.reset();
  }
  if (mInputStream)
  {
    mInputStream->close();
    mInputStream.reset();
  }
  mRunning.store(false, std::memory_order_relaxed);
}

void OboeAudioHost::updateLatency()
{
  if (!mOutputStream)
    return;
  const auto result = mOutputStream->calculateLatencyMillis();
  if (result)
    mOutputLatencyMs.store(result.value(), std::memory_order_relaxed);
  const auto xr = mOutputStream->getXRunCount();
  mXRuns.store(xr ? xr.value() : 0, std::memory_order_relaxed);
}

oboe::DataCallbackResult OboeAudioHost::onBothStreamsReady(const void* inputData, int numInputFrames, void* outputData,
                                                           int numOutputFrames)
{
  auto* out = static_cast<float*>(outputData);        // interleaved stereo
  const auto* in = static_cast<const float*>(inputData); // mono

  mFramesPerCallback.store(numOutputFrames, std::memory_order_relaxed);

  // Priming callbacks: input not yet ready -> output silence.
  if (numInputFrames < numOutputFrames || in == nullptr)
  {
    std::fill(out, out + static_cast<size_t>(numOutputFrames) * 2, 0.0f);
    return oboe::DataCallbackResult::Continue;
  }

  // Engine consumes mono input and writes interleaved stereo directly into the
  // Oboe output buffer (the POST chain runs in stereo). It internally clamps
  // processing to its prepared max block and zero-fills any tail frames.
  mEngine.process(in, out, numOutputFrames);

  return oboe::DataCallbackResult::Continue;
}

} // namespace volum::mobile
