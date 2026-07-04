// VoLum Android — M0 native smoke.
//
// Proves the shared NAM/AudioDSPTools DSP core builds and runs under the NDK
// (arm64-v8a on the S20). Loads a .nam model, resets/prewarms it, processes a
// number of blocks with a test signal, and reports timing so we get a first
// real-time-factor number on the target CPU.
//
// This translation unit is intentionally the only Android-specific file in M0.
// Later milestones add the Oboe host, the IGraphicsAndroid backend, and the JNI
// UI bridge alongside it.

#include <jni.h>
#include <android/log.h>

#include <chrono>
#include <cmath>
#include <memory>
#include <string>
#include <vector>

#include <mutex>

#include <sstream>

#include "dsp.h"
#include "get_dsp.h"
#include "audio/OboeAudioHost.h"
#include "engine/VolumMobileEngine.h"

#define LOG_TAG "VoLumNative"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

namespace
{
struct SmokeResult
{
  bool ok = false;
  std::string message;
};

SmokeResult RunNamSmoke(const std::string& path, double sampleRate, int blockSize, int numBlocks)
{
  SmokeResult r;
  std::unique_ptr<nam::DSP> model;
  try
  {
    model = nam::get_dsp(std::filesystem::path(path));
  }
  catch (const std::exception& e)
  {
    r.message = std::string("get_dsp threw: ") + e.what();
    return r;
  }
  if (!model)
  {
    r.message = "get_dsp returned null";
    return r;
  }

  model->ResetAndPrewarm(sampleRate, blockSize);

  std::vector<NAM_SAMPLE> in(static_cast<size_t>(blockSize));
  std::vector<NAM_SAMPLE> out(static_cast<size_t>(blockSize));
  NAM_SAMPLE* inPtr[1] = {in.data()};
  NAM_SAMPLE* outPtr[1] = {out.data()};

  // A quiet-ish test signal (small sine) — exercises the model without clipping.
  const double twoPiF = 2.0 * M_PI * 220.0 / sampleRate;
  long long sampleIdx = 0;

  bool sawNonFinite = false;
  double peak = 0.0;

  const auto t0 = std::chrono::steady_clock::now();
  for (int b = 0; b < numBlocks; ++b)
  {
    for (int i = 0; i < blockSize; ++i)
      in[static_cast<size_t>(i)] = 0.1 * std::sin(twoPiF * static_cast<double>(sampleIdx++));

    model->process(inPtr, outPtr, blockSize);

    for (int i = 0; i < blockSize; ++i)
    {
      const double v = static_cast<double>(out[static_cast<size_t>(i)]);
      if (!std::isfinite(v))
        sawNonFinite = true;
      const double a = std::fabs(v);
      if (a > peak)
        peak = a;
    }
  }
  const auto t1 = std::chrono::steady_clock::now();

  const double elapsedSec = std::chrono::duration<double>(t1 - t0).count();
  const double audioSec = static_cast<double>(numBlocks) * static_cast<double>(blockSize) / sampleRate;
  const double rtFactor = elapsedSec > 0.0 ? audioSec / elapsedSec : 0.0;

  char buf[512];
  std::snprintf(buf, sizeof(buf),
                "OK sr=%.0f block=%d blocks=%d\nprocessed=%.3fs cpu=%.3fs\nRT factor=%.2fx (>1 = faster than realtime)\npeak=%.4f finite=%s",
                sampleRate, blockSize, numBlocks, audioSec, elapsedSec, rtFactor, peak,
                sawNonFinite ? "NO" : "yes");
  r.ok = !sawNonFinite;
  r.message = buf;
  LOGI("%s", r.message.c_str());
  return r;
}
} // namespace

extern "C" JNIEXPORT jstring JNICALL Java_com_lum_volum_NativeBridge_nativeNamSmoke(
  JNIEnv* env, jobject /*thiz*/, jstring jpath, jdouble sampleRate, jint blockSize, jint numBlocks)
{
  const char* cpath = env->GetStringUTFChars(jpath, nullptr);
  const std::string path = cpath ? cpath : "";
  if (cpath)
    env->ReleaseStringUTFChars(jpath, cpath);

  const SmokeResult res = RunNamSmoke(path, static_cast<double>(sampleRate), static_cast<int>(blockSize),
                                      static_cast<int>(numBlocks));
  const std::string prefix = res.ok ? "" : "FAIL: ";
  return env->NewStringUTF((prefix + res.message).c_str());
}

