#include "SimilarityEmaFunctionsFactory.h"

SimilarityEmaUpdateFunction SimilarityEmaFunctionsFactory_getEmaUpdateFunction(const Shared* const shared) {
#if !defined(AVX2_ONLY)

#ifdef X64_SIMD_AVAILABLE
  const SIMDType chosenSimd = shared->chosenSimd;
  if (chosenSimd >= SIMD_AVX2) {
    return &SimilarityEmaFunctions_AVX2_update;
  }
  else if (chosenSimd >= SIMD_SSE41) {
    return &SimilarityEmaFunctions_SSE41_update;
  }
#endif
  return &SimilarityEmaFunctions_Scalar_update;

#else
  (void)shared;
  return &SimilarityEmaFunctions_AVX2_update;
#endif

}
