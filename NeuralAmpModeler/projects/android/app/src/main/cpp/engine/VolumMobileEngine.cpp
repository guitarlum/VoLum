#include "VolumMobileEngine.h"

#include <algorithm>
#include <cmath>
#include <filesystem>

#include "get_dsp.h"

namespace volum::mobile
{

namespace
{
inline double DbToLin(double db)
{
  return std::pow(10.0, db / 20.0);
}

// Mirrors VolumMakeNoiseGateTriggerParams() in VoLumProcessBlock.inc.cpp.
dsp::noise_gate::TriggerParams MakeGateParams(double thresholdDb)
{
  const double time = 0.01;
  const double ratio = 0.1;
  const double openTime = 0.005;
  const double holdTime = 0.01;
  const double closeTime = 0.05;
  return {time, thresholdDb, ratio, openTime, holdTime, closeTime};
}

// Final-bus soft clip: map non-finite to 0 and gently tame anything past ~+3 dBFS.
inline float SafetyClip(double v)
{
  if (!std::isfinite(v))
    return 0.0f;
  constexpr double knee = 1.414; // ~+3 dBFS
  if (v > knee)
    v = knee + std::tanh(v - knee);
  else if (v < -knee)
    v = -knee + std::tanh(v + knee);
  return static_cast<float>(v);
}

// Replace any non-finite sample with 0; returns true if it scrubbed anything, so
// the caller can flush the POST feedback lines (NaN would otherwise persist).
inline bool ScrubNonFinite(NAM_SAMPLE* buf, int n)
{
  bool dirty = false;
  for (int i = 0; i < n; ++i)
  {
    if (!std::isfinite(buf[i]))
    {
      buf[i] = 0.0;
      dirty = true;
    }
  }
  return dirty;
}
} // namespace

VolumMobileEngine::VolumMobileEngine() = default;
VolumMobileEngine::~VolumMobileEngine() = default;

void VolumMobileEngine::prepare(double sampleRate, int maxBlockSize)
{
  mSampleRate = sampleRate;
  mMaxBlockSize = maxBlockSize;
  mInMono.assign(static_cast<size_t>(maxBlockSize), 0.0);
  mAmpMono.assign(static_cast<size_t>(maxBlockSize), 0.0);
  mLeft.assign(static_cast<size_t>(maxBlockSize), 0.0);
  mRight.assign(static_cast<size_t>(maxBlockSize), 0.0);

  mToneStack.Reset(sampleRate, maxBlockSize);
  applyPendingTone();

  mHighPass.SetParams(recursive_linear_filter::HighPassParams(sampleRate, 10.0));

  if (!mGateListenerWired)
  {
    mGateTrigger.AddListener(&mGateGain);
    mGateListenerWired = true;
  }
  mGateTrigger.SetSampleRate(sampleRate);

  mDelay.Prepare(2, static_cast<size_t>(maxBlockSize), sampleRate);
  mReverb.Prepare(2, static_cast<size_t>(maxBlockSize), sampleRate);
  mTremolo.Prepare(sampleRate, maxBlockSize, 2);
  mTuner.prepare(sampleRate);

  // Prewarm: force every stage (incl. all Delay/Reverb modes) to allocate its
  // internal buffers now, on this off-audio-thread call, so that a runtime mode
  // switch or first block never allocates on the audio callback.
  {
    std::vector<DSP_SAMPLE> zl(static_cast<size_t>(maxBlockSize), 0.0);
    std::vector<DSP_SAMPLE> zr(static_cast<size_t>(maxBlockSize), 0.0);
    DSP_SAMPLE* mono[1] = {zl.data()};
    DSP_SAMPLE* stereo[2] = {zl.data(), zr.data()};

    mGateTrigger.SetParams(MakeGateParams(-80.0));
    mGateTrigger.Process(mono, 1, maxBlockSize);
    mGateGain.Process(mono, 1, maxBlockSize);
    mToneStack.Process(mono, 1, maxBlockSize);
    mHighPass.Process(mono, 1, maxBlockSize);

    for (int m = 0; m < dsp::effect::Delay::kNumModes; ++m)
    {
      std::fill(zl.begin(), zl.end(), 0.0);
      std::fill(zr.begin(), zr.end(), 0.0);
      mDelay.SetParams(380.0, 0.3, 0.3, m, sampleRate, 0.5, 0.0, false);
      mDelay.Process(stereo, 2, maxBlockSize);
    }
    for (int m = 0; m < dsp::effect::Reverb::kNumModes; ++m)
    {
      std::fill(zl.begin(), zl.end(), 0.0);
      std::fill(zr.begin(), zr.end(), 0.0);
      mReverb.SetParams(0.3, 3.0, 4.5, 20.0, 0.5, m, sampleRate, 0);
      mReverb.Process(stereo, 2, maxBlockSize);
    }
  }
  resetPost();

  // If a model is already loaded, re-reset it for the new stream config.
  std::lock_guard<std::mutex> lock(mModelMutex);
  if (mModel)
    mModel->ResetAndPrewarm(sampleRate, maxBlockSize);
}

void VolumMobileEngine::resetPost()
{
  mDelay.Reset();
  mReverb.Reset();
  mTremolo.Reset();
}

std::string VolumMobileEngine::loadModel(const std::string& path)
{
  std::unique_ptr<nam::DSP> fresh;
  try
  {
    fresh = nam::get_dsp(std::filesystem::path(path));
  }
  catch (const std::exception& e)
  {
    return std::string("load failed: ") + e.what();
  }
  if (!fresh)
    return "load failed: null model";

  const double modelSr = fresh->GetExpectedSampleRate();
  fresh->ResetAndPrewarm(mSampleRate, mMaxBlockSize > 0 ? mMaxBlockSize : 512);

  {
    std::lock_guard<std::mutex> lock(mModelMutex);
    mModel = std::move(fresh);
  }
  mModelSampleRate.store(modelSr, std::memory_order_relaxed);
  mHasModel.store(true, std::memory_order_relaxed);
  return {};
}

void VolumMobileEngine::clearModel()
{
  std::lock_guard<std::mutex> lock(mModelMutex);
  mModel.reset();
  mHasModel.store(false, std::memory_order_relaxed);
}

void VolumMobileEngine::setInputGainDb(double db)
{
  mInputGainLin.store(DbToLin(db), std::memory_order_relaxed);
}

void VolumMobileEngine::setOutputGainDb(double db)
{
  mOutputGainLin.store(DbToLin(db), std::memory_order_relaxed);
}

void VolumMobileEngine::setTone(double bass, double mid, double treble)
{
  mBass.store(bass, std::memory_order_relaxed);
  mMid.store(mid, std::memory_order_relaxed);
  mTreble.store(treble, std::memory_order_relaxed);
  mToneDirty.store(true, std::memory_order_relaxed);
}

void VolumMobileEngine::setGate(bool enabled, double thresholdDb)
{
  mGateEnabled.store(enabled, std::memory_order_relaxed);
  mGateThresholdDb.store(thresholdDb, std::memory_order_relaxed);
}

void VolumMobileEngine::setDelay(bool enabled, double timeMs, double feedback, double mix, int mode, double tone,
                                 double age, bool pingPong)
{
  mDelayEnabled.store(enabled, std::memory_order_relaxed);
  mDelayTimeMs.store(timeMs, std::memory_order_relaxed);
  mDelayFeedback.store(feedback, std::memory_order_relaxed);
  mDelayMix.store(mix, std::memory_order_relaxed);
  mDelayMode.store(mode, std::memory_order_relaxed);
  mDelayTone.store(tone, std::memory_order_relaxed);
  mDelayAge.store(age, std::memory_order_relaxed);
  mDelayPingPong.store(pingPong, std::memory_order_relaxed);
}

void VolumMobileEngine::setReverb(bool enabled, double mix, double decay, double tone, double preDelayMs,
                                  double shimmer, int mode, int subMode)
{
  mReverbEnabled.store(enabled, std::memory_order_relaxed);
  mReverbMix.store(mix, std::memory_order_relaxed);
  mReverbDecay.store(decay, std::memory_order_relaxed);
  mReverbTone.store(tone, std::memory_order_relaxed);
  mReverbPreDelay.store(preDelayMs, std::memory_order_relaxed);
  mReverbShimmer.store(shimmer, std::memory_order_relaxed);
  mReverbMode.store(mode, std::memory_order_relaxed);
  mReverbSubMode.store(subMode, std::memory_order_relaxed);
}

void VolumMobileEngine::setTremolo(bool enabled, double rateHz, double depthKnob, double shape, double mix,
                                   double crossoverHz, int mode)
{
  mTremEnabled.store(enabled, std::memory_order_relaxed);
  mTremRate.store(rateHz, std::memory_order_relaxed);
  mTremDepthKnob.store(depthKnob, std::memory_order_relaxed);
  mTremShape.store(shape, std::memory_order_relaxed);
  mTremMix.store(mix, std::memory_order_relaxed);
  mTremCrossover.store(crossoverHz, std::memory_order_relaxed);
  mTremMode.store(mode, std::memory_order_relaxed);
}

void VolumMobileEngine::applyPendingTone()
{
  if (!mToneDirty.exchange(false, std::memory_order_relaxed))
    return;
  mToneStack.SetParam("bass", mBass.load(std::memory_order_relaxed));
  mToneStack.SetParam("middle", mMid.load(std::memory_order_relaxed));
  mToneStack.SetParam("treble", mTreble.load(std::memory_order_relaxed));
}

void VolumMobileEngine::process(const float* in, float* out, int numFrames)
{
  const int n = std::min(numFrames, mMaxBlockSize);

  // Tuner taps the raw DI (pre-gain, pre-amp) so it tracks the true string
  // pitch regardless of gain/amp/bypass state.
  if (mTunerEnabled.load(std::memory_order_relaxed))
    mTuner.push(in, n);

  if (mBypass.load(std::memory_order_relaxed))
  {
    for (int i = 0; i < numFrames; ++i)
    {
      out[i * 2] = in[i];
      out[i * 2 + 1] = in[i];
    }
    return;
  }

  // ---- Input gain --------------------------------------------------------
  const double inGain = mInputGainLin.load(std::memory_order_relaxed);
  for (int i = 0; i < n; ++i)
    mInMono[static_cast<size_t>(i)] = static_cast<NAM_SAMPLE>(in[i]) * inGain;

  // ---- Noise gate: trigger analyses pre-amp signal -----------------------
  const bool gateOn = mGateEnabled.load(std::memory_order_relaxed);
  DSP_SAMPLE* inPtr[1] = {mInMono.data()};
  DSP_SAMPLE** gatedIn = inPtr;
  if (gateOn)
  {
    mGateTrigger.SetParams(MakeGateParams(mGateThresholdDb.load(std::memory_order_relaxed)));
    mGateTrigger.SetSampleRate(mSampleRate);
    gatedIn = mGateTrigger.Process(inPtr, 1, n);
  }

  // ---- NAM model (dry passthrough while swapping / no model) -------------
  {
    std::unique_lock<std::mutex> lock(mModelMutex, std::try_to_lock);
    if (lock.owns_lock() && mModel)
    {
      NAM_SAMPLE* ins[1] = {gatedIn[0]};
      NAM_SAMPLE* outs[1] = {mAmpMono.data()};
      mModel->process(ins, outs, n);
    }
    else
    {
      for (int i = 0; i < n; ++i)
        mAmpMono[static_cast<size_t>(i)] = gatedIn[0][i];
    }
  }

  // Protect the POST feedback lines from any NaN/inf the model produced.
  if (ScrubNonFinite(mAmpMono.data(), n))
    resetPost();

  // ---- Noise gate gain (applied post-amp) --------------------------------
  DSP_SAMPLE* ampPtr[1] = {mAmpMono.data()};
  DSP_SAMPLE** postGate = ampPtr;
  if (gateOn)
    postGate = mGateGain.Process(ampPtr, 1, n);

  // ---- Tone stack --------------------------------------------------------
  DSP_SAMPLE** toned = postGate;
  if (mToneEnabled.load(std::memory_order_relaxed))
  {
    applyPendingTone();
    toned = mToneStack.Process(postGate, 1, n);
  }

  // ---- DC blocker --------------------------------------------------------
  DSP_SAMPLE** hp = mHighPass.Process(toned, 1, n);

  // ---- Split mono -> stereo POST bus -------------------------------------
  for (int i = 0; i < n; ++i)
  {
    mLeft[static_cast<size_t>(i)] = hp[0][i];
    mRight[static_cast<size_t>(i)] = hp[0][i];
  }
  DSP_SAMPLE* stereo[2] = {mLeft.data(), mRight.data()};
  DSP_SAMPLE** postPtrs = stereo;

  // ---- POST: Delay -> Reverb -> Tremolo (tremolo last, like the amp) -----
  if (mDelayEnabled.load(std::memory_order_relaxed))
  {
    mDelay.SetParams(mDelayTimeMs.load(std::memory_order_relaxed), mDelayFeedback.load(std::memory_order_relaxed),
                     mDelayMix.load(std::memory_order_relaxed), mDelayMode.load(std::memory_order_relaxed), mSampleRate,
                     mDelayTone.load(std::memory_order_relaxed), mDelayAge.load(std::memory_order_relaxed),
                     mDelayPingPong.load(std::memory_order_relaxed));
    postPtrs = mDelay.Process(postPtrs, 2, n);
  }
  if (mReverbEnabled.load(std::memory_order_relaxed))
  {
    mReverb.SetParams(mReverbMix.load(std::memory_order_relaxed), mReverbDecay.load(std::memory_order_relaxed),
                      mReverbTone.load(std::memory_order_relaxed), mReverbPreDelay.load(std::memory_order_relaxed),
                      mReverbShimmer.load(std::memory_order_relaxed), mReverbMode.load(std::memory_order_relaxed),
                      mSampleRate, mReverbSubMode.load(std::memory_order_relaxed));
    postPtrs = mReverb.Process(postPtrs, 2, n);
  }
  if (mTremEnabled.load(std::memory_order_relaxed))
  {
    mTremolo.SetParams(mTremRate.load(std::memory_order_relaxed),
                       volum::VoLumTremoloDepthKnobToInternal(mTremDepthKnob.load(std::memory_order_relaxed)),
                       mTremShape.load(std::memory_order_relaxed), mTremMix.load(std::memory_order_relaxed),
                       mTremCrossover.load(std::memory_order_relaxed), mTremMode.load(std::memory_order_relaxed),
                       mSampleRate);
    mTremolo.Process(postPtrs, 2, n);
  }

  // ---- Output gain + safety scrub + interleave ---------------------------
  const double outGain = mOutputGainLin.load(std::memory_order_relaxed);
  float peak = 0.0f;
  for (int i = 0; i < n; ++i)
  {
    const float l = SafetyClip(postPtrs[0][i] * outGain);
    const float r = SafetyClip(postPtrs[1][i] * outGain);
    out[i * 2] = l;
    out[i * 2 + 1] = r;
    peak = std::max(peak, std::max(std::fabs(l), std::fabs(r)));
  }
  for (int i = n; i < numFrames; ++i)
  {
    out[i * 2] = 0.0f;
    out[i * 2 + 1] = 0.0f;
  }
  mLastPeak.store(peak, std::memory_order_relaxed);
}

} // namespace volum::mobile
