#pragma once

// VoLum Android — real-time engine.
//
// Reuses the shared VoLum/NAM DSP core behind an Oboe-friendly, allocation-free
// process() entry point. The signal path faithfully mirrors the desktop MAIN +
// POST chain (see NeuralAmpModeler/VoLumProcessBlock.inc.cpp), using the exact
// same DSP classes rather than reimplementations:
//
//   input gain
//     -> noise gate (dsp::noise_gate::Trigger analyses pre-amp, Gain applied post-amp)
//     -> NAM model
//     -> tone stack (BasicNamToneStack)
//     -> DC blocker (recursive_linear_filter::HighPass)
//     -> [split mono -> stereo]
//     -> Delay   (dsp::effect::Delay)
//     -> Reverb  (dsp::effect::Reverb)
//     -> Tremolo (volum::TremoloDSP, runs last)
//     -> output gain -> final-bus safety scrub
//
// It deliberately does NOT pull in iPlug. The PRE pedals, dual-amp support lane,
// and per-lane IR are the remaining desktop-parity items; the architecture is
// ready for them.
//
// Threading contract:
//   - process() runs on the Oboe audio callback thread. It never allocates
//     (all DSP buffers are pre-sized/prewarmed in prepare()), never locks
//     blockingly (try_lock only on the model), never throws.
//   - loadModel()/prepare() run off the audio thread.
//   - Parameter setters are lock-free (atomics), applied at block start.

#include <atomic>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include "dsp.h"                  // nam::DSP, NAM_SAMPLE
#include "ToneStack.h"            // dsp::tone_stack::BasicNamToneStack, DSP_SAMPLE
#include "NoiseGate.h"            // dsp::noise_gate::Trigger / Gain
#include "RecursiveLinearFilter.h"// recursive_linear_filter::HighPass
#include "Delay.h"               // dsp::effect::Delay
#include "Reverb.h"              // dsp::effect::Reverb
#include "VoLumTremolo.h"        // volum::TremoloDSP
#include "TunerDsp.h"            // volum::mobile::TunerDsp

namespace volum::mobile
{

class VolumMobileEngine
{
public:
  VolumMobileEngine();
  ~VolumMobileEngine();

  // Off audio thread. Sizes scratch buffers, resets and prewarms every stage so
  // the steady-state process() is allocation-free (incl. all Delay/Reverb modes).
  void prepare(double sampleRate, int maxBlockSize);

  // Off audio thread. Loads a .nam and hot-swaps it in under mModelMutex.
  // Returns empty string on success, else an error message.
  std::string loadModel(const std::string& path);

  // Off audio thread. Drops the active model (engine passes audio through dry).
  void clearModel();

  bool hasModel() const { return mHasModel.load(std::memory_order_relaxed); }
  double modelSampleRate() const { return mModelSampleRate.load(std::memory_order_relaxed); }

  // ---- Lock-free parameter setters (any thread) --------------------------
  void setInputGainDb(double db);
  void setOutputGainDb(double db);
  void setTone(double bass, double mid, double treble); // each 0..10, 5 = noon
  void setToneEnabled(bool on) { mToneEnabled.store(on, std::memory_order_relaxed); }
  void setBypass(bool on) { mBypass.store(on, std::memory_order_relaxed); }
  void setTunerEnabled(bool on) { mTunerEnabled.store(on, std::memory_order_relaxed); }
  float tunerHz() const { return mTuner.hz(); }
  float tunerClarity() const { return mTuner.clarity(); }

  void setGate(bool enabled, double thresholdDb);
  void setDelay(bool enabled, double timeMs, double feedback, double mix, int mode, double tone, double age,
                bool pingPong);
  void setReverb(bool enabled, double mix, double decay, double tone, double preDelayMs, double shimmer, int mode,
                 int subMode);
  void setTremolo(bool enabled, double rateHz, double depthKnob, double shape, double mix, double crossoverHz,
                  int mode);

  // Audio thread. Mono in -> interleaved stereo out. numFrames <= maxBlockSize.
  void process(const float* in, float* interleavedStereoOut, int numFrames);

  // Diagnostics (audio thread writes, UI reads).
  float lastPeak() const { return mLastPeak.load(std::memory_order_relaxed); }

private:
  void applyPendingTone();
  void resetPost();

  double mSampleRate = 48000.0;
  int mMaxBlockSize = 0;

  // Model: swapped under mModelMutex off-thread; process() try_locks.
  std::unique_ptr<nam::DSP> mModel;
  mutable std::mutex mModelMutex;
  std::atomic<bool> mHasModel{false};
  std::atomic<double> mModelSampleRate{0.0};

  // MAIN chain DSP.
  dsp::noise_gate::Trigger mGateTrigger;
  dsp::noise_gate::Gain mGateGain;
  bool mGateListenerWired = false;
  dsp::tone_stack::BasicNamToneStack mToneStack;
  recursive_linear_filter::HighPass mHighPass;

  // POST chain DSP (stereo).
  dsp::effect::Delay mDelay;
  dsp::effect::Reverb mReverb;
  volum::TremoloDSP mTremolo;
  TunerDsp mTuner;
  std::atomic<bool> mTunerEnabled{false};

  // ---- Lock-free params --------------------------------------------------
  std::atomic<double> mInputGainLin{1.0};
  std::atomic<double> mOutputGainLin{1.0};
  std::atomic<bool> mToneEnabled{true};
  std::atomic<bool> mBypass{false};
  std::atomic<bool> mToneDirty{false};
  std::atomic<double> mBass{5.0}, mMid{5.0}, mTreble{5.0};

  std::atomic<bool> mGateEnabled{true};
  std::atomic<double> mGateThresholdDb{-80.0};

  std::atomic<bool> mDelayEnabled{false};
  std::atomic<double> mDelayTimeMs{380.0};
  std::atomic<double> mDelayFeedback{0.35};
  std::atomic<double> mDelayMix{0.28};
  std::atomic<int> mDelayMode{0};
  std::atomic<double> mDelayTone{0.5};
  std::atomic<double> mDelayAge{0.0};
  std::atomic<bool> mDelayPingPong{false};

  std::atomic<bool> mReverbEnabled{false};
  std::atomic<double> mReverbMix{0.3};
  std::atomic<double> mReverbDecay{3.0};
  std::atomic<double> mReverbTone{4.5};
  std::atomic<double> mReverbPreDelay{20.0};
  std::atomic<double> mReverbShimmer{0.5};
  std::atomic<int> mReverbMode{0};
  std::atomic<int> mReverbSubMode{0};

  std::atomic<bool> mTremEnabled{false};
  std::atomic<double> mTremRate{5.0};
  std::atomic<double> mTremDepthKnob{0.5};
  std::atomic<double> mTremShape{0.0};
  std::atomic<double> mTremMix{1.0};
  std::atomic<double> mTremCrossover{800.0};
  std::atomic<int> mTremMode{1};

  std::atomic<float> mLastPeak{0.0f};

  // Audio-thread scratch (double), sized in prepare().
  std::vector<NAM_SAMPLE> mInMono;   // input * gain
  std::vector<NAM_SAMPLE> mAmpMono;  // amp/tone/hpf output (mono)
  std::vector<DSP_SAMPLE> mLeft;     // POST stereo L
  std::vector<DSP_SAMPLE> mRight;    // POST stereo R
};

} // namespace volum::mobile
