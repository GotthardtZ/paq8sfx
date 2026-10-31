#include "MixerFactory.hpp"

MixerFactory::MixerFactory(const Shared* const sh) : shared(sh) {}

Mixer* MixerFactory::createMixer(const int n, const int m, const int s, const int promoted) const {
#ifdef AVX2_ONLY
  return new Mixer_AVX2(shared, n, m, s, promoted);
#else
#ifdef X64_SIMD_AVAILABLE
  const SIMDType chosenSimd = shared->chosenSimd;
  if (chosenSimd >= SIMDType::SIMD_AVX2) {
    return new Mixer_AVX2(shared, n, m, s, promoted);
  }
  else if (chosenSimd >= SIMDType::SIMD_SSE2) {
    return new Mixer_SSE2(shared, n, m, s, promoted);
  }
#endif
  return new Mixer_Scalar(shared, n, m, s, promoted);
#endif
}