extern "C" JNIEXPORT jstring JNICALL Java_com_lum_volum_NativeBridge_nativeVersion(JNIEnv* env, jobject /*thiz*/)
{
  return env->NewStringUTF("VoLum native core (DSP smoke + Oboe engine + POST chain)");
}

// --- Deterministic engine self-test (instrumented, device-independent) -------
// Drives VolumMobileEngine directly with synthetic buffers and asserts the
// signal-chain contract: bypass identity, unity passthrough, output gain law,
// noise-gate finiteness, model finiteness, and that Delay/Reverb produce a decay
// tail and Tremolo modulates the envelope. Returns "PASS ..." or "FAIL: ...".
namespace
{
double BlockRms(const float* interleaved, int frames)
{
  double acc = 0.0;
  for (int i = 0; i < frames; ++i)
  {
    const double l = interleaved[i * 2];
    acc += l * l;
  }
  return std::sqrt(acc / std::max(1, frames));
}

std::string RunEngineSelfTest(const std::string& modelPath)
{
  using volum::mobile::VolumMobileEngine;
  const double sr = 48000.0;
  const int block = 512;

  std::vector<float> in(static_cast<size_t>(block), 0.0f);
  std::vector<float> out(static_cast<size_t>(block) * 2, 0.0f);
  auto sine = [&](double freq, double amp) {
    const double w = 2.0 * M_PI * freq / sr;
    for (int i = 0; i < block; ++i)
      in[static_cast<size_t>(i)] = static_cast<float>(amp * std::sin(w * i));
  };
  auto allFinite = [&](int frames) {
    for (int i = 0; i < frames * 2; ++i)
      if (!std::isfinite(out[static_cast<size_t>(i)]))
        return false;
    return true;
  };

  std::vector<std::string> fails;
  auto check = [&](bool cond, const char* name) {
    if (!cond)
      fails.emplace_back(name);
  };

  VolumMobileEngine eng;
  eng.prepare(sr, block);
  eng.setGate(false, -80.0);
  eng.setToneEnabled(false);

  // 1) Bypass is a bit-exact identity (mono -> both channels).
  eng.setBypass(true);
  sine(220.0, 0.3);
  eng.process(in.data(), out.data(), block);
  {
    bool identity = true;
    for (int i = 0; i < block; ++i)
      identity &= (out[i * 2] == in[static_cast<size_t>(i)]) && (out[i * 2 + 1] == in[static_cast<size_t>(i)]);
    check(identity, "bypass_identity");
  }

  // 2) Dry passthrough (no model) is ~unity and finite.
  eng.setBypass(false);
  eng.setInputGainDb(0.0);
  eng.setOutputGainDb(0.0);
  sine(500.0, 0.2);
  eng.process(in.data(), out.data(), block);
  const double dryRms = BlockRms(out.data(), block);
  check(allFinite(block), "passthrough_finite");
  check(dryRms > 0.10 && dryRms < 0.20, "passthrough_unity");

  // 3) +6 dB output gain ~doubles level.
  eng.setOutputGainDb(6.0206);
  eng.process(in.data(), out.data(), block);
  const double loudRms = BlockRms(out.data(), block);
  check(loudRms > dryRms * 1.8 && loudRms < dryRms * 2.2, "output_gain_law");
  eng.setOutputGainDb(0.0);

  // 4) Noise gate keeps output finite and does not blow up.
  eng.setGate(true, -60.0);
  sine(300.0, 0.25);
  eng.process(in.data(), out.data(), block);
  check(allFinite(block), "gate_finite");
  eng.setGate(false, -80.0);

  // 5) Model loads and produces finite, non-silent output.
  if (!modelPath.empty())
  {
    const std::string err = eng.loadModel(modelPath);
    check(err.empty(), "model_load");
    sine(220.0, 0.2);
    eng.process(in.data(), out.data(), block);
    check(allFinite(block), "model_finite");
    check(BlockRms(out.data(), block) > 1e-5, "model_nonsilent");
    eng.clearModel();
  }

  // 6) Reverb produces a decay tail after the input stops.
  eng.setReverb(true, 0.6, 4.0, 5.0, 20.0, 0.5, 0 /*Hall*/, 0);
  sine(440.0, 0.3);
  eng.process(in.data(), out.data(), block);
  std::fill(in.begin(), in.end(), 0.0f);
  double reverbTail = 0.0;
  bool reverbFinite = true;
  for (int b = 0; b < 20; ++b)
  {
    eng.process(in.data(), out.data(), block);
    reverbFinite &= allFinite(block);
    reverbTail = std::max(reverbTail, BlockRms(out.data(), block));
  }
  check(reverbFinite, "reverb_finite");
  check(reverbTail > 1e-5, "reverb_tail");
  eng.setReverb(false, 0.3, 3.0, 4.5, 20.0, 0.5, 0, 0);

  // 7) Delay repeats energy after the input stops.
  eng.setDelay(true, 120.0, 0.5, 0.6, 0 /*Digital*/, 0.5, 0.0, false);
  sine(440.0, 0.3);
  eng.process(in.data(), out.data(), block);
  std::fill(in.begin(), in.end(), 0.0f);
  double delayTail = 0.0;
  bool delayFinite = true;
  for (int b = 0; b < 40; ++b)
  {
    eng.process(in.data(), out.data(), block);
    delayFinite &= allFinite(block);
    delayTail = std::max(delayTail, BlockRms(out.data(), block));
  }
  check(delayFinite, "delay_finite");
  check(delayTail > 1e-5, "delay_repeat");
  eng.setDelay(false, 380.0, 0.35, 0.28, 0, 0.5, 0.0, false);

  // 8) Tremolo modulates the envelope (block RMS varies over the LFO cycle).
  eng.setTremolo(true, 8.0, 1.0, 0.0, 1.0, 800.0, 1 /*Bias*/);
  double minR = 1e9, maxR = 0.0;
  bool tremFinite = true;
  for (int b = 0; b < 200; ++b)
  {
    sine(600.0, 0.3);
    eng.process(in.data(), out.data(), block);
    tremFinite &= allFinite(block);
    const double r = BlockRms(out.data(), block);
    minR = std::min(minR, r);
    maxR = std::max(maxR, r);
  }
  check(tremFinite, "tremolo_finite");
  check(maxR > 0.0 && (maxR - minR) / maxR > 0.10, "tremolo_modulates");
  eng.setTremolo(false, 5.0, 0.5, 0.0, 1.0, 800.0, 1);

  // 9) Tuner detects a known pitch (A4 = 440 Hz) within a few cents.
  eng.setTunerEnabled(true);
  double tunerHz = 0.0;
  double tphase = 0.0;
  const double tw = 2.0 * M_PI * 440.0 / sr; // A4, phase-continuous across blocks
  for (int b = 0; b < 80; ++b) // ~0.85 s of audio: several detection windows
  {
    for (int i = 0; i < block; ++i)
    {
      in[static_cast<size_t>(i)] = static_cast<float>(0.3 * std::sin(tphase));
      tphase += tw;
      if (tphase > 2.0 * M_PI)
        tphase -= 2.0 * M_PI;
    }
    eng.process(in.data(), out.data(), block);
    const float hz = eng.tunerHz();
    if (hz > 0.0f)
      tunerHz = hz;
  }
  check(tunerHz > 430.0 && tunerHz < 450.0, "tuner_a4");
  eng.setTunerEnabled(false);

  if (fails.empty())
    return "PASS engine self-test (9 groups)";
  std::ostringstream os;
  os << "FAIL:";
  for (const auto& f : fails)
    os << ' ' << f;
  return os.str();
}
} // namespace

