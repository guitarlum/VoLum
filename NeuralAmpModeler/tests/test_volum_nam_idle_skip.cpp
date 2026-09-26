#include "third_party/doctest.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

#include "activations.h"
#include "dsp.h"
#include "get_dsp.h"
#include "slimmable.h"

#define VOLUM_DSP_STAGING_SKIP_WDL
#include "../VoLumDspStagingWdl.h"
#include "../VoLumResamplingNam.h"
#include "../architecture.hpp"
#if defined(ARCH_X86)
  #include <immintrin.h>
#endif

// ResamplingNAM stops running a feed-forward model once its input has been one
// constant value for longer than the receptive field, and repeats the last
// output instead. That must not change a single output bit: every case renders
// the same input through a skipping wrapper and an always-processing one and
// compares the bytes.

namespace
{
constexpr double kPi = 3.14159265358979323846;

class ScopedDenormalsOff
{
public:
  ScopedDenormalsOff()
  {
#if defined(ARCH_X86)
    mSaved = _mm_getcsr();
    disable_denormals();
#endif
  }
  ~ScopedDenormalsOff()
  {
#if defined(ARCH_X86)
    _mm_setcsr(mSaved);
#endif
  }
  ScopedDenormalsOff(const ScopedDenormalsOff&) = delete;
  ScopedDenormalsOff& operator=(const ScopedDenormalsOff&) = delete;

#if defined(ARCH_X86)
private:
  unsigned int mSaved = 0;
#endif
};

std::filesystem::path RepoRoot()
{
  return std::filesystem::path(__FILE__).parent_path().parent_path().parent_path();
}

std::filesystem::path FirstNam(const std::filesystem::path& dir, const std::string& prefix)
{
  std::vector<std::filesystem::path> hits;
  for (const auto& e : std::filesystem::directory_iterator(dir))
  {
    const std::string name = e.path().filename().string();
    if (e.path().extension() == ".nam" && name.rfind(prefix, 0) == 0)
      hits.push_back(e.path());
  }
  std::sort(hits.begin(), hits.end());
  const std::string missing = dir.string() + " has no " + prefix + "*.nam";
  REQUIRE_MESSAGE(!hits.empty(), missing);
  return hits.front();
}

std::filesystem::path SoldanoAmp()
{
  return FirstNam(RepoRoot() / "rigs" / "Soldano SLO100", "AMP-");
}

std::filesystem::path MythPedal()
{
  return FirstNam(RepoRoot() / "rigs" / "PrePedals", "FX-PettyJohn-Myth");
}

std::filesystem::path ExampleModel(const char* name)
{
  return RepoRoot() / "NeuralAmpModelerCore" / "example_models" / name;
}

std::unique_ptr<nam::DSP> Load(const std::filesystem::path& path)
{
  nam::activations::Activation::enable_fast_tanh();
  auto model = nam::get_dsp(path);
  REQUIRE_MESSAGE(model != nullptr, path.string());
  return model;
}

// Sits between ResamplingNAM and the real model: counts the frames the wrapper
// asks for, and can hide the receptive field so the wrapper never skips.
class SpyDsp : public nam::DSP, public nam::SlimmableModel
{
public:
  SpyDsp(std::unique_ptr<nam::DSP> inner, bool exposeReceptiveField)
  : nam::DSP(inner->NumInputChannels(), inner->NumOutputChannels(), inner->GetExpectedSampleRate())
  , mInner(std::move(inner))
  , mExpose(exposeReceptiveField)
  {
  }

  void process(NAM_SAMPLE** input, NAM_SAMPLE** output, const int num_frames) override
  {
    frames += num_frames;
    mInner->process(input, output, num_frames);
  }
  void prewarm() override { mInner->prewarm(); }
  void Reset(const double sampleRate, const int maxBufferSize) override { mInner->Reset(sampleRate, maxBufferSize); }
  int FeedForwardReceptiveField() override { return mExpose ? mInner->FeedForwardReceptiveField() : 0; }
  void SetSlimmableSize(const double val) override
  {
    if (auto* slim = dynamic_cast<nam::SlimmableModel*>(mInner.get()))
      slim->SetSlimmableSize(val);
  }

