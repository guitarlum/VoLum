#include "third_party/doctest.h"

#include "../ToneStack.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <limits>
#include <random>
#include <vector>

// recursive_linear_filter::Base::Process runs fixed-degree kernels for the filters VoLum uses (biquads,
// high-pass, low-pass, level: the tone stack, PRE EQ, PRE levels, IR cuts and the DC high-pass). The sound
// contract is exact: every kernel must leave the same output bits and the same history rings as the
// any-degree loop, which Base keeps as _ProcessGeneric and which is the reference here.

#if defined(__SANITIZE_ADDRESS__)
  #define VOLUM_FILTER_CORE_SANITIZED 1
#elif defined(__has_feature)
  #if __has_feature(address_sanitizer) || __has_feature(thread_sanitizer) || __has_feature(undefined_behavior_sanitizer)
    #define VOLUM_FILTER_CORE_SANITIZED 1
  #endif
#endif

namespace
{
using recursive_linear_filter::HighPass;
using recursive_linear_filter::HighShelf;
using recursive_linear_filter::Level;
using recursive_linear_filter::LowPass;
using recursive_linear_filter::LowShelf;
using recursive_linear_filter::Peaking;

#if !defined(NDEBUG) || defined(VOLUM_FILTER_CORE_SANITIZED) || (defined(__GNUC__) && !defined(__OPTIMIZE__))
constexpr bool kSkipTiming = true;
#else
constexpr bool kSkipTiming = false;
#endif

constexpr size_t kMaxBlock = 512;

bool SameBits(const DSP_SAMPLE* a, const DSP_SAMPLE* b, size_t count)
{
  return count == 0 || std::memcmp(a, b, count * sizeof(DSP_SAMPLE)) == 0;
}

bool SameRings(const std::vector<std::vector<DSP_SAMPLE>>& a, const std::vector<std::vector<DSP_SAMPLE>>& b)
{
  if (a.size() != b.size())
    return false;
  for (size_t c = 0; c < a.size(); ++c)
    if (a[c].size() != b[c].size() || !SameBits(a[c].data(), b[c].data(), a[c].size()))
      return false;
  return true;
}

// Opens the reference path, the coefficients and the history rings of any filter.
template <class Filter>
class Probe : public Filter
{
public:
  template <class... Args>
  explicit Probe(Args... args)
  : Filter(args...)
  {
  }

  DSP_SAMPLE** ProcessGeneric(DSP_SAMPLE** inputs, size_t numChannels, size_t numFrames)
  {
    return this->_ProcessGeneric(inputs, numChannels, numFrames);
  }

  std::vector<double>& InputCoefficients() { return this->mInputCoefficients; }
  std::vector<double>& OutputCoefficients() { return this->mOutputCoefficients; }

