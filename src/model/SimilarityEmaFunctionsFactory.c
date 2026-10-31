#include "SimilarityEmaFunctionsFactory.h"

SimilarityEmaUpdateFunction SimilarityEmaFunctionsFactory_getEmaUpdateFunction(const Shared* const shared) {
#if !defined(AVX2_ONLY)

#ifdef X64_SIMD_AVAILABLE
  const SIMDType chosenSimd = shared->chosenSimd;
  if (chosenSimd >= SIMD_AVX2) {
    return &SimilarityEmaFunctions_AVX2_update_and_find;
  }
  else if (chosenSimd >= SIMD_SSE41) {
    return &SimilarityEmaFunctions_SSE41_update_and_find;
  }
#endif
  return &SimilarityEmaFunctions_Scalar_update_and_find;

#else
  (void)shared;
  return &SimilarityEmaFunctions_AVX2_update_and_find;
#endif

}
