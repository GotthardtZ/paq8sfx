#include "SimilarityModelPair.h"

// Maximum match/record search distance (in bytes) by compression level (1-12):
// 768, 1024, 1536, 2048, 3072, 4096, 6144, 8192, 12288, 16384, 24576, 32768
// (3 or 4 times a power of 2). The values must be multiples of 16 (AVX2 stride).
static uint32_t maxMatchDistance(const int level) {
  return (uint32_t)((level & 1) ? 3 : 4) << (8 + (level - 1) / 2);
}

void SimilarityModelPair_init(SimilarityModelPair* const pair, Shared* const sh, const uint64_t mem) {
  pair->shared = sh;

  pair->MAX_MATCH_DISTANCE = maxMatchDistance(sh->level); // common for slow and fast models

  pair->EMA_ALPHA_SLOW = 1; // alpha = 1/64 (1-alpha = 63/64)
  pair->EMA_ALPHA_FAST = 7; // alpha = 7/64 (1-alpha = 57/64)

  // Record length is capped relative to MAX_MATCH_DISTANCE so that
  // the 4x-period lookback used in update() always stays within the buffer.
  const size_t max_record_length_slow = min(1024, (int)(pair->MAX_MATCH_DISTANCE / 4));
  // Fast model uses a shorter record window
  const size_t max_record_length_fast = min(512, (int)(max_record_length_slow / 2));

  pair->similarityModel_slow = SimilarityModel_new(sh, mem / 16, pair->MAX_MATCH_DISTANCE, max_record_length_slow);
  pair->similarityModel_fast = SimilarityModel_new(sh, mem / 32, pair->MAX_MATCH_DISTANCE, max_record_length_fast);

  // Runtime SIMD dispatch: AVX2 > SSE4.1 > scalar
  pair->emaUpdateFunction = SimilarityEmaFunctionsFactory_getEmaUpdateFunction(sh);
}

void SimilarityModelPair_update(SimilarityModelPair* const pair) {
  const Shared* const shared = pair->shared;
  const size_t MAX_MATCH_DISTANCE = pair->MAX_MATCH_DISTANCE;

  // from slow model
  uint16_t* const ema_buf1 = &pair->similarityModel_slow->ema_buf[0];
  uint32_t* const match_index1 = pair->similarityModel_slow->match_index;
  uint32_t* const match_score1 = pair->similarityModel_slow->match_score;

  // from fast model
  uint16_t* const ema_buf2 = &pair->similarityModel_fast->ema_buf[0];
  uint32_t* const match_index2 = pair->similarityModel_fast->match_index;
  uint32_t* const match_score2 = pair->similarityModel_fast->match_score;

  uint8_t* history_buffer_ptr;
  size_t buffer_mask;
  size_t head_pos;
  RingBuffer_leakInternals(&shared->buf, &history_buffer_ptr, &buffer_mask, &head_pos);
  head_pos--; // point to last known byte (c1)
  INJECT_SHARED_c1
  const size_t byte_count = min(pair->warmup, MAX_MATCH_DISTANCE); // number of valid history bytes available
  pair->warmup++; // total bytes processed; capped to MAX_MATCH_DISTANCE

  // EMA update loop
  const size_t buf_base = head_pos - byte_count;           // logical start of the history window
  const size_t ema_base = MAX_MATCH_DISTANCE - byte_count; // corresponding start offset in ema_buf[] during warmup
  
  // EMA update: two paths depending on warmup state and ring-buffer wrap
  //   1. scalar: initial warmup (byte_count < MAX_MATCH_DISTANCE) OR very rare ring-buffer wrap
  //   2. vectorized: steady-state with no ring-buffer wrap
  const size_t buf_start_physical = buf_base & buffer_mask; // physical start index in ring buffer
  if (byte_count == MAX_MATCH_DISTANCE && (buf_start_physical + MAX_MATCH_DISTANCE) <= (buffer_mask + 1)) {
    // Path 2: steady-state, no wrap - contiguous block, vectorized path
    const uint8_t* const history = history_buffer_ptr + buf_start_physical;
    pair->emaUpdateFunction(ema_buf1, history, MAX_MATCH_DISTANCE, c1, pair->EMA_ALPHA_SLOW, match_index1, match_score1);
    pair->emaUpdateFunction(ema_buf2, history, MAX_MATCH_DISTANCE, c1, pair->EMA_ALPHA_FAST, match_index2, match_score2);
  }
  else {
    // Path 1: warmup or rare ring-buffer wrap - scalar with masking
    SimilarityEmaFunctions_Scalar_update_with_wrapping(
      ema_buf1,
      history_buffer_ptr,
      buf_base,
      buffer_mask,
      byte_count,
      ema_base,
      c1,
      pair->EMA_ALPHA_SLOW,
      match_index1,
      match_score1);
    SimilarityEmaFunctions_Scalar_update_with_wrapping(
      ema_buf2,
      history_buffer_ptr,
      buf_base,
      buffer_mask,
      byte_count,
      ema_base,
      c1,
      pair->EMA_ALPHA_FAST,
      match_index2,
      match_score2);
  }

}

void SimilarityModelPair_mix(SimilarityModelPair* const pair, Mixer* const m) {
  const Shared* const shared = pair->shared;
  INJECT_SHARED_bpos
  if (bpos == 0) {
    SimilarityModelPair_update(pair);
    SimilarityModel_update(pair->similarityModel_slow, pair->warmup);
    SimilarityModel_update(pair->similarityModel_fast, pair->warmup);
  }
  SimilarityModel_mix(pair->similarityModel_slow, m);
  SimilarityModel_mix(pair->similarityModel_fast, m);
}
