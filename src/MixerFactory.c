#include "MixerFactory.h"

Mixer* MixerFactory_createMixer(const Shared* const sh, const int n, const int m, const int s, const int promoted) {
#ifdef AVX2_ONLY
  (void)sh;
  const MixerImpl* const impl = &Mixer_AVX2;
#elif defined(X64_SIMD_AVAILABLE)
  // SSE2 is part of x64: no scalar mixer is needed
  const MixerImpl* const impl = sh->chosenSimd >= SIMD_AVX2 ? &Mixer_AVX2 : &Mixer_SSE2;
#else
  (void)sh;
  const MixerImpl* const impl = &Mixer_Scalar;
#endif
  return Mixer_new(sh, n, m, s, promoted, impl);
}
