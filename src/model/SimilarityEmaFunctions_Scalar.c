#include "SimilarityEmaFunctions_Scalar.h"
#include "../Utils.h"

// Inserts score r of index idx into the best two (lower = better), if it is
// one of them. Shared by every EMA implementation: it is rarely reached in the
// SIMD loops (only when a lane beats the second-best).
void SimilarityEma_top2(const uint32_t r, const uint32_t idx, uint32_t* const match_index, uint32_t* const match_score) {
  // Note: iterating longest-to-shortest means on equal scores, longer periods win
  //       this way we'll start at the beginning of a sequence, not at the end
  if (r < match_score[1]) {
    if (r < match_score[0]) {
      match_index[1] = match_index[0];
      match_score[1] = match_score[0];
      match_index[0] = idx;
      match_score[0] = r;
    }
    else {
      match_index[1] = idx;
      match_score[1] = r;
    }
  }
}

// Scalar EMA update with ring-buffer index masking (warmup and rare wrap case).
// Fixed-point note: diff (0..128) is shifted left 8 to form the 8.8 fixed-point current sample (0..32768).
// Overflow check: max intermediate = 32768 * 64 = 2'097'152 < 2^21, fits in uint32.
void SimilarityEmaFunctions_Scalar_update_with_wrapping(
  uint16_t* ema_buf, // 8.8 fixed-point: value = stored / 256.0
  const uint8_t* history_buffer_ptr,
  const size_t buf_base,
  const size_t buffer_mask,
  const size_t count,
  const size_t ema_start,
  const uint8_t c1,
  const uint32_t ema_alpha,
  uint32_t* match_index,
  uint32_t* match_score) {

  match_index[0] = 0; // index of the period with the lowest EMA score
  match_index[1] = 0; // index of the period with the second-lowest EMA score
  match_score[0] = UINT16_MAX;
  match_score[1] = UINT16_MAX;

  const uint32_t ema_one_minus_alpha = 64 - ema_alpha; // (1 - alpha) in fixed-point, denominator 64

  for (size_t i = 0; i < count; i++) {

    uint8_t pred = history_buffer_ptr[(buf_base + i) & buffer_mask];
    uint32_t diff = rabs(pred, c1); // 0..128
    uint32_t current = diff << 8;               // 8.8 fixed-point, 0..32768
    size_t ema_idx = ema_start + i;
    uint32_t r = ema_buf[ema_idx];
    r = ((r * ema_one_minus_alpha) + (current * ema_alpha)) >> 6; // divide by 64 to renormalize
    ema_buf[ema_idx] = (uint16_t)(r);

    // Note: iterating longest-to-shortest means on equal scores, longer periods win
    //       this way we'll start at the beginning of a sequence, not at the end
    SimilarityEma_top2(r, ema_idx, match_index, match_score);
  }
}

// Scalar EMA update over a contiguous block (no ring-buffer wrap): the same
// as update_with_wrapping() with a mask that keeps every index.
void SimilarityEmaFunctions_Scalar_update(
  uint16_t* ema_buf,                  // [in/out] 8.8 fixed-point EMA buffer, length >= count
  const uint8_t* history_buffer_ptr,  // contiguous portion of the ring buffer with history data, length = count
  const size_t count,                 // number of distances
  const uint8_t c1,                   // incoming byte to be used for calculating the distances (basis of the update)
  const uint32_t ema_alpha,           // EMA alpha (fixed-point, denominator 64)
  uint32_t* match_index,              // [out] top-2 ema_buf indices (best, second-best)
  uint32_t* match_score)              // [out] top-2 ema_buf scores  (best, second-best)
{
  SimilarityEmaFunctions_Scalar_update_with_wrapping(ema_buf, history_buffer_ptr, 0, SIZE_MAX, count, 0, c1, ema_alpha, match_index, match_score);
}
