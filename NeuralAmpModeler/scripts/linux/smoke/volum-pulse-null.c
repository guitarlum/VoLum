/* Align a Pulse recording with the gate's -24 dBFS 440 Hz sine.
 *
 *   volum-pulse-null analyze rec.wav --tol-peak-db N --dropout-limit N
 *   volum-pulse-null self-test
 *
 * The sine, the gain window and the dropout ratio are fixed. Only the null
 * peak tolerance and the dropout-sample limit are arguments, so a caller can
 * print a calibration and then keep those two numbers.
 */

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const double kFreq = 440.0;
static const double kAmpDb = -24.0;
static const double kGainMin = 0.3;
static const double kGainMax = 3.0;
/* A zeroed sample of the tone leaves an error of about |sin|. Count samples
 * whose error exceeds half the fitted tone. A clean resampler stays far under
 * that; one dropped buffer crosses it for most of a period. */
static const double kDropoutRatio = 0.5;

static double Amp(void) { return pow(10.0, kAmpDb / 20.0); }

static int ReadU16(const unsigned char* p) { return p[0] | (p[1] << 8); }
static int ReadU32(const unsigned char* p) { return p[0] | (p[1] << 8) | (p[2] << 16) | (p[3] << 24); }

static int LoadWav(const char* path, int* rate, float** mono, int* frames)
{
  FILE* f = fopen(path, "rb");
  if (!f)
  {
    fprintf(stderr, "REASON: cannot open %s\n", path);
    return 1;
  }
  if (fseek(f, 0, SEEK_END) != 0)
  {
    fclose(f);
    return 1;
  }
  long bytes = ftell(f);
  if (bytes < 44)
  {
    fclose(f);
    fprintf(stderr, "REASON: %s is not a wav file\n", path);
    return 1;
  }
  rewind(f);
  unsigned char* raw = (unsigned char*)malloc((size_t)bytes);
  if (!raw || fread(raw, 1, (size_t)bytes, f) != (size_t)bytes)
  {
    free(raw);
    fclose(f);
    fprintf(stderr, "REASON: short read of %s\n", path);
    return 1;
  }
  fclose(f);
  if (memcmp(raw, "RIFF", 4) || memcmp(raw + 8, "WAVE", 4))
  {
    free(raw);
    fprintf(stderr, "REASON: %s is not a wav file\n", path);
    return 1;
  }

  int format = 0, channels = 0, fileRate = 0, bits = 0;
  const unsigned char* data = NULL;
  int dataBytes = 0;
  int pos = 12;
  while (pos + 8 <= bytes)
  {
    int size = ReadU32(raw + pos + 4);
    if (size < 0 || pos + 8 + size > bytes)
      break;
    if (!memcmp(raw + pos, "fmt ", 4) && size >= 16)
    {
      format = ReadU16(raw + pos + 8);
      channels = ReadU16(raw + pos + 10);
      fileRate = ReadU32(raw + pos + 12);
      bits = ReadU16(raw + pos + 22);
    }
    else if (!memcmp(raw + pos, "data", 4))
    {
      data = raw + pos + 8;
      dataBytes = size;
    }
    pos += 8 + size + (size & 1);
  }
  /* 1 = PCM, 3 = float, 0xFFFE = extensible (parecord). 16-bit is PCM either way. */
  int pcm16 = (bits == 16 && (format == 1 || format == 0xFFFE));
  int pcm32f = (bits == 32 && format == 3);
  if (!data || channels < 1 || fileRate < 1000 || (!pcm16 && !pcm32f))
  {
    free(raw);
    fprintf(stderr, "REASON: unsupported wav (format %d, %d bit, %d ch)\n", format, bits, channels);
    return 1;
  }
  int frameBytes = channels * (bits / 8);
  int n = dataBytes / frameBytes;
  float* out = (float*)malloc((size_t)n * sizeof(float));
  if (!out)
  {
    free(raw);
    return 1;
  }
  for (int i = 0; i < n; ++i)
  {
    const unsigned char* s = data + (size_t)i * frameBytes;
    if (pcm16)
    {
      int v = s[0] | (s[1] << 8);
      if (v & 0x8000)
        v -= 0x10000;
      out[i] = (float)v / 32768.f;
    }
    else
    {
      float v;
      memcpy(&v, s, 4);
      out[i] = v;
    }
  }
  free(raw);
  *rate = fileRate;
  *mono = out;
  *frames = n;
  return 0;
}

