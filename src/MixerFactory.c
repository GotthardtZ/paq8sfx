#include "MixerFactory.h"

Mixer* MixerFactory_createMixer(const Shared* const sh, const int n, const int m, const int s, const int promoted) {
#ifdef AVX2_ONLY
  return Mixer_AVX2_new(sh, n, m, s, promoted);
#else
#ifdef X64_SIMD_AVAILABLE
  const SIMDType chosenSimd = sh->chosenSimd;
  if (chosenSimd >= SIMD_AVX2) {
    return Mixer_AVX2_new(sh, n, m, s, promoted);
  }
  else if (chosenSimd >= SIMD_SSE2) {
    return Mixer_SSE2_new(sh, n, m, s, promoted);
  }
#endif
  return Mixer_Scalar_new(sh, n, m, s, promoted);
#endif
}
