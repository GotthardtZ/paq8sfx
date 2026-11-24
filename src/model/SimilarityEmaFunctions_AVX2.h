#pragma once

#include "../Utils.h"
#ifdef X64_SIMD_AVAILABLE

/**
 * AVX2 EMA update implementation.
 * See the scalar implementation for reference.
 */

// AVX2 EMA update + top-2 min-index search (16 values per iteration).
// Preconditions:
//   - ema_buf: 32-byte aligned, length >= count
//   - history_buffer_ptr[0..count): contiguous block, no ring-buffer wrap
//   - count: multiple of 16
void SimilarityEmaFunctions_AVX2_update(
  uint16_t* ema_buf,                  // [in/out] 8.8 fixed-point EMA buffer, length >= count
  const uint8_t* history_buffer_ptr,  // contiguous portion of the ring buffer with history data, length = count
  const size_t count,                 // number of distances
  const uint8_t c1,                   // incoming byte to be used for calculating the distances (basis of the update)
  const uint32_t ema_alpha,           // EMA alpha (fixed-point, denominator 64)
  uint32_t* match_index,              // [out] top-2 ema_buf indices (best, second-best)
  uint32_t* match_score);             // [out] top-2 ema_buf scores  (best, second-best)

#endif