  long long frames = 0;

private:
  std::unique_ptr<nam::DSP> mInner;
  bool mExpose;
};

struct Wrapped
{
  std::unique_ptr<ResamplingNAM> nam;
  SpyDsp* spy = nullptr;
};

Wrapped Wrap(const std::filesystem::path& path, bool skip, bool full, double hostRate, int block)
{
  auto spy = std::make_unique<SpyDsp>(Load(path), skip);
  Wrapped w;
  w.spy = spy.get();
  w.nam = std::make_unique<ResamplingNAM>(std::move(spy), hostRate);
  w.nam->SetSlimmableSize(full ? 1.0 : 0.0);
  w.nam->Reset(hostRate, volum::dsp_staging::NamResetBlockSize(block));
  return w;
}

int HostBlockSize(int block, bool varied, int index)
{
  if (!varied)
    return block;
  const int cycle[] = {block, 1, block - 1, 37, 2 * block + 5, block / 2, 3};
  return cycle[index % 7];
}

struct Render
{
  std::vector<NAM_SAMPLE> out;
  // Model frames the wrapper processed during each host block.
  std::vector<int> blockStart;
  std::vector<long long> processed;
};

// Host blocks the way ProcessBlock hands them over (FTZ/DAZ on); a block past
// the Reset size is chunked inside ResamplingNAM::process.
Render RenderThrough(Wrapped& w, const std::vector<NAM_SAMPLE>& input, int block, bool varied = false)
{
  Render r;
  std::vector<NAM_SAMPLE> in(input);
  r.out.assign(input.size(), 0);
  const int total = static_cast<int>(input.size());
  int n = 0;
  for (int off = 0, index = 0; off < total; off += n, ++index)
  {
    n = std::min(HostBlockSize(block, varied, index), total - off);
    const long long before = w.spy->frames;
    {
      const ScopedDenormalsOff denormalsOff;
      w.nam->process(in.data() + off, r.out.data() + off, n);
    }
    r.blockStart.push_back(off);
    r.processed.push_back(w.spy->frames - before);
  }
  return r;
}

void AppendGuitar(std::vector<NAM_SAMPLE>& v, double seconds, double rate, double level)
{
  const int frames = static_cast<int>(seconds * rate);
  for (int i = 0; i < frames; ++i)
  {
    const double t = static_cast<double>(i) / rate;
    const double env = std::exp(-3.0 * t);
    const double s = std::sin(2.0 * kPi * 110.0 * t) + 0.5 * std::sin(2.0 * kPi * 220.0 * t + 0.3)
                     + 0.25 * std::sin(2.0 * kPi * 330.0 * t + 1.1);
    v.push_back(static_cast<NAM_SAMPLE>(level * env * s));
  }
}

void AppendConstant(std::vector<NAM_SAMPLE>& v, double seconds, double rate, NAM_SAMPLE value)
{
  v.insert(v.end(), static_cast<size_t>(seconds * rate), value);
}

struct Stretch
{
  int begin;
  int end;
};

// Signal, a long exact silence, a constant DC, signal again, then silence.
struct Program
{
  std::vector<NAM_SAMPLE> input;
  Stretch silence;
  Stretch dc;
};

Program SilenceDcProgram(double rate)
{
  Program p;
  AppendGuitar(p.input, 0.4, rate, 0.4);
  p.silence.begin = static_cast<int>(p.input.size());
  AppendConstant(p.input, 1.0, rate, 0.0);
  p.silence.end = static_cast<int>(p.input.size());
  p.dc.begin = p.silence.end;
  AppendConstant(p.input, 1.0, rate, 0.25);
  p.dc.end = static_cast<int>(p.input.size());
  AppendGuitar(p.input, 0.4, rate, 0.2);
  AppendConstant(p.input, 0.5, rate, 0.0);
  return p;
}

long long FirstMismatch(const std::vector<NAM_SAMPLE>& a, const std::vector<NAM_SAMPLE>& b)
{
  REQUIRE(a.size() == b.size());
  for (size_t i = 0; i < a.size(); ++i)
    if (std::memcmp(&a[i], &b[i], sizeof(NAM_SAMPLE)) != 0)
      return static_cast<long long>(i);
  return -1;
}

long long ProcessedTotal(const Render& r)
{
  long long sum = 0;
  for (const long long p : r.processed)
    sum += p;
  return sum;
}

// Model frames processed in host blocks that start at or after `from` and end
// by `to`.
long long ProcessedWithin(const Render& r, int from, int to, int total)
{
  long long sum = 0;
  for (size_t i = 0; i < r.blockStart.size(); ++i)
  {
    const int start = r.blockStart[i];
    const int end = i + 1 < r.blockStart.size() ? r.blockStart[i + 1] : total;
    if (start >= from && end <= to)
      sum += r.processed[i];
  }
  return sum;
}

struct SkipCase
{
  const char* name;
  std::filesystem::path path;
  bool full;
  double hostRate;
  int block;
  bool varied;
  bool expectDcSkip;
};
} // namespace