static int WriteS16(const char* path, int rate, const float* x, int n)
{
  FILE* f = fopen(path, "wb");
  if (!f)
    return 1;
  int dataBytes = n * 2;
  int riffBytes = 36 + dataBytes;
  unsigned char hdr[44];
  memset(hdr, 0, sizeof hdr);
  memcpy(hdr, "RIFF", 4);
  hdr[4] = riffBytes & 0xff;
  hdr[5] = (riffBytes >> 8) & 0xff;
  hdr[6] = (riffBytes >> 16) & 0xff;
  hdr[7] = (riffBytes >> 24) & 0xff;
  memcpy(hdr + 8, "WAVEfmt ", 8);
  hdr[16] = 16;
  hdr[20] = 1;
  hdr[22] = 1;
  hdr[24] = rate & 0xff;
  hdr[25] = (rate >> 8) & 0xff;
  hdr[26] = (rate >> 16) & 0xff;
  hdr[27] = (rate >> 24) & 0xff;
  int byteRate = rate * 2;
  hdr[28] = byteRate & 0xff;
  hdr[29] = (byteRate >> 8) & 0xff;
  hdr[30] = (byteRate >> 16) & 0xff;
  hdr[31] = (byteRate >> 24) & 0xff;
  hdr[32] = 2;
  hdr[34] = 16;
  memcpy(hdr + 36, "data", 4);
  hdr[40] = dataBytes & 0xff;
  hdr[41] = (dataBytes >> 8) & 0xff;
  hdr[42] = (dataBytes >> 16) & 0xff;
  hdr[43] = (dataBytes >> 24) & 0xff;
  if (fwrite(hdr, 1, 44, f) != 44)
  {
    fclose(f);
    return 1;
  }
  for (int i = 0; i < n; ++i)
  {
    double c = x[i] * 32767.0;
    if (c > 32767.0)
      c = 32767.0;
    if (c < -32767.0)
      c = -32767.0;
    int v = (int)lrint(c);
    unsigned char b[2] = {(unsigned char)(v & 0xff), (unsigned char)((v >> 8) & 0xff)};
    if (fwrite(b, 1, 2, f) != 2)
    {
      fclose(f);
      return 1;
    }
  }
  fclose(f);
  return 0;
}

typedef struct Result
{
  int rc;
  const char* reason;
  double recPeakDb;
  double playGain;
  double residualPeakDb;
  double residualRmsDb;
  int dropoutSamples;
  int lag;
  double overlapSeconds;
} Result;

static double Db(double v) { return v > 1e-12 ? 20.0 * log10(v) : -999.0; }

