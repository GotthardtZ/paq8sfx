#pragma once

#include "../Shared.h"
#include "SimilarityEmaFunctions_Scalar.h"
#ifdef X64_SIMD_AVAILABLE
#include "SimilarityEmaFunctions_SSE41.h"
#include "SimilarityEmaFunctions_AVX2.h"
#endif

// Function pointer type for the steady-state EMA update path (contiguous block, no ring-buffer wrap).
// Updates one EMA buffer (the slow or the fast model) in-place (8.8 fixed-point uint16_t values),
// and writes the top-2 lowest-score indices and scores into match_index/match_score.
// count is a multiple of 16 (AVX2 stride).
typedef void (*SimilarityEmaUpdateFunction)(
  uint16_t* ema_buf,                  // [in/out] 8.8 fixed-point EMA buffer, length >= count
  const uint8_t* history_buffer_ptr,  // contiguous portion of the ring buffer with history data, length = count
  const size_t count,                 // number of distances
  const uint8_t c1,                   // incoming byte to be used for calculating the distances (basis of the update)
  const uint32_t ema_alpha,           // EMA alpha (fixed-point, denominator 64)
  uint32_t* match_index,              // [out] top-2 ema_buf indices (best, second-best)
  uint32_t* match_score);             // [out] top-2 ema_buf scores  (best, second-best)

/**
 * Selects the best available EMA update function at runtime.
 */
SimilarityEmaUpdateFunction SimilarityEmaFunctionsFactory_getEmaUpdateFunction(const Shared* shared);