  bool SameStateAs(const Probe& other) const
  {
    return this->mInputStart == other.mInputStart && this->mOutputStart == other.mOutputStart
           && SameRings(this->mInputHistory, other.mInputHistory)
           && SameRings(this->mOutputHistory, other.mOutputHistory);
  }
};

class RawFilter : public recursive_linear_filter::Base
{
public:
  RawFilter(size_t inputDegree, size_t outputDegree)
  : Base(inputDegree, outputDegree)
  {
  }
};

// Random coefficients: mostly stable feedback (poles inside the unit circle), sometimes unstable so the
// output overflows to inf and then NaN, which exercises the NaN scrub. The unused output coefficient [0]
// is NaN: neither path may read it.
template <class Filter>
void RandomCoefficients(Probe<Filter>& probe, std::mt19937& rng)
{
  std::uniform_real_distribution<double> unit(-1.0, 1.0);
  for (double& c : probe.InputCoefficients())
    c = 2.0 * unit(rng);
  std::vector<double>& out = probe.OutputCoefficients();
  const bool unstable = rng() % 8 == 0;
  if (!out.empty())
    out[0] = std::numeric_limits<double>::quiet_NaN();
  if (out.size() == 2)
    out[1] = unstable ? 1.0 + std::abs(unit(rng)) : 0.999 * unit(rng);
  else if (out.size() >= 3)
  {
    const double radius = unstable ? 1.0 + 0.2 * std::abs(unit(rng)) : 0.999 * std::abs(unit(rng));
    const double angle = 3.14159265358979323846 * unit(rng);
    out[1] = 2.0 * radius * std::cos(angle);
    out[2] = -radius * radius;
    for (size_t i = 3; i < out.size(); ++i)
      out[i] = 0.1 * unit(rng);
  }
}

DSP_SAMPLE RandomSample(std::mt19937& rng)
{
  const uint32_t pick = rng() % 200;
  if (pick == 0)
    return std::numeric_limits<DSP_SAMPLE>::quiet_NaN();
  if (pick == 1)
    return std::numeric_limits<DSP_SAMPLE>::infinity();
  if (pick == 2)
    return -std::numeric_limits<DSP_SAMPLE>::infinity();
  if (pick == 3)
    return static_cast<DSP_SAMPLE>(-0.0);
  if (pick == 4)
    return std::numeric_limits<DSP_SAMPLE>::denorm_min();
  std::uniform_real_distribution<double> unit(-1.0, 1.0);
  return static_cast<DSP_SAMPLE>(unit(rng));
}

struct Counts
{
  int blocks = 0;
  int outputMismatches = 0;
  int stateMismatches = 0;
};

// Three copies of one filter take the same blocks: `fixed` through Process, `generic` through the reference,
// `mixed` alternating between the two so a kernel continues from rings the reference left and vice versa.
// Block sizes vary 0..512 every block and the channel count sometimes flips between 1 and 2.
constexpr int kBlocksPerSet = 24;

template <class Filter, class Setup, class... Args>
void RunEquivalence(std::mt19937& rng, const Setup& setup, Counts& counts, Args... filterArgs)
{
  std::vector<std::vector<DSP_SAMPLE>> buffers(2, std::vector<DSP_SAMPLE>(kMaxBlock));
  Probe<Filter> fixed(filterArgs...), generic(filterArgs...), mixed(filterArgs...);
  for (Probe<Filter>* p : {&fixed, &generic, &mixed})
    setup(*p);
  size_t numChannels = 1 + rng() % 2;
  for (int block = 0; block < kBlocksPerSet; ++block)
  {
    if (rng() % 10 == 0)
      numChannels = 3 - numChannels;
    const size_t numFrames = rng() % 16 == 0 ? rng() % 4 : 1 + rng() % kMaxBlock;
    DSP_SAMPLE* inputs[2] = {buffers[0].data(), buffers[1].data()};
    for (size_t c = 0; c < numChannels; ++c)
      for (size_t s = 0; s < numFrames; ++s)
        buffers[c][s] = RandomSample(rng);

    DSP_SAMPLE** outFixed = fixed.Process(inputs, numChannels, numFrames);
    DSP_SAMPLE** outGeneric = generic.ProcessGeneric(inputs, numChannels, numFrames);
    DSP_SAMPLE** outMixed = block % 2 == 0 ? mixed.Process(inputs, numChannels, numFrames)
                                           : mixed.ProcessGeneric(inputs, numChannels, numFrames);
    ++counts.blocks;
    for (size_t c = 0; c < numChannels; ++c)
      if (!SameBits(outFixed[c], outGeneric[c], numFrames) || !SameBits(outMixed[c], outGeneric[c], numFrames))
      {
        ++counts.outputMismatches;
        break;
      }
    if (!fixed.SameStateAs(generic) || !mixed.SameStateAs(generic))
      ++counts.stateMismatches;
  }
}

void CheckRandomCoefficients(size_t inputDegree, size_t outputDegree, uint32_t seed)
{
  constexpr int kSets = 60;
  std::mt19937 rng(seed);
  Counts counts;
  for (int set = 0; set < kSets; ++set)
  {
    Probe<RawFilter> shape(inputDegree, outputDegree);
    RandomCoefficients(shape, rng);
    const std::vector<double> in = shape.InputCoefficients();
    const std::vector<double> out = shape.OutputCoefficients();
    RunEquivalence<RawFilter>(
      rng,
      [&](Probe<RawFilter>& p) {
        p.InputCoefficients() = in;
        p.OutputCoefficients() = out;
      },
      counts, inputDegree, outputDegree);
  }
  INFO(counts.blocks << " blocks");
  CHECK(counts.blocks == kSets * kBlocksPerSet);
  CHECK(counts.outputMismatches == 0);
  CHECK(counts.stateMismatches == 0);
}
} // namespace