static Result Measure(const float* rec, int n, int rate, double tolPeakDb, int dropoutLimit)
{
  Result r;
  memset(&r, 0, sizeof r);
  r.residualPeakDb = -999.0;
  r.residualRmsDb = -999.0;
  r.recPeakDb = -999.0;
  r.reason = "";
  const double amp = Amp();
  const double w = 2.0 * 3.14159265358979323846 * kFreq / (double)rate;

  double peak = 0.0;
  for (int i = 0; i < n; ++i)
  {
    double a = fabs((double)rec[i]);
    if (a > peak)
      peak = a;
  }
  r.recPeakDb = Db(peak);
  if (n < rate / 2 || peak < amp * 0.2)
  {
    r.rc = 1;
    r.reason = "recording is silent or too short; playback did not reach the source";
    return r;
  }

  const double thresh = amp * 0.25;
  int onset = -1, last = -1;
  for (int i = 0; i < n; ++i)
  {
    if (fabs((double)rec[i]) >= thresh)
    {
      if (onset < 0)
        onset = i;
      last = i;
    }
  }
  int ncorr = 4096;
  if (last - onset < ncorr * 2)
    ncorr = (last - onset) / 2;
  if (ncorr < 256)
  {
    r.rc = 1;
    r.reason = "recording overlap is shorter than 0.5 s";
    return r;
  }
  int lo = onset - 800;
  int hi = onset + 800;
  if (lo < 0)
    lo = 0;
  if (hi + ncorr > n)
    hi = n - ncorr;
  double best = -1e300;
  int bestLag = onset;
  const double cs = cos(w), sn = sin(w);
  for (int lag = lo; lag <= hi; ++lag)
  {
    double s = 0.0, c = 1.0, dot = 0.0;
    const float* p = rec + lag;
    for (int i = 0; i < ncorr; ++i)
    {
      dot += (double)p[i] * s;
      double ns = s * cs + c * sn;
      c = c * cs - s * sn;
      s = ns;
    }
    if (dot > best)
    {
      best = dot;
      bestLag = lag;
    }
  }
  r.lag = bestLag;

  /* The first half-second of the tone is the stream opening (resampler
   * startup). It is not part of the null. A dropped buffer later still is. */
  int skip = rate / 2;
  int a = bestLag + skip;
  int b = last - skip;
  if (a < 0)
    a = 0;
  if (b > n)
    b = n;
  if (b - a < rate / 2)
  {
    r.rc = 1;
    r.reason = "recording overlap is shorter than 0.5 s";
    r.overlapSeconds = (b > a ? (b - a) : 0) / (double)rate;
    return r;
  }
  r.overlapSeconds = (b - a) / (double)rate;

  /* A time shift of a pure sine is a phase shift, so fit sin and cos. An
   * integer lag that lands a fraction of a period off then still nulls, which
   * is what a 5 minute file needs: the onset search can lock the start. */
  double rs = 0.0, rc = 0.0, ss = 0.0, cc = 0.0;
  double ps = 0.0, pc = 1.0;
  for (int i = a; i < b; ++i)
  {
    if (((i - a) & 255) == 0)
    {
      const double len = hypot(ps, pc);
      if (len > 0.0)
      {
        ps /= len;
        pc /= len;
      }
    }
    rs += (double)rec[i] * ps;
    rc += (double)rec[i] * pc;
    ss += ps * ps;
    cc += pc * pc;
    const double ns = ps * cs + pc * sn;
    pc = pc * cs - ps * sn;
    ps = ns;
  }
  const double coefS = ss > 0.0 ? rs / ss : 0.0;
  const double coefC = cc > 0.0 ? rc / cc : 0.0;
  const double tone = hypot(coefS, coefC);
  r.playGain = amp > 0.0 ? tone / amp : 0.0;
  if (!(r.playGain >= kGainMin && r.playGain <= kGainMax))
  {
    r.rc = 1;
    r.reason = "playback gain is outside 0.3..3 (muted, or not the played signal)";
    return r;
  }

  double errPeak = 0.0, errSq = 0.0;
  int dropouts = 0;
  const double dropAbs = kDropoutRatio * tone;
  ps = 0.0;
  pc = 1.0;
  for (int i = a; i < b; ++i)
  {
    if (((i - a) & 255) == 0)
    {
      const double len = hypot(ps, pc);
      if (len > 0.0)
      {
        ps /= len;
        pc /= len;
      }
    }
    const double err = (double)rec[i] - (coefS * ps + coefC * pc);
    const double ae = fabs(err);
    if (ae > errPeak)
      errPeak = ae;
    errSq += err * err;
    if (ae > dropAbs)
      ++dropouts;
    const double ns = ps * cs + pc * sn;
    pc = pc * cs - ps * sn;
    ps = ns;
  }
  r.residualPeakDb = Db(errPeak);
  r.residualRmsDb = Db(sqrt(errSq / (double)(b - a)));
  r.dropoutSamples = dropouts;
  if (r.residualPeakDb > tolPeakDb)
  {
    r.rc = 1;
    r.reason = "null residual peak is above the tolerance";
    return r;
  }
  if (dropouts > dropoutLimit)
  {
    r.rc = 1;
    r.reason = "dropped samples in the aligned recording";
    return r;
  }
  r.rc = 0;
  r.reason = "ok";
  return r;
}

static void PrintResult(const Result* r)
{
  printf("rec_peak_dbfs=%.2f\n", r->recPeakDb);
  printf("lag_samples=%d\n", r->lag);
  printf("play_gain=%.4f\n", r->playGain);
  printf("overlap_seconds=%.2f\n", r->overlapSeconds);
  printf("residual_peak_dbfs=%.2f\n", r->residualPeakDb);
  printf("residual_rms_dbfs=%.2f\n", r->residualRmsDb);
  printf("dropout_samples=%d\n", r->dropoutSamples);
  if (r->rc != 0)
    printf("REASON: %s\n", r->reason);
}

