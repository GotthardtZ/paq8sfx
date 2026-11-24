#include "SimilarityEmaFunctions_AVX2.h"
#include "SimilarityEmaFunctions_Scalar.h" // SimilarityEma_top2()

#ifdef X64_SIMD_AVAILABLE

// Process 16 uint16 EMA values per iteration using AVX2.
//
// Fixed-point layout: 8.8 - see SSE4.1 file for full derivation.
//   current = diff << 8,  blend computed in 32-bit lanes, packed back to uint16.
//
// Strategy: load 16 x uint16 from ema_buf, widen into two groups of 8 x uint32
// (low/high halves of the 256-bit register), compute the EMA blend in 32-bit lanes,
// then pack back to uint16 with _mm256_packus_epi32.
//
// AVX2 lane-crossing note: _mm256_packus_epi32(lo, hi) does not produce sequential
// output - it interleaves results from the two 128-bit lanes. A subsequent
// _mm256_permute4x64_epi64(..., 0xD8) is required to restore sequential order.

TARGET("avx2")
void SimilarityEmaFunctions_AVX2_update(
  uint16_t* ema_buf,                  // [in/out] 8.8 fixed-point EMA buffer, length >= count
  const uint8_t* history_buffer_ptr,  // contiguous portion of the ring buffer with history data, length = count
  const size_t count,                 // number of distances
  const uint8_t c1,                   // incoming byte to be used for calculating the distances (basis of the update)
  const uint32_t ema_alpha,           // EMA alpha (fixed-point, denominator 64)
  uint32_t* match_index,              // [out] top-2 ema_buf indices (best, second-best)
  uint32_t* match_score)              // [out] top-2 ema_buf scores  (best, second-best)
{

  match_index[0] = 0;
  match_index[1] = 0;
  match_score[0] = UINT16_MAX;
  match_score[1] = UINT16_MAX;

  const uint32_t ema_one_minus_alpha = 64 - ema_alpha;

  // rabs is computed as abs_epi8(sub_epi8(pred, c1)), then zero-extended to 32-bit.
  // This mirrors: abs(int8_t(x1 - x2));
  const __m256i vc1_bytes = _mm256_set1_epi8((int8_t)c1);

  const __m256i va = _mm256_set1_epi32(ema_alpha);
  const __m256i vb = _mm256_set1_epi32(ema_one_minus_alpha);

  // AVX2 has no unsigned 16-bit compare; simulate u16 < by XORing both sides with 0x8000
  // to shift the unsigned range into the signed range before _mm256_cmpgt_epi16.
  const __m256i v_sign_flip = _mm256_set1_epi16((int16_t)0x8000);

  // Current second-best thresholds for fast reject.
  __m256i vbest2 = _mm256_set1_epi16((int16_t)match_score[1]);

  assert(count % 16 == 0);
  for (size_t i = 0; i < count; i += 16) {
    // --- Load predictors and compute rabs ---
    // sub_epi8 wraps into int8 range naturally; abs_epi8 gives absolute value (0..128).
    // _mm256_abs_epi8 on 0x80 (-128) yields 128 unsigned - correct for our 0..128 range.
    __m128i pred_bytes = _mm_loadu_si128((const __m128i*)(history_buffer_ptr + i));
    __m256i pred_bytes256 = _mm256_broadcastsi128_si256(pred_bytes);
    __m256i abs256 = _mm256_abs_epi8(_mm256_sub_epi8(pred_bytes256, vc1_bytes));

    __m256i abs_lo = _mm256_cvtepu8_epi32(_mm256_castsi256_si128(abs256));
    __m256i abs_hi = _mm256_cvtepu8_epi32(_mm_srli_si128(_mm256_castsi256_si128(abs256), 8));

    __m256i cur_lo = _mm256_slli_epi32(abs_lo, 8);
    __m256i cur_hi = _mm256_slli_epi32(abs_hi, 8);

    // --- Load + Update EMA ---
    __m256i ema16 = _mm256_load_si256((const __m256i*)(ema_buf + i));
    __m256i ema_lo = _mm256_cvtepu16_epi32(_mm256_castsi256_si128(ema16));
    __m256i ema_hi = _mm256_cvtepu16_epi32(_mm256_extracti128_si256(ema16, 1));

    __m256i tmp_lo = _mm256_add_epi32(_mm256_mullo_epi32(ema_lo, vb), _mm256_mullo_epi32(cur_lo, va));
    __m256i tmp_hi = _mm256_add_epi32(_mm256_mullo_epi32(ema_hi, vb), _mm256_mullo_epi32(cur_hi, va));

    __m256i res_lo = _mm256_srli_epi32(tmp_lo, 6);
    __m256i res_hi = _mm256_srli_epi32(tmp_hi, 6);

    // _mm256_packus_epi32 interleaves 128-bit lanes; permute restores sequential order.
    // 0xD8 (0b11011000) = indices [0,2,1,3] → moves lane1-lo and lane1-hi into contiguous positions.
    __m256i new_ema = _mm256_permute4x64_epi64(_mm256_packus_epi32(res_lo, res_hi), 0xD8);

    // --- Store updated EMA values ---
    _mm256_store_si256((__m256i*)(ema_buf + i), new_ema);

    // --- Fast reject: skip scalar top-2 update if no lane beat the current second-best ---
    __m256i cmp = _mm256_cmpgt_epi16(
      _mm256_sub_epi16(vbest2, v_sign_flip),
      _mm256_sub_epi16(new_ema, v_sign_flip));

    if (_mm256_testz_si256(cmp, cmp))
      continue;

    // Fast reject: we reach here only if at least one lane beat the second-best
    // which is roughly 30/8192 of the times

    // --- Scalar top-2 update ---
    for (int k = 0; k < 16; k++) {
      uint32_t idx = (uint32_t)(i + k);
      uint32_t r = ema_buf[idx];
      // Iterates longest-to-shortest; on equal scores, shorter periods win ties.
      SimilarityEma_top2(r, idx, match_index, match_score);
    }

    // Refresh thresholds
    vbest2 = _mm256_set1_epi16((int16_t)match_score[1]);
  }
}
#endif
