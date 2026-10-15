#include "third_party/doctest.h"
#include "../VoLumPreEffects.h"

#define _USE_MATH_DEFINES
#include <algorithm>
#include <cmath>
#include <cstring>
#include <vector>

#ifndef M_PI
  #define M_PI 3.14159265358979323846
#endif

namespace
{
// VoLumCompressor before the fet-reuse / 0 dB pow trims: fet() evaluated again in the wet
// path and pow() on every sample. Keep the expressions as they were; the live compressor must
// match it bit for bit.
class ReferenceCompressor
{
public:
  void SetParams(double amount, double attackMs, double releaseMs, double mix, double levelDb, double sampleRate)
  {
    mAmount = std::clamp(amount, 0.0, 10.0);
    mAttackMs = std::clamp(attackMs, 0.02, 30.0);
    mReleaseMs = std::clamp(releaseMs, 20.0, 1100.0);
    mMix = std::clamp(mix, 0.0, 1.0);
    const double clampedLevelDb = std::clamp(levelDb, -20.0, 20.0);
    mLevel = volum::IsLevelMuteValue(clampedLevelDb, -20.0)
               ? 0.0
               : std::pow(10.0, (clampedLevelDb + dsp::effect::VoLumCompressor::kUnityOutputCalibrationDb) / 20.0);
    if (mSampleRate != sampleRate)
    {
      mSampleRate = sampleRate;
      mEnvelope = 0.0;
      mEnvelopeSlow = 0.0;
    }
  }

  void Process(DSP_SAMPLE** inputs, DSP_SAMPLE** outputs, size_t numChannels, size_t numFrames)
  {
    if (mSampleRate <= 0.0)
      mSampleRate = 48000.0;
    const double inputDriveDb = mAmount * 2.4;
    const double inputDriveGain = std::pow(10.0, inputDriveDb / 20.0);
    const double thresholdDb = -22.0 + (10.0 - mAmount) * 0.6;
    const double softKneeDb = 6.0;
    const double makeupDb = mAmount * 0.55;
    const double makeup = std::pow(10.0, makeupDb / 20.0);
    const double attack = std::exp(-1.0 / ((mAttackMs / 1000.0) * mSampleRate));
    const double releaseFast = std::exp(-1.0 / ((mReleaseMs / 1000.0) * mSampleRate));
    const double releaseSlow = std::exp(-1.0 / ((mReleaseMs * 4.0 / 1000.0) * mSampleRate));
    constexpr double kFetA = 1.0;
    constexpr double kFetB = 0.06;
    const double fetTanhB = std::tanh(kFetB);
    auto fet = [&](double x) {
      const double drive = 1.0 + (inputDriveGain - 1.0) * 0.5;
      return std::tanh(kFetA * drive * x + kFetB) - fetTanhB;
    };

    for (size_t s = 0; s < numFrames; ++s)
    {
      double detector = 0.0;
      for (size_t c = 0; c < numChannels; ++c)
      {
        const double driven = fet(static_cast<double>(inputs[c][s]) * inputDriveGain);
        detector = std::max(detector, std::abs(driven));
      }
      const double targetEnv = detector;
      const double coeff = targetEnv > mEnvelope ? attack : releaseFast;
      mEnvelope = coeff * mEnvelope + (1.0 - coeff) * targetEnv;
      const double slowCoeff = targetEnv > mEnvelopeSlow ? attack : releaseSlow;
      mEnvelopeSlow = slowCoeff * mEnvelopeSlow + (1.0 - slowCoeff) * targetEnv;
      const double envForGr = std::max(mEnvelope, mEnvelopeSlow);
      const double envDb = 20.0 * std::log10(std::max(envForGr, 1.0e-9));

      double gainDb = 0.0;
      const double over = envDb - thresholdDb;
      if (over > softKneeDb * 0.5)
      {
        gainDb = -(over - over / dsp::effect::VoLumCompressor::kFixedRatio);
      }
      else if (over > -softKneeDb * 0.5)
      {
        const double slope = 1.0 - 1.0 / dsp::effect::VoLumCompressor::kFixedRatio;
        const double kneeOver = over;
        gainDb = -slope * (kneeOver + softKneeDb * 0.5) * (kneeOver + softKneeDb * 0.5) / (2.0 * softKneeDb);
      }
      if (gainDb == 0.0)
        ++belowKneeSamples;
      else
        ++compressingSamples;

      const double gain = std::pow(10.0, gainDb / 20.0) * makeup;
      for (size_t c = 0; c < numChannels; ++c)
      {
        const double dry = static_cast<double>(inputs[c][s]);
        const double driven = fet(dry * inputDriveGain);
        const double wet = driven * gain * mLevel;
        double out = dry * (1.0 - mMix) + wet * mMix;
        if (!std::isfinite(out))
          out = 0.0;
        outputs[c][s] = static_cast<DSP_SAMPLE>(out);
      }
    }
  }

