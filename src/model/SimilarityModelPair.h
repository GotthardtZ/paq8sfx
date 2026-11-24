#pragma once

#include "SimilarityEmaFunctionsFactory.h"
#include "SimilarityModel.h"

/**
 * Owns a pair of SimilarityModel instances (slow and fast)
 * and dispatches EMA buffer updates through the best available SIMD path
 * (AVX2, SSE4.1, or scalar) selected at construction time.
 *
 * The slow model (alpha=1) tracks stable long-range structure;
 * the fast model (alpha=7) adapts quickly to changes.
 * Both share the same MAX_MATCH_DISTANCE, derived from the compression level.
 */

typedef struct SimilarityModelPair {
  Shared* shared;
  SimilarityModel* similarityModel_slow; // alpha=1: slow adaptation, stable predictions
  SimilarityModel* similarityModel_fast; // alpha=7: fast adaptation, responsive to change
  SimilarityEmaUpdateFunction emaUpdateFunction;         // best available SIMD path, selected at construction

  size_t MAX_MATCH_DISTANCE;                             // shared search window size in bytes for both models (typical: 8192)

  // EMA smoothing factors: weight on new sample: alpha/64, weight on history: (64-alpha)/64
  uint32_t EMA_ALPHA_SLOW;                               // for the slow model (typical: 1), fixed-point with denominator of 64
  uint32_t EMA_ALPHA_FAST;                               // for the fast model (typical: 5-9), fixed-point with denominator of 64
  size_t warmup;                                         // bytes processed so far; caps EMA loop range
} SimilarityModelPair;

void SimilarityModelPair_init(SimilarityModelPair* pair, Shared* sh, uint64_t mem);
void SimilarityModelPair_update(SimilarityModelPair* pair);
void SimilarityModelPair_mix(SimilarityModelPair* pair, Mixer* m);
