#pragma once

// Inner loops of the VoLum pitch engine (VoLumPitchVoice.h, VoLumPitchTracker.h) and the ring reader
// they share. Each blocked kernel carries the one-lag-at-a-time oracle's bits; the exact-output locks in
// tests/test_volum_pitch_burst.cpp hold them to it.

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <vector>

#if defined(_M_X64) || defined(__x86_64__) || defined(__SSE2__) || (defined(_M_IX86_FP) && _M_IX86_FP >= 2)
  #include <emmintrin.h>
  #define VOLUM_PITCH_SSE2 1
#endif

namespace dsp
{
namespace effect
{
namespace pitch_kernels
{

constexpr int kWsolaBlock = 8;

// Linear-interpolated read `delay` samples behind the write head of a ring.
inline double ReadRingAtDelay(const std::vector<double>& buf, size_t write, double delay)
{
  const double sz = static_cast<double>(buf.size());
  double rp = static_cast<double>(write) - delay;
  while (rp < 0.0)
    rp += sz;
  while (rp >= sz)
    rp -= sz;
  const double fl = std::floor(rp);
  const size_t n = buf.size();
  size_t i0 = static_cast<size_t>(fl);
  if (i0 >= n)
    i0 %= n; // only a non-finite delay gets here; the loops above keep rp in [0, n)
  const size_t i1 = (i0 + 1 >= n) ? 0 : i0 + 1;
  const double frac = rp - fl;
  return buf[i0] * (1.0 - frac) + buf[i1] * frac;
}

// dot[b] = sum over j < win of ref[j] * cand[b + j], sn[b] = sum of cand[b + j]^2, for b < count
// (count <= kWsolaBlock), each summed in ascending j. SSE2 lanes round every multiply and add
// separately, like scalar x64 code, so the squares come precomputed in candSq (same products).
// Non-SSE2 must split mul and add the same way: Apple clang contracts `d += r * v` into an FMA
// that does not match the one-lag-at-a-time oracle's association.
inline void WindowCorrelations(const double* ref, const double* cand, const double* candSq, int win, int count,
                               double* dot, double* sn)
{
  int b = 0;
#if defined(VOLUM_PITCH_SSE2)
  for (; b + 4 <= count; b += 4)
  {
    __m128d d0 = _mm_setzero_pd(), d1 = _mm_setzero_pd(), s0 = _mm_setzero_pd(), s1 = _mm_setzero_pd();
    const double* c = cand + b;
    const double* q = candSq + b;
    for (int j = 0; j < win; ++j)
    {
      const __m128d r = _mm_set1_pd(ref[j]);
      d0 = _mm_add_pd(d0, _mm_mul_pd(r, _mm_loadu_pd(c + j)));
      d1 = _mm_add_pd(d1, _mm_mul_pd(r, _mm_loadu_pd(c + j + 2)));
      s0 = _mm_add_pd(s0, _mm_loadu_pd(q + j));
      s1 = _mm_add_pd(s1, _mm_loadu_pd(q + j + 2));
    }
    _mm_storeu_pd(dot + b, d0);
    _mm_storeu_pd(dot + b + 2, d1);
    _mm_storeu_pd(sn + b, s0);
    _mm_storeu_pd(sn + b + 2, s1);
  }
#else
  (void)candSq;
  if (count == kWsolaBlock)
  {
    double d0 = 0.0, d1 = 0.0, d2 = 0.0, d3 = 0.0, d4 = 0.0, d5 = 0.0, d6 = 0.0, d7 = 0.0;
    double s0 = 0.0, s1 = 0.0, s2 = 0.0, s3 = 0.0, s4 = 0.0, s5 = 0.0, s6 = 0.0, s7 = 0.0;
    for (int j = 0; j < win; ++j)
    {
      const double r = ref[j];
      const double* p = cand + j;
      const double v0 = p[0], v1 = p[1], v2 = p[2], v3 = p[3], v4 = p[4], v5 = p[5], v6 = p[6], v7 = p[7];
      const double rd0 = r * v0, rd1 = r * v1, rd2 = r * v2, rd3 = r * v3;
      const double rd4 = r * v4, rd5 = r * v5, rd6 = r * v6, rd7 = r * v7;
      const double sq0 = v0 * v0, sq1 = v1 * v1, sq2 = v2 * v2, sq3 = v3 * v3;
      const double sq4 = v4 * v4, sq5 = v5 * v5, sq6 = v6 * v6, sq7 = v7 * v7;
      d0 += rd0;
      s0 += sq0;
      d1 += rd1;
      s1 += sq1;
      d2 += rd2;
      s2 += sq2;
      d3 += rd3;
      s3 += sq3;
      d4 += rd4;
      s4 += sq4;
      d5 += rd5;
      s5 += sq5;
      d6 += rd6;
      s6 += sq6;
      d7 += rd7;
      s7 += sq7;
    }
    const double dots[kWsolaBlock] = {d0, d1, d2, d3, d4, d5, d6, d7};
    const double sns[kWsolaBlock] = {s0, s1, s2, s3, s4, s5, s6, s7};
    std::copy(dots, dots + kWsolaBlock, dot);
    std::copy(sns, sns + kWsolaBlock, sn);
    return;
  }
#endif
  for (; b < count; ++b)
  {
    double d = 0.0, s = 0.0;
    for (int j = 0; j < win; ++j)
    {
      const double v = cand[b + j];
      const double rd = ref[j] * v;
      const double sq = v * v;
      d += rd;
      s += sq;
    }
    dot[b] = d;
    sn[b] = s;
  }
}

// r[lag] = sum over k < L of s[k] * s[k + lag] for lag in [lagBegin, lagEnd). Blocks of lags share
// each pass over k; each lag keeps one accumulator summed in ascending k. SSE2 rounds mul and add
// apart. The non-SSE2 blocked path must too: Apple clang contracts `a += x * p[i]` into an FMA that
// does not match the one-lag-at-a-time oracle's association on the same machine.
inline void LagCorrelations(const double* s, int L, int lagBegin, int lagEnd, double* r)
{
  int lag = lagBegin;
#if defined(VOLUM_PITCH_SSE2)
  // Four accumulators: MSVC unrolls k by four and spills anything wider to the stack.
  for (; lag + 8 <= lagEnd; lag += 8)
  {
    __m128d a0 = _mm_setzero_pd(), a1 = _mm_setzero_pd(), a2 = _mm_setzero_pd(), a3 = _mm_setzero_pd();
    const double* p = s + lag;
    for (int k = 0; k < L; ++k)
    {
      const __m128d x = _mm_set1_pd(s[k]);
      a0 = _mm_add_pd(a0, _mm_mul_pd(x, _mm_loadu_pd(p + k)));
      a1 = _mm_add_pd(a1, _mm_mul_pd(x, _mm_loadu_pd(p + k + 2)));
      a2 = _mm_add_pd(a2, _mm_mul_pd(x, _mm_loadu_pd(p + k + 4)));
      a3 = _mm_add_pd(a3, _mm_mul_pd(x, _mm_loadu_pd(p + k + 6)));
    }
    _mm_storeu_pd(r + lag, a0);
    _mm_storeu_pd(r + lag + 2, a1);
    _mm_storeu_pd(r + lag + 4, a2);
    _mm_storeu_pd(r + lag + 6, a3);
  }
  for (; lag + 2 <= lagEnd; lag += 2)
  {
    __m128d a = _mm_setzero_pd();
    for (int k = 0; k < L; ++k)
      a = _mm_add_pd(a, _mm_mul_pd(_mm_set1_pd(s[k]), _mm_loadu_pd(s + k + lag)));
    _mm_storeu_pd(r + lag, a);
  }
#else
  for (; lag + 8 <= lagEnd; lag += 8)
  {
    double a0 = 0.0, a1 = 0.0, a2 = 0.0, a3 = 0.0, a4 = 0.0, a5 = 0.0, a6 = 0.0, a7 = 0.0;
    for (int k = 0; k < L; ++k)
    {
      const double x = s[k];
      const double* p = s + k + lag;
      // Mul then add apart, like the SSE2 path — fused `+= x * p[i]` diverges from the oracle.
      const double t0 = x * p[0];
      const double t1 = x * p[1];
      const double t2 = x * p[2];
      const double t3 = x * p[3];
      const double t4 = x * p[4];
      const double t5 = x * p[5];
      const double t6 = x * p[6];
      const double t7 = x * p[7];
      a0 += t0;
      a1 += t1;
      a2 += t2;
      a3 += t3;
      a4 += t4;
      a5 += t5;
      a6 += t6;
      a7 += t7;
    }
    const double sums[8] = {a0, a1, a2, a3, a4, a5, a6, a7};
    std::copy(sums, sums + 8, r + lag);
  }
#endif
  for (; lag < lagEnd; ++lag)
  {
    double a = 0.0;
    for (int k = 0; k < L; ++k)
    {
      const double t = s[k] * s[k + lag];
      a += t;
    }
    r[lag] = a;
  }
}

} // namespace pitch_kernels
} // namespace effect
} // namespace dsp
