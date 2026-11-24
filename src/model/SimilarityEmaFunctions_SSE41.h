#pragma once

#include "../Utils.h"
#if defined(X64_SIMD_AVAILABLE) && !defined(AVX2_ONLY)

/**
 * SSE4.1 EMA update implementation.
 * See the scalar implementation for reference.
 */

// SSE4.1 EMA update + top-2 min-index search (8 values per iteration).
// Preconditions:
//   - ema_buf: 16-byte aligned, length >= count
//   - history_buffer_ptr[0..count): contiguous block, no ring-buffer wrap
//   - count: multiple of 8
void SimilarityEmaFunctions_SSE41_update(
  uint16_t* ema_buf,                  // [in/out] 8.8 fixed-point EMA buffer, length >= count
  const uint8_t* history_buffer_ptr,  // contiguous portion of the ring buffer with history data, length = count
  const size_t count,                 // number of distances
  const uint8_t c1,                   // incoming byte to be used for calculating the distances (basis of the update)
  const uint32_t ema_alpha,           // EMA alpha (fixed-point, denominator 64)
  uint32_t* match_index,              // [out] top-2 ema_buf indices (best, second-best)
  uint32_t* match_score);             // [out] top-2 ema_buf scores  (best, second-best)

#endif
