#pragma once

#include "../SystemDefines.h"

/**
 * Scalar (non-SIMD) EMA update implementations.
 *
 * Provides two update paths:
 *   update_with_wrapping  - warmup and ring-buffer wrap cases; processes a single EMA buffer
 *                           with index masking for non-contiguous access.
 *   update                - steady-state path; processes one EMA buffer over a contiguous block
 *                           (no ring-buffer wrap).
 *
 * Fixed-point layout: 8.8 (uint16_t), denominator 256.
 * Denominator for the EMA weights: 64 (alpha and one_minus_alpha sum to 64).
 */

// Inserts score r of index idx into the best two (match_index, match_score:
// best, second-best; lower = better) if it is one of them.
void SimilarityEma_top2(uint32_t r, uint32_t idx, uint32_t* match_index, uint32_t* match_score);

// EMA update with ring-buffer index masking.
// Used during warmup (count < MAX_MATCH_DISTANCE) and on the rare ring-buffer wrap.
// Processes a single EMA buffer; for the steady-state path use update.
void SimilarityEmaFunctions_Scalar_update_with_wrapping(
  uint16_t* ema_buf,          // [in/out] 8.8 fixed-point EMA values; length >= ema_start + count
  const uint8_t* history_buffer_ptr,  // ring buffer base pointer to history data
  const size_t buf_base,      // logical start index into the ring buffer
  const size_t buffer_mask,   // ring buffer size minus 1 (for index masking)
  const size_t count,         // number of distances to process
  const size_t ema_start,     // starting index into ema_buf[] (= MAX_MATCH_DISTANCE - count)
  const uint8_t c1,           // incoming byte to be used for calculating the distances (basis of the update)
  const uint32_t ema_alpha,   // EMA alpha (fixed-point, denominator 64)
  uint32_t* match_index,      // [out] top-2 ema_buf indices (best, second-best)
  uint32_t* match_score);     // [out] top-2 EMA scores      (best, second-best)

// EMA update over a contiguous block (no ring-buffer wrap).
// Steady-state path; satisfies the SimilarityEmaUpdateFunction signature.
void SimilarityEmaFunctions_Scalar_update(
  uint16_t* ema_buf,                  // [in/out] 8.8 fixed-point EMA buffer, length >= count
  const uint8_t* history_buffer_ptr,  // contiguous portion of the ring buffer with history data, length = count
  const size_t count,                 // number of distances
  const uint8_t c1,                   // incoming byte to be used for calculating the distances (basis of the update)
  const uint32_t ema_alpha,           // EMA alpha (fixed-point, denominator 64)
  uint32_t* match_index,              // [out] top-2 ema_buf indices (best, second-best)
  uint32_t* match_score);             // [out] top-2 ema_buf scores  (best, second-best)