extern "C" JNIEXPORT jstring JNICALL Java_com_lum_volum_NativeBridge_nativeEngineSelfTest(JNIEnv* env, jobject /*thiz*/,
                                                                                          jstring jpath)
{
  const char* cpath = env->GetStringUTFChars(jpath, nullptr);
  const std::string path = cpath ? cpath : "";
  if (cpath)
    env->ReleaseStringUTFChars(jpath, cpath);
  const std::string result = RunEngineSelfTest(path);
  LOGI("%s", result.c_str());
  return env->NewStringUTF(result.c_str());
}

// --- M2: real-time audio host --------------------------------------------
namespace
{
std::mutex gHostMutex;
std::unique_ptr<volum::mobile::OboeAudioHost> gHost;

volum::mobile::OboeAudioHost& Host()
{
  // gHostMutex must be held by callers that (re)create the host.
  if (!gHost)
    gHost = std::make_unique<volum::mobile::OboeAudioHost>();
  return *gHost;
}
} // namespace

extern "C" JNIEXPORT jboolean JNICALL Java_com_lum_volum_NativeBridge_nativeAudioStart(
  JNIEnv* /*env*/, jobject /*thiz*/, jint inputDeviceId, jint outputDeviceId, jint sampleRate)
{
  std::lock_guard<std::mutex> lock(gHostMutex);
  return Host().startStreams(static_cast<int>(inputDeviceId), static_cast<int>(outputDeviceId),
                             static_cast<int>(sampleRate), 0)
           ? JNI_TRUE
           : JNI_FALSE;
}