  size_t belowKneeSamples = 0;
  size_t compressingSamples = 0;

private:
  double mSampleRate = 0.0;
  double mAmount = 3.0;
  double mAttackMs = 0.4;
  double mReleaseMs = 250.0;
  double mMix = 1.0;
  double mLevel = 1.0;
  double mEnvelope = 0.0;
  double mEnvelopeSlow = 0.0;
};
} // namespace

TEST_CASE("VoLumCompressor output is bit-identical to the pre-trim compressor")
{
  // Loud bursts separated by near-silence so the envelope falls below the knee (the skipped
  // pow) and climbs back over it; params change mid-stream; mono and stereo.
  struct Settings
  {
    double amount, attackMs, releaseMs, mix, levelDb;
  };
  const Settings settings[] = {
    {7.25, 0.38, 185.0, 0.82, 1.5}, {0.0, 0.02, 20.0, 1.0, 0.0},  {10.0, 1.0, 1100.0, 1.0, -20.0},
    {3.0, 0.4, 250.0, 0.5, 6.0},    {5.5, 30.0, 60.0, 1.0, 20.0},
  };
  constexpr double sampleRate = 48000.0;
  constexpr size_t frames = 256;
  constexpr size_t blocks = 1500;
  for (size_t numChannels : {size_t(1), size_t(2)})
  {
    dsp::effect::VoLumCompressor live;
    ReferenceCompressor ref;
    std::vector<std::vector<DSP_SAMPLE>> in(numChannels, std::vector<DSP_SAMPLE>(frames));
    std::vector<std::vector<DSP_SAMPLE>> refOut(numChannels, std::vector<DSP_SAMPLE>(frames));
    size_t frame = 0;
    bool allSame = true;
    size_t firstMismatchBlock = 0;
    for (size_t b = 0; b < blocks; ++b)
    {
      const Settings& p = settings[(b / 150) % (sizeof(settings) / sizeof(settings[0]))];
      live.SetParams(p.amount, 4.0, p.attackMs, p.releaseMs, p.mix, p.levelDb, sampleRate);
      ref.SetParams(p.amount, p.attackMs, p.releaseMs, p.mix, p.levelDb, sampleRate);
      for (size_t i = 0; i < frames; ++i, ++frame)
      {
        const double t = static_cast<double>(frame) / sampleRate;
        // 0.1 s loud, then 0.4 s at about -120 dBFS.
        const double level = (static_cast<size_t>(t * 10.0) % 5 == 0) ? 0.8 : 1.0e-6;
        for (size_t c = 0; c < numChannels; ++c)
          in[c][i] = static_cast<DSP_SAMPLE>(level * std::sin(2.0 * M_PI * (150.0 + 90.0 * c) * t + 0.3 * c));
      }
      std::vector<DSP_SAMPLE*> inPtrs(numChannels), refPtrs(numChannels);
      for (size_t c = 0; c < numChannels; ++c)
      {
        inPtrs[c] = in[c].data();
        refPtrs[c] = refOut[c].data();
      }
      ref.Process(inPtrs.data(), refPtrs.data(), numChannels, frames);
      DSP_SAMPLE** out = live.Process(inPtrs.data(), numChannels, frames);
      for (size_t c = 0; c < numChannels; ++c)
      {
        if (allSame && std::memcmp(out[c], refOut[c].data(), frames * sizeof(DSP_SAMPLE)) != 0)
        {
          allSame = false;
          firstMismatchBlock = b;
        }
      }
    }
    INFO("channels=" << numChannels << " first mismatch block=" << firstMismatchBlock);
    CHECK(allSame);
    // Both branches of the gain computation must have run, or the lock covers only one.
    CHECK(ref.belowKneeSamples > 1000);
    CHECK(ref.compressingSamples > 1000);
  }
}

