#include "SimilarityEmaFunctions_SSE41.h"
#include "SimilarityEmaFunctions_Scalar.h" // SimilarityEma_top2()

#if defined(X64_SIMD_AVAILABLE) && !defined(AVX2_ONLY)

#include <smmintrin.h> // SSE4.1

// Process 8 uint16 EMA values per iteration using SSE4.1.
//
// Fixed-point layout: 8.8 (8 integer bits, 8 fractional bits)
//   current      = diff << 8             → max 128 << 8 = 32768       (fits in uint16)
//   blend        = (r * one_minus_alpha + current * alpha) >> 6
//   intermediate = r * one_minus_alpha   → max 65535 * 64 = 4,194,240 (needs 32-bit lanes)
//   result       = blend >> 6            → max 65535                  (fits back in uint16)
//
// Strategy: widen 8 x uint16 → two groups of 4 x uint32, compute blend in 32-bit lanes,
// pack results back to uint16 with _mm_packus_epi32 (requires SSE4.1).

TARGET("sse4.1")
void SimilarityEmaFunctions_SSE41_update(
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
  const __m128i vc1_bytes = _mm_set1_epi8((int8_t)c1);

  const __m128i va = _mm_set1_epi32(ema_alpha);
  const __m128i vb = _mm_set1_epi32(ema_one_minus_alpha);

  // SSE has no unsigned 16-bit compare; simulate u16 < by XORing both sides with 0x8000
  // to shift the unsigned range into the signed range before _mm_cmplt_epi16.
  const __m128i v_sign_flip = _mm_set1_epi16((int16_t)0x8000);

  // Current second-best thresholds for fast reject.
  __m128i vbest2 = _mm_set1_epi16((int16_t)match_score[1]);

  assert(count % 8 == 0);
  for (size_t i = 0; i < count; i += 8) {
    // --- Load predictors and compute rabs ---
    // sub_epi8 wraps into int8 range naturally; abs_epi8 gives absolute value (0..128).
    // _mm_abs_epi8 on 0x80 (-128) yields 128 unsigned - correct for our 0..128 range.
    __m128i pred_bytes = _mm_loadl_epi64((const __m128i*)(history_buffer_ptr + i));
    __m128i abs_bytes = _mm_abs_epi8(_mm_sub_epi8(pred_bytes, vc1_bytes));

    __m128i abs_lo = _mm_cvtepu8_epi32(abs_bytes);
    __m128i abs_hi = _mm_cvtepu8_epi32(_mm_srli_si128(abs_bytes, 4));

    __m128i cur_lo = _mm_slli_epi32(abs_lo, 8); // 8.8 fixed-point, 0..32768
    __m128i cur_hi = _mm_slli_epi32(abs_hi, 8);

    // --- Load + Update EMA ---
    __m128i ema16 = _mm_load_si128((const __m128i*)(ema_buf + i));
    __m128i ema_lo = _mm_cvtepu16_epi32(ema16);
    __m128i ema_hi = _mm_cvtepu16_epi32(_mm_srli_si128(ema16, 8));

    __m128i tmp_lo = _mm_add_epi32(_mm_mullo_epi32(ema_lo, vb),
      _mm_mullo_epi32(cur_lo, va));
    __m128i tmp_hi = _mm_add_epi32(_mm_mullo_epi32(ema_hi, vb),
      _mm_mullo_epi32(cur_hi, va));

    __m128i res_lo = _mm_srli_epi32(tmp_lo, 6);
    __m128i res_hi = _mm_srli_epi32(tmp_hi, 6);
    __m128i new_ema = _mm_packus_epi32(res_lo, res_hi);

    // --- Store updated EMA values ---
    _mm_store_si128((__m128i*)(ema_buf + i), new_ema);

    // --- Fast reject: skip scalar top-2 update if no lane beat the current second-best ---
    __m128i cmp = _mm_cmpgt_epi16(_mm_sub_epi16(vbest2, v_sign_flip),
      _mm_sub_epi16(new_ema, v_sign_flip));

    if (_mm_testz_si128(cmp, cmp))
      continue;

    // Fast reject: we reach here only if at least one lane beat the second-best
    // which is roughly 30/8192 of the times

    // --- Scalar top-2 update ---
    for (int k = 0; k < 8; k++) {
      uint32_t idx = (uint32_t)(i + k);
      uint32_t r = ema_buf[idx];
      // Iterates longest-to-shortest; on equal scores, shorter periods win ties.
      SimilarityEma_top2(r, idx, match_index, match_score);
    }

    // Refresh thresholds
    vbest2 = _mm_set1_epi16((int16_t)match_score[1]);
  }
}

#endif
