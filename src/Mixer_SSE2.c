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

const MixerImpl Mixer_SSE2 = {
  (int)SIMD_WIDTH_SSE2,
  Mixer_SSE2_dotProduct,
  Mixer_SSE2_train
};

#endif // X64_SIMD_AVAILABLE