TEST_CASE("NAM idle skip: receptive field hook is set only on feed-forward models")
{
  auto amp = Load(SoldanoAmp());
  auto* slim = dynamic_cast<nam::SlimmableModel*>(amp.get());
  REQUIRE(slim != nullptr);
  slim->SetSlimmableSize(1.0);
  const int full = amp->FeedForwardReceptiveField();
  slim->SetSlimmableSize(0.0);
  const int lite = amp->FeedForwardReceptiveField();
  // 23 A2 layers plus the 16-tap head: about 132 ms at 48 kHz.
  CHECK(full > 6000);
  CHECK(full < 7000);
  CHECK(lite == full);

  // The generic WaveNet's constant output depends on the block size, so
  // repeating it would not match processing.
  CHECK(Load(ExampleModel("wavenet_a1_standard.nam"))->FeedForwardReceptiveField() == 0);
  CHECK(Load(ExampleModel("lstm.nam"))->FeedForwardReceptiveField() == 0);
  CHECK(Load(ExampleModel("wavenet_condition_dsp.nam"))->FeedForwardReceptiveField() == 0);
  CHECK(Load(ExampleModel("slimmable_wavenet.nam"))->FeedForwardReceptiveField() == 0);
}

TEST_CASE("NAM idle skip: output bits match always-process through silence and DC")
{
  const std::vector<SkipCase> cases = {
    {"Soldano FULL 48k/64", SoldanoAmp(), true, 48000.0, 64, false, true},
    {"Soldano LITE 48k/64", SoldanoAmp(), false, 48000.0, 64, false, true},
    {"Soldano FULL 48k/400 varied", SoldanoAmp(), true, 48000.0, 400, true, true},
    {"Soldano LITE 48k/400 varied", SoldanoAmp(), false, 48000.0, 400, true, true},
    {"Myth PRE FULL 48k/128", MythPedal(), true, 48000.0, 128, false, true},
    {"Soldano FULL 44.1k/64 resampled", SoldanoAmp(), true, 44100.0, 64, false, false},
    {"Soldano LITE 44.1k/256 resampled varied", SoldanoAmp(), false, 44100.0, 256, true, false},
  };
  for (const SkipCase& c : cases)
  {
    INFO(std::string(c.name));
    const Program p = SilenceDcProgram(c.hostRate);
    const int total = static_cast<int>(p.input.size());
    Wrapped always = Wrap(c.path, false, c.full, c.hostRate, c.block);
    Wrapped skipping = Wrap(c.path, true, c.full, c.hostRate, c.block);
    const Render ref = RenderThrough(always, p.input, c.block, c.varied);
    const Render got = RenderThrough(skipping, p.input, c.block, c.varied);

    CHECK(FirstMismatch(ref.out, got.out) == -1);
    CHECK(std::all_of(got.out.begin(), got.out.end(), [](NAM_SAMPLE v) { return std::isfinite(v); }));

    // Past the receptive field (plus the resampler's delay) the model sleeps.
    const int settle = static_cast<int>(0.25 * c.hostRate);
    CHECK(ProcessedWithin(ref, p.silence.begin + settle, p.silence.end, total) > 0);
    CHECK(ProcessedWithin(got, p.silence.begin + settle, p.silence.end, total) == 0);
    if (c.expectDcSkip)
      CHECK(ProcessedWithin(got, p.dc.begin + settle, p.dc.end, total) == 0);
    CHECK(ProcessedTotal(got) < ProcessedTotal(ref));
  }
}