extern "C" JNIEXPORT void JNICALL Java_com_lum_volum_NativeBridge_nativeAudioStop(JNIEnv* /*env*/, jobject /*thiz*/)
{
  std::lock_guard<std::mutex> lock(gHostMutex);
  if (gHost)
    gHost->stopStreams();
}

extern "C" JNIEXPORT jboolean JNICALL Java_com_lum_volum_NativeBridge_nativeAudioIsRunning(JNIEnv* /*env*/,
                                                                                           jobject /*thiz*/)
{
  std::lock_guard<std::mutex> lock(gHostMutex);
  return (gHost && gHost->isRunning()) ? JNI_TRUE : JNI_FALSE;
}

extern "C" JNIEXPORT jstring JNICALL Java_com_lum_volum_NativeBridge_nativeLoadModel(JNIEnv* env, jobject /*thiz*/,
                                                                                     jstring jpath)
{
  const char* cpath = env->GetStringUTFChars(jpath, nullptr);
  const std::string path = cpath ? cpath : "";
  if (cpath)
    env->ReleaseStringUTFChars(jpath, cpath);

  std::string err;
  {
    std::lock_guard<std::mutex> lock(gHostMutex);
    err = Host().engine().loadModel(path);
  }
  return env->NewStringUTF(err.c_str());
}

extern "C" JNIEXPORT void JNICALL Java_com_lum_volum_NativeBridge_nativeClearModel(JNIEnv* /*env*/, jobject /*thiz*/)
{
  std::lock_guard<std::mutex> lock(gHostMutex);
  if (gHost)
    gHost->engine().clearModel();
}

extern "C" JNIEXPORT void JNICALL Java_com_lum_volum_NativeBridge_nativeSetInputGainDb(JNIEnv* /*env*/, jobject /*thiz*/,
                                                                                       jdouble db)
{
  std::lock_guard<std::mutex> lock(gHostMutex);
  Host().engine().setInputGainDb(static_cast<double>(db));
}

extern "C" JNIEXPORT void JNICALL Java_com_lum_volum_NativeBridge_nativeSetOutputGainDb(JNIEnv* /*env*/,
                                                                                        jobject /*thiz*/, jdouble db)
{
  std::lock_guard<std::mutex> lock(gHostMutex);
  Host().engine().setOutputGainDb(static_cast<double>(db));
}

extern "C" JNIEXPORT void JNICALL Java_com_lum_volum_NativeBridge_nativeSetTone(JNIEnv* /*env*/, jobject /*thiz*/,
                                                                                jdouble bass, jdouble mid,
                                                                                jdouble treble)
{
  std::lock_guard<std::mutex> lock(gHostMutex);
  Host().engine().setTone(static_cast<double>(bass), static_cast<double>(mid), static_cast<double>(treble));
}

extern "C" JNIEXPORT void JNICALL Java_com_lum_volum_NativeBridge_nativeSetToneEnabled(JNIEnv* /*env*/, jobject /*thiz*/,
                                                                                       jboolean on)
{
  std::lock_guard<std::mutex> lock(gHostMutex);
  Host().engine().setToneEnabled(on == JNI_TRUE);
}

extern "C" JNIEXPORT void JNICALL Java_com_lum_volum_NativeBridge_nativeSetBypass(JNIEnv* /*env*/, jobject /*thiz*/,
                                                                                  jboolean on)
{
  std::lock_guard<std::mutex> lock(gHostMutex);
  Host().engine().setBypass(on == JNI_TRUE);
}