TEST_CASE("Filter core: fixed-degree kernels match the any-degree loop bit for bit, random coefficients")
{
  // The dispatched degree pairs: biquad (3,3), high-pass (2,2), low-pass (1,2), level (1,0).
  SUBCASE("biquad (3,3)")
  {
    CheckRandomCoefficients(3, 3, 101u);
  }
  SUBCASE("high-pass (2,2)")
  {
    CheckRandomCoefficients(2, 2, 202u);
  }
  SUBCASE("low-pass (1,2)")
  {
    CheckRandomCoefficients(1, 2, 303u);
  }
  SUBCASE("level (1,0)")
  {
    CheckRandomCoefficients(1, 0, 404u);
  }
  // Pairs that stay on the any-degree loop.
  SUBCASE("other degrees")
  {
    CheckRandomCoefficients(4, 4, 505u);
    CheckRandomCoefficients(3, 0, 606u);
    CheckRandomCoefficients(2, 3, 707u);
  }
}

TEST_CASE("Filter core: VoLum's filter classes match the any-degree loop bit for bit")
{
  std::mt19937 rng(8080u);
  std::uniform_real_distribution<double> unit(0.0, 1.0);
  Counts counts;
  const double sampleRates[] = {44100.0, 48000.0, 96000.0};
  for (int set = 0; set < 20; ++set)
  {
    const double sampleRate = sampleRates[set % 3];
    const double frequency = 20.0 + 15000.0 * unit(rng) * unit(rng);
    const double quality = 0.3 + 3.0 * unit(rng);
    const double gainDB = -24.0 + 48.0 * unit(rng);
    const recursive_linear_filter::BiquadParams biquad(sampleRate, frequency, quality, gainDB);
    RunEquivalence<LowShelf>(rng, [&](Probe<LowShelf>& p) { p.SetParams(biquad); }, counts);
    RunEquivalence<Peaking>(rng, [&](Probe<Peaking>& p) { p.SetParams(biquad); }, counts);
    RunEquivalence<HighShelf>(rng, [&](Probe<HighShelf>& p) { p.SetParams(biquad); }, counts);
    const recursive_linear_filter::HighPassParams highPass(sampleRate, frequency);
    RunEquivalence<HighPass>(rng, [&](Probe<HighPass>& p) { p.SetParams(highPass); }, counts);
    const recursive_linear_filter::LowPassParams lowPass(sampleRate, frequency);
    RunEquivalence<LowPass>(rng, [&](Probe<LowPass>& p) { p.SetParams(lowPass); }, counts);
    const recursive_linear_filter::LevelParams level(std::pow(10.0, gainDB / 20.0));
    RunEquivalence<Level>(rng, [&](Probe<Level>& p) { p.SetParams(level); }, counts);
  }
  INFO(counts.blocks << " blocks");
  CHECK(counts.blocks == 20 * 6 * kBlocksPerSet);
  CHECK(counts.outputMismatches == 0);
  CHECK(counts.stateMismatches == 0);
}