TEST_CASE("VoLumPreEq noon settings preserve finite audio")
{
  dsp::effect::VoLumPreEq eq;
  eq.Reset(48000.0, 64);
  eq.SetParams(5.0, 5.0, 650.0, 5.0);

  std::vector<DSP_SAMPLE> samples(64, 0.1);
  DSP_SAMPLE* ptrs[] = {samples.data()};
  auto** out = eq.Process(ptrs, 1, samples.size());

  for (size_t i = 0; i < samples.size(); ++i)
    CHECK(std::isfinite(out[0][i]));
}

TEST_CASE("VoLumCompressor outputs finite bounded signal")
{
  dsp::effect::VoLumCompressor comp;
  comp.SetParams(8.0, 8.0, 2.0, 120.0, 1.0, 0.0, 48000.0);

  std::vector<DSP_SAMPLE> samples(128, 1.0);
  DSP_SAMPLE* ptrs[] = {samples.data()};
  auto** out = comp.Process(ptrs, 1, samples.size());

  double peak = 0.0;
  for (size_t i = 0; i < samples.size(); ++i)
  {
    CHECK(std::isfinite(out[0][i]));
    peak = std::max(peak, std::abs(static_cast<double>(out[0][i])));
  }
  CHECK(peak < 2.5);
}

TEST_CASE("VoLumCompressor dry mix passes input unchanged")
{
  dsp::effect::VoLumCompressor comp;
  comp.SetParams(10.0, 20.0, 0.1, 80.0, 0.0, 0.0, 48000.0);

  std::vector<DSP_SAMPLE> samples{0.0, 0.25, -0.5, 0.75, -1.0};
  DSP_SAMPLE* ptrs[] = {samples.data()};
  auto** out = comp.Process(ptrs, 1, samples.size());

  for (size_t i = 0; i < samples.size(); ++i)
    CHECK(out[0][i] == doctest::Approx(samples[i]));
}

TEST_CASE("VoLumCompressor amount reduces sustained loud signal")
{
  dsp::effect::VoLumCompressor comp;
  comp.SetParams(10.0, 20.0, 0.1, 80.0, 1.0, 0.0, 48000.0);

  std::vector<DSP_SAMPLE> samples(256, 1.0);
  DSP_SAMPLE* ptrs[] = {samples.data()};
  auto** out = comp.Process(ptrs, 1, samples.size());

  double peak = 0.0;
  for (size_t i = 0; i < samples.size(); ++i)
    peak = std::max(peak, std::abs(static_cast<double>(out[0][i])));

  CHECK(peak < 1.5);
  CHECK(out[0][samples.size() - 1] < 0.5);
}

// ---- 1176-style FET compressor (iteration 2) ----