extern "C" JNIEXPORT void JNICALL Java_com_lum_volum_NativeBridge_nativeSetGate(JNIEnv* /*env*/, jobject /*thiz*/,
                                                                                jboolean enabled, jdouble thresholdDb)
{
  std::lock_guard<std::mutex> lock(gHostMutex);
  Host().engine().setGate(enabled == JNI_TRUE, static_cast<double>(thresholdDb));
}

extern "C" JNIEXPORT void JNICALL Java_com_lum_volum_NativeBridge_nativeSetDelay(
  JNIEnv* /*env*/, jobject /*thiz*/, jboolean enabled, jdouble timeMs, jdouble feedback, jdouble mix, jint mode,
  jdouble tone, jdouble age, jboolean pingPong)
{
  std::lock_guard<std::mutex> lock(gHostMutex);
  Host().engine().setDelay(enabled == JNI_TRUE, static_cast<double>(timeMs), static_cast<double>(feedback),
                           static_cast<double>(mix), static_cast<int>(mode), static_cast<double>(tone),
                           static_cast<double>(age), pingPong == JNI_TRUE);
}

extern "C" JNIEXPORT void JNICALL Java_com_lum_volum_NativeBridge_nativeSetReverb(
  JNIEnv* /*env*/, jobject /*thiz*/, jboolean enabled, jdouble mix, jdouble decay, jdouble tone, jdouble preDelayMs,
  jdouble shimmer, jint mode, jint subMode)
{
  std::lock_guard<std::mutex> lock(gHostMutex);
  Host().engine().setReverb(enabled == JNI_TRUE, static_cast<double>(mix), static_cast<double>(decay),
                            static_cast<double>(tone), static_cast<double>(preDelayMs), static_cast<double>(shimmer),
                            static_cast<int>(mode), static_cast<int>(subMode));
}

extern "C" JNIEXPORT void JNICALL Java_com_lum_volum_NativeBridge_nativeSetTremolo(
  JNIEnv* /*env*/, jobject /*thiz*/, jboolean enabled, jdouble rateHz, jdouble depthKnob, jdouble shape, jdouble mix,
  jdouble crossoverHz, jint mode)
{
  std::lock_guard<std::mutex> lock(gHostMutex);
  Host().engine().setTremolo(enabled == JNI_TRUE, static_cast<double>(rateHz), static_cast<double>(depthKnob),
                             static_cast<double>(shape), static_cast<double>(mix), static_cast<double>(crossoverHz),
                             static_cast<int>(mode));
}

extern "C" JNIEXPORT void JNICALL Java_com_lum_volum_NativeBridge_nativeSetTunerEnabled(JNIEnv* /*env*/,
                                                                                        jobject /*thiz*/, jboolean on)
{
  std::lock_guard<std::mutex> lock(gHostMutex);
  Host().engine().setTunerEnabled(on == JNI_TRUE);
}

extern "C" JNIEXPORT jdouble JNICALL Java_com_lum_volum_NativeBridge_nativeTunerHz(JNIEnv* /*env*/, jobject /*thiz*/)
{
  std::lock_guard<std::mutex> lock(gHostMutex);
  return gHost ? static_cast<jdouble>(gHost->engine().tunerHz()) : 0.0;
}

extern "C" JNIEXPORT jstring JNICALL Java_com_lum_volum_NativeBridge_nativeStatus(JNIEnv* env, jobject /*thiz*/)
{
  std::lock_guard<std::mutex> lock(gHostMutex);
  char buf[256];
  if (gHost && gHost->isRunning())
  {
    gHost->refreshDiagnostics();
    std::snprintf(buf, sizeof(buf), "running sr=%d frames=%d latency=%.1fms xruns=%d model=%s peak=%.3f",
                  gHost->actualSampleRate(), gHost->framesPerCallback(), gHost->outputLatencyMs(), gHost->xRunCount(),
                  gHost->engine().hasModel() ? "yes" : "none", gHost->engine().lastPeak());
  }
  else
  {
    std::snprintf(buf, sizeof(buf), "stopped");
  }
  return env->NewStringUTF(buf);
}