TEST_CASE("NAM idle skip: the amp sleeps on a PRE NAM's DC output")
{
  // A PRE NAM turns silence into a DC constant, not zeros; the amp behind it
  // must still stop, and still match always-process.
  const double rate = 48000.0;
  const int block = 64;
  std::vector<NAM_SAMPLE> input;
  AppendGuitar(input, 0.4, rate, 0.4);
  const int silentFrom = static_cast<int>(input.size());
  AppendConstant(input, 1.0, rate, 0.0);
  const int silentTo = static_cast<int>(input.size());
  AppendGuitar(input, 0.4, rate, 0.3);

  Wrapped pedal = Wrap(MythPedal(), true, true, rate, block);
  const Render pre = RenderThrough(pedal, input, block);
  const NAM_SAMPLE dc = pre.out[static_cast<size_t>(silentTo - 1)];
  CHECK(dc != 0);
  CHECK(pre.out[static_cast<size_t>(silentTo - 2)] == dc);

  Wrapped always = Wrap(SoldanoAmp(), false, true, rate, block);
  Wrapped skipping = Wrap(SoldanoAmp(), true, true, rate, block);
  const Render ref = RenderThrough(always, pre.out, block);
  const Render got = RenderThrough(skipping, pre.out, block);
  CHECK(FirstMismatch(ref.out, got.out) == -1);
  const int total = static_cast<int>(input.size());
  CHECK(ProcessedWithin(got, silentFrom + static_cast<int>(0.4 * rate), silentTo, total) == 0);
}

TEST_CASE("NAM idle skip: Reset and a slice change restart the run")
{
  const double rate = 48000.0;
  const int block = 64;
  const int reserve = volum::dsp_staging::NamResetBlockSize(block);
  std::vector<NAM_SAMPLE> silence;
  AppendConstant(silence, 0.5, rate, 0.0);
  std::vector<NAM_SAMPLE> program;
  AppendGuitar(program, 0.2, rate, 0.4);
  AppendConstant(program, 0.5, rate, 0.0);

  Wrapped always = Wrap(SoldanoAmp(), false, true, rate, block);
  Wrapped skipping = Wrap(SoldanoAmp(), true, true, rate, block);
  std::vector<Render> ref, got;
  for (Wrapped* w : {&always, &skipping})
  {
    auto& out = (w == &always) ? ref : got;
    out.push_back(RenderThrough(*w, program, block));
    w->nam->SetSlimmableSize(0.0);
    out.push_back(RenderThrough(*w, silence, block));
    out.push_back(RenderThrough(*w, program, block));
    w->nam->Reset(rate, reserve);
    out.push_back(RenderThrough(*w, silence, block));
  }
  for (size_t i = 0; i < ref.size(); ++i)
  {
    INFO("segment " << i);
    CHECK(FirstMismatch(ref[i].out, got[i].out) == -1);
  }
  // Right after the slice change and after Reset the new state is processed,
  // not replaced by the old slice's settled output.
  CHECK(got[1].processed.front() == block);
  CHECK(got[3].processed.front() == block);
  CHECK(got[0].processed.back() == 0);
  CHECK(got[2].processed.back() == 0);
}

TEST_CASE("NAM idle skip: LSTM and generic WaveNet models always process")
{
  const double rate = 48000.0;
  const int block = 64;
  std::vector<NAM_SAMPLE> silence;
  AppendConstant(silence, 0.5, rate, 0.0);
  for (const char* name : {"lstm.nam", "wavenet_a1_standard.nam"})
  {
    INFO(name);
    Wrapped w = Wrap(ExampleModel(name), true, true, rate, block);
    const Render r = RenderThrough(w, silence, block);
    CHECK(ProcessedTotal(r) == static_cast<long long>(silence.size()));
  }
}

TEST_CASE("NAM idle skip: 10 s of silence, skipping vs always-process" * doctest::skip())
{
  const double rate = 48000.0;
  const int block = 64;
  std::vector<NAM_SAMPLE> silence;
  AppendConstant(silence, 10.0, rate, 0.0);
  for (const bool full : {true, false})
  {
    for (const bool skip : {false, true})
    {
      Wrapped w = Wrap(SoldanoAmp(), skip, full, rate, block);
      const auto t0 = std::chrono::steady_clock::now();
      const Render r = RenderThrough(w, silence, block);
      const double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
      const std::string label = std::string(full ? "FULL" : "LITE") + (skip ? " skipping: " : " always-process: ");
      MESSAGE(label << ms << " ms, " << ProcessedTotal(r) << " model frames");
    }
  }
}