TEST_CASE("VoLumCompressor 1176: fast attack rises faster than slow attack")
{
  // After a sudden burst of loud signal, faster attack should produce smaller wet output
  // (more gain reduction having taken effect) at the same time index.
  const size_t frames = 512;
  std::vector<DSP_SAMPLE> burst(frames, 0.9);
  DSP_SAMPLE* ptrs[] = {burst.data()};

  dsp::effect::VoLumCompressor fast;
  fast.SetParams(8.0, 4.0, /*attack*/ 0.05, /*release*/ 200.0, 1.0, 0.0, 48000.0);
  auto** outFast = fast.Process(ptrs, 1, frames);
  std::vector<DSP_SAMPLE> fastOut(outFast[0], outFast[0] + frames);

  // Reset input data because Process may have written into shared buffers.
  std::fill(burst.begin(), burst.end(), 0.9);

  dsp::effect::VoLumCompressor slow;
  slow.SetParams(8.0, 4.0, /*attack*/ 1.0, /*release*/ 200.0, 1.0, 0.0, 48000.0);
  auto** outSlow = slow.Process(ptrs, 1, frames);
  std::vector<DSP_SAMPLE> slowOut(outSlow[0], outSlow[0] + frames);

  // After a few ms (much longer than fast attack, comparable to slow attack), the fast
  // compressor should have pulled the wet signal lower than the slow one.
  const size_t checkIdx = static_cast<size_t>(48000.0 * 0.005); // ~5 ms in
  REQUIRE(checkIdx < frames);
  CHECK(std::abs(fastOut[checkIdx]) <= std::abs(slowOut[checkIdx]) + 1e-3);
}

TEST_CASE("VoLumCompressor 1176: ratio param is ignored (fixed 4:1)")
{
  // Two compressors with the same settings but different `ratio` arg should produce
  // identical output - the ratio parameter is locked internally.
  const size_t frames = 256;
  std::vector<DSP_SAMPLE> samples(frames, 0.6);
  DSP_SAMPLE* ptrsA[] = {samples.data()};

  dsp::effect::VoLumCompressor a;
  a.SetParams(6.0, 4.0, 0.5, 200.0, 1.0, 0.0, 48000.0);
  auto** outA = a.Process(ptrsA, 1, frames);
  std::vector<DSP_SAMPLE> aOut(outA[0], outA[0] + frames);

  std::fill(samples.begin(), samples.end(), 0.6);
  dsp::effect::VoLumCompressor b;
  b.SetParams(6.0, 20.0, 0.5, 200.0, 1.0, 0.0, 48000.0); // ratio differs - should be ignored
  auto** outB = b.Process(ptrsA, 1, frames);

  for (size_t i = 0; i < frames; ++i)
    CHECK(outB[0][i] == doctest::Approx(aOut[i]).epsilon(1e-6));
}

TEST_CASE("VoLumCompressor 1176: Output 0 dB uses hidden unity calibration")
{
  const size_t frames = 512;
  std::vector<DSP_SAMPLE> samples(frames, 0.35);
  DSP_SAMPLE* ptrs[] = {samples.data()};

  dsp::effect::VoLumCompressor unity;
  unity.SetParams(3.0, 4.0, 0.4, 250.0, 1.0, 0.0, 48000.0);
  auto** outUnity = unity.Process(ptrs, 1, frames);
  std::vector<DSP_SAMPLE> unityOut(outUnity[0], outUnity[0] + frames);

  std::fill(samples.begin(), samples.end(), 0.35);
  dsp::effect::VoLumCompressor plusFive;
  plusFive.SetParams(3.0, 4.0, 0.4, 250.0, 1.0, 5.0, 48000.0);
  auto** outPlusFive = plusFive.Process(ptrs, 1, frames);

  // The user-facing Output knob still displays 0 dB at default, but internally that
  // point is calibrated about 5 dB below the raw 1176 make-up stage.
  double rmsUnity = 0.0;
  double rmsPlusFive = 0.0;
  for (size_t i = 0; i < frames; ++i)
  {
    rmsUnity += static_cast<double>(unityOut[i]) * static_cast<double>(unityOut[i]);
    rmsPlusFive += static_cast<double>(outPlusFive[0][i]) * static_cast<double>(outPlusFive[0][i]);
  }
  rmsUnity = std::sqrt(rmsUnity / static_cast<double>(frames));
  rmsPlusFive = std::sqrt(rmsPlusFive / static_cast<double>(frames));

  CHECK(rmsUnity > 0.0);
  CHECK(rmsUnity / rmsPlusFive == doctest::Approx(std::pow(10.0, -5.0 / 20.0)).epsilon(1e-3));
}