static int CmdAnalyze(int argc, char** argv)
{
  const char* path = NULL;
  double tol = -40.0;
  int limit = 32;
  for (int i = 0; i < argc; ++i)
  {
    if (!strcmp(argv[i], "--tol-peak-db") && i + 1 < argc)
      tol = atof(argv[++i]);
    else if (!strcmp(argv[i], "--dropout-limit") && i + 1 < argc)
      limit = atoi(argv[++i]);
    else if (argv[i][0] != '-')
      path = argv[i];
  }
  if (!path)
  {
    fprintf(stderr, "REASON: analyze needs a wav path\n");
    return 2;
  }
  int rate = 0, n = 0;
  float* rec = NULL;
  if (LoadWav(path, &rate, &rec, &n))
    return 1;
  Result r = Measure(rec, n, rate, tol, limit);
  free(rec);
  PrintResult(&r);
  return r.rc;
}

static int FailTest(const char* what) 
{
  fprintf(stderr, "SELF-TEST FAIL: %s\n", what);
  return 1;
}

static int CmdSelfTest(void)
{
  const int rate = 44100;
  const int n = rate * 2;
  const int lag = 3000;
  const double amp = Amp();
  float* x = (float*)calloc((size_t)n, sizeof(float));
  if (!x)
    return 1;
  for (int i = lag; i < n; ++i)
    x[i] = (float)(0.9 * amp * sin(2.0 * 3.14159265358979323846 * kFreq * (i - lag) / rate));
  const char* clean = "/tmp/volum-null-self-clean.wav";
  const char* mute = "/tmp/volum-null-self-mute.wav";
  const char* drop = "/tmp/volum-null-self-drop.wav";
  if (WriteS16(clean, rate, x, n))
    return FailTest("write clean");
  int fileRate = 0, fileN = 0;
  float* loaded = NULL;
  if (LoadWav(clean, &fileRate, &loaded, &fileN))
    return FailTest("read clean");
  Result ok = Measure(loaded, fileN, fileRate, -40.0, 32);
  free(loaded);
  PrintResult(&ok);
  if (ok.rc != 0)
    return FailTest("clean recording was rejected");
  if (ok.lag < lag - 5 || ok.lag > lag + 5)
    return FailTest("lag did not find the tone onset");
  if (ok.playGain < 0.8 || ok.playGain > 1.0)
    return FailTest("gain of the 0.9 tone is wrong");
  if (ok.dropoutSamples != 0)
    return FailTest("clean recording counted dropouts");

  memset(x, 0, (size_t)n * sizeof(float));
  x[100] = 1.f / 32768.f;
  if (WriteS16(mute, rate, x, n))
    return FailTest("write mute");
  if (LoadWav(mute, &fileRate, &loaded, &fileN))
    return FailTest("read mute");
  Result muted = Measure(loaded, fileN, fileRate, -40.0, 32);
  free(loaded);
  PrintResult(&muted);
  if (muted.rc == 0 || !muted.reason || !strstr(muted.reason, "silent"))
    return FailTest("muted recording was accepted");

  for (int i = lag; i < n; ++i)
    x[i] = (float)(0.9 * amp * sin(2.0 * 3.14159265358979323846 * kFreq * (i - lag) / rate));
  for (int i = 0; i < 256; ++i)
    x[lag + rate + i] = 0.f;
  if (WriteS16(drop, rate, x, n))
    return FailTest("write drop");
  if (LoadWav(drop, &fileRate, &loaded, &fileN))
    return FailTest("read drop");
  Result dropped = Measure(loaded, fileN, fileRate, 0.0, 32);
  free(loaded);
  free(x);
  PrintResult(&dropped);
  /* tol 0 dBFS so only the dropout counter can fail. */
  if (dropped.rc == 0 || dropped.dropoutSamples < 64)
    return FailTest("dropped buffer was accepted");
  printf("self-test ok\n");
  return 0;
}

int main(int argc, char** argv)
{
  if (argc >= 2 && !strcmp(argv[1], "self-test"))
    return CmdSelfTest();
  if (argc >= 3 && !strcmp(argv[1], "analyze"))
    return CmdAnalyze(argc - 2, argv + 2);
  fprintf(stderr, "usage: volum-pulse-null analyze rec.wav [--tol-peak-db N] [--dropout-limit N] | self-test\n");
  return 2;
}
