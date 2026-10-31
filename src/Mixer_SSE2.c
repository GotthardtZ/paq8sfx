#include "Mixer_SSE2.h"

#if defined(X64_SIMD_AVAILABLE) && !defined(AVX2_ONLY)

#define SIMD_WIDTH_SSE2 (16 / sizeof(short)) // 8 shorts per 128-bit lane

TARGET("sse2")
static int Mixer_SSE2_dotProduct(const Mixer* const self, const short* const w, const size_t n) {
  const short* const tx = self->tx;
  __m128i sum = _mm_setzero_si128();

  for (size_t i = 0; i < n; i += 8) {
    __m128i tmp = _mm_madd_epi16(*(const __m128i*) &tx[i], *(const __m128i*) &w[i]);
    tmp = _mm_srai_epi32(tmp, 8);
    sum = _mm_add_epi32(sum, tmp);
  }

  sum = _mm_add_epi32(sum, _mm_srli_si128(sum, 8));
  sum = _mm_add_epi32(sum, _mm_srli_si128(sum, 4));
  return _mm_cvtsi128_si32(sum);
}

TARGET("sse2")
static int Mixer_SSE2_dotProduct2(const Mixer* const self, const short* const w0, const short* const w1, const size_t n, int* const sum1) {
  const short* const tx = self->tx;
  __m128i s0 = _mm_setzero_si128();
  __m128i s1 = _mm_setzero_si128();
  for (size_t i = 0; i < n; i += 8) {
    const __m128i t = *(const __m128i*) &tx[i];
    __m128i tmp0 = _mm_madd_epi16(t, *(const __m128i*) &w0[i]);
    __m128i tmp1 = _mm_madd_epi16(t, *(const __m128i*) &w1[i]);
    s0 = _mm_add_epi32(s0, _mm_srai_epi32(tmp0, 8));
    s1 = _mm_add_epi32(s1, _mm_srai_epi32(tmp1, 8));
  }
  s0 = _mm_add_epi32(s0, _mm_srli_si128(s0, 8));
  s0 = _mm_add_epi32(s0, _mm_srli_si128(s0, 4));
  s1 = _mm_add_epi32(s1, _mm_srli_si128(s1, 8));
  s1 = _mm_add_epi32(s1, _mm_srli_si128(s1, 4));
  *sum1 = _mm_cvtsi128_si32(s1);
  return _mm_cvtsi128_si32(s0);
}

TARGET("sse2")
static void Mixer_SSE2_train(const Mixer* const self, short* const w, const size_t n, const int e) {
  const short* const tx = self->tx;
  const __m128i one = _mm_set1_epi16(1);
  const __m128i err = _mm_set1_epi16((short)e);

  for (size_t i = 0; i < n; i += 8) {
    __m128i tmp = _mm_adds_epi16(*(const __m128i*) &tx[i], *(const __m128i*) &tx[i]);
    tmp = _mm_mulhi_epi16(tmp, err);
    tmp = _mm_adds_epi16(tmp, one);
    tmp = _mm_srai_epi16(tmp, 1);
    tmp = _mm_adds_epi16(tmp, *(__m128i*) &w[i]);
    *(__m128i*) &w[i] = tmp;
  }
}

Mixer* Mixer_SSE2_new(const Shared* const sh, const int n, const int m, const int s, const int promoted) {
  Mixer* const mixer = (Mixer*)calloc(1, sizeof(Mixer));
  if (mixer == NULL) {
    quit("Out of memory.");
  }
  Mixer_init(mixer, sh, n, m, s, (int)SIMD_WIDTH_SSE2);
  mixer->dotProduct = Mixer_SSE2_dotProduct;
  mixer->dotProduct2 = Mixer_SSE2_dotProduct2;
  mixer->train = Mixer_SSE2_train;
  if (s > 1) {
    mixer->mp = Mixer_SSE2_new(sh, s + promoted, 1, 1, 0);
  }
  return mixer;
}

#endif // X64_SIMD_AVAILABLE