TEST_CASE("VoLumCompressor 1176: Output minimum mutes")
{
  const size_t frames = 128;
  std::vector<DSP_SAMPLE> samples(frames, 0.35);
  DSP_SAMPLE* ptrs[] = {samples.data()};

  dsp::effect::VoLumCompressor comp;
  comp.SetParams(3.0, 4.0, 0.4, 250.0, 1.0, -20.0, 48000.0);
  auto** out = comp.Process(ptrs, 1, frames);

  for (size_t i = 0; i < frames; ++i)
    CHECK(out[0][i] == doctest::Approx(0.0));
}

TEST_CASE("VoLumCompressor 1176: FET pre-detector saturation present at high drive")
{
  // Feed a clean sine at high amplitude and cranked Input. Output should contain
  // measurable harmonic content (the FET nonlinearity at work).
  const double sr = 48000.0;
  const double freq = 440.0;
  const size_t frames = 4096;

  std::vector<DSP_SAMPLE> samples(frames);
  for (size_t i = 0; i < frames; ++i)
    samples[i] = static_cast<DSP_SAMPLE>(0.85 * std::sin(2.0 * M_PI * freq * i / sr));
  DSP_SAMPLE* ptrs[] = {samples.data()};

  dsp::effect::VoLumCompressor comp;
  comp.SetParams(10.0, 4.0, 0.4, 250.0, 1.0, 0.0, sr);
  auto** out = comp.Process(ptrs, 1, frames);

  // Check that the output is not a pure sine: difference from a least-squares-fit sine
  // at the fundamental should be non-trivial (i.e. harmonic content exists).
  double sumSinSq = 0.0, sumCosSq = 0.0, sumSinX = 0.0, sumCosX = 0.0;
  for (size_t i = 0; i < frames; ++i)
  {
    const double phase = 2.0 * M_PI * freq * i / sr;
    const double s = std::sin(phase);
    const double c = std::cos(phase);
    const double x = static_cast<double>(out[0][i]);
    sumSinSq += s * s;
    sumCosSq += c * c;
    sumSinX += s * x;
    sumCosX += c * x;
  }
  const double aFit = sumSinX / sumSinSq;
  const double bFit = sumCosX / sumCosSq;

  double residualEnergy = 0.0;
  double signalEnergy = 0.0;
  for (size_t i = 0; i < frames; ++i)
  {
    const double phase = 2.0 * M_PI * freq * i / sr;
    const double fit = aFit * std::sin(phase) + bFit * std::cos(phase);
    const double x = static_cast<double>(out[0][i]);
    residualEnergy += (x - fit) * (x - fit);
    signalEnergy += x * x;
  }
  const double thd = std::sqrt(residualEnergy / std::max(signalEnergy, 1.0e-12));
  // Expect at least ~1% non-fundamental content - FET saturation is meaningful at
  // high input drive on a nearly-full-scale sine.
  CHECK(thd > 0.01);
}

TEST_CASE("VoLumCompressor 1176: attack range covers 0.02 ms target")
{
  // Even at the new fast attack target, no NaN and bounded output.
  dsp::effect::VoLumCompressor comp;
  comp.SetParams(7.0, 4.0, 0.02, 50.0, 1.0, 0.0, 48000.0);

  std::vector<DSP_SAMPLE> samples(128, 0.7);
  DSP_SAMPLE* ptrs[] = {samples.data()};
  auto** out = comp.Process(ptrs, 1, samples.size());

  double peak = 0.0;
  for (size_t i = 0; i < samples.size(); ++i)
  {
    CHECK(std::isfinite(out[0][i]));
    peak = std::max(peak, std::abs(static_cast<double>(out[0][i])));
  }
  CHECK(peak < 2.5);
}