TEST_CASE("Filter core: tone stack biquads cost at most half the any-degree loop" * doctest::skip(kSkipTiming))
{
  // The BasicNamToneStack chain (bass low shelf, mid peaking, treble high shelf) at bass 7, middle 4,
  // treble 6, one channel, 64-frame blocks at 48 kHz.
  constexpr size_t kBlock = 64;
  constexpr int kBlocksPerRound = 500;
  constexpr int kRounds = 41;
  constexpr double kSampleRate = 48000.0;
  const recursive_linear_filter::BiquadParams bass(kSampleRate, 150.0, 0.707, 4.0 * (7.0 - 5.0));
  const recursive_linear_filter::BiquadParams mid(kSampleRate, 425.0, 1.5, 3.0 * (4.0 - 5.0));
  const recursive_linear_filter::BiquadParams treble(kSampleRate, 1800.0, 0.707, 2.0 * (6.0 - 5.0));

  struct Chain
  {
    Probe<LowShelf> bass;
    Probe<Peaking> mid;
    Probe<HighShelf> treble;
  };
  Chain fixed, generic;
  for (Chain* chain : {&fixed, &generic})
  {
    chain->bass.SetParams(bass);
    chain->mid.SetParams(mid);
    chain->treble.SetParams(treble);
  }

  std::vector<DSP_SAMPLE> input(kBlock);
  std::mt19937 rng(64u);
  std::uniform_real_distribution<double> unit(-0.5, 0.5);
  for (DSP_SAMPLE& x : input)
    x = static_cast<DSP_SAMPLE>(unit(rng));
  DSP_SAMPLE* inputs[1] = {input.data()};
  volatile double sink = 0.0;

  auto timeFixed = [&]() {
    const auto t0 = std::chrono::steady_clock::now();
    for (int b = 0; b < kBlocksPerRound; ++b)
    {
      DSP_SAMPLE** out = fixed.bass.Process(inputs, 1, kBlock);
      out = fixed.mid.Process(out, 1, kBlock);
      out = fixed.treble.Process(out, 1, kBlock);
      sink = sink + out[0][kBlock - 1];
    }
    return std::chrono::duration<double, std::micro>(std::chrono::steady_clock::now() - t0).count() / kBlocksPerRound;
  };
  auto timeGeneric = [&]() {
    const auto t0 = std::chrono::steady_clock::now();
    for (int b = 0; b < kBlocksPerRound; ++b)
    {
      DSP_SAMPLE** out = generic.bass.ProcessGeneric(inputs, 1, kBlock);
      out = generic.mid.ProcessGeneric(out, 1, kBlock);
      out = generic.treble.ProcessGeneric(out, 1, kBlock);
      sink = sink + out[0][kBlock - 1];
    }
    return std::chrono::duration<double, std::micro>(std::chrono::steady_clock::now() - t0).count() / kBlocksPerRound;
  };

  timeFixed();
  timeGeneric();
  std::vector<double> fixedUs, genericUs;
  for (int round = 0; round < kRounds; ++round)
  {
    if (round % 2 == 0)
    {
      fixedUs.push_back(timeFixed());
      genericUs.push_back(timeGeneric());
    }
    else
    {
      genericUs.push_back(timeGeneric());
      fixedUs.push_back(timeFixed());
    }
  }
  // Preemption and a busy sibling core only ever add time, so each path's fastest round is its cost.
  const double fixedBest = *std::min_element(fixedUs.begin(), fixedUs.end());
  const double genericBest = *std::min_element(genericUs.begin(), genericUs.end());
  const double ratio = fixedBest / std::max(1e-6, genericBest);
  MESSAGE("tone stack, 64 frames: fixed kernels " << fixedBest << " us, any-degree loop " << genericBest
                                                  << " us per block (best of " << kRounds << " rounds); ratio "
                                                  << ratio);
#if defined(_M_X64) || defined(__x86_64__)
  CHECK(ratio <= 0.5);
#else
  // Measured on x86-64 only so far; 64-bit division is much cheaper on Apple silicon, so the ratio is only
  // printed here until a CI run pins it.
#endif
}
