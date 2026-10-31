#pragma once

#include "../ResidualMap.h"
#include "../ContextMap2.h"

/**
 * Combined similarity-based match model and record model.
 * Matches and record lengths are detected based on byte similarity (EMA of |x1-x2|) rather than equality.
 *
 * Match model:
 *   - best for content with undetected audio, image, or other 1D numeric streams.
 *
 * Record model:
 *   - for content with shorter-period record structures for which byte equality based detection struggles to find the record length
 *     such as undetected 8/24/32-bit raw image data or other 2D numeric streams.
 *
 * Maintains two EMA buffers (slow + fast alpha) via SimilarityModelPair.
 * Best and second-best match periods and record periods are tracked by index into ema_buf[].
 */

enum {
  SimilarityModel_nRM1 = 16,
  SimilarityModel_nRM2 = 2,
  SimilarityModel_nCM = 8,
  SimilarityModel_MIXERINPUTS =
    (SimilarityModel_nRM1 + SimilarityModel_nRM2) * ResidualMap_MIXERINPUTS +
    SimilarityModel_nCM * (ContextMap2_MIXERINPUTS), // 92
  SimilarityModel_MIXERCONTEXTS = (3 * 32) + (3 * 2), // 102
  SimilarityModel_MIXERCONTEXTSETS = 2
};

typedef struct SimilarityModel {
  Shared* shared;
  ResidualMap mapR1;
  ResidualMap mapR2;
  ContextMap2 cm;
 
  uint32_t record_len;    // best candidate period length (in bytes)
  uint32_t record_score;  // sum of EMA scores at 1x..4x record_len (lower = better)
  uint16_t mctx1;         // mixer context
  uint16_t mctx2;         // mixer context

  // exposed public members for SimilarityModelPair

  size_t MAX_MATCH_DISTANCE; // typically 8192
  size_t MAX_RECORD_LENGTH;  // typically 1024 or 512

  uint16_t* ema_buf;              // 8.8 fixed-point EMA of |x-pred| for each distance/period; size = MAX_MATCH_DISTANCE; 32-byte aligned
  uint32_t match_index[2];        // ema_buf[] indices of the best and second-best match period
  uint32_t match_score[2];        // EMA scores (lower = better) for match_index[0] and [1]
} SimilarityModel;

SimilarityModel* SimilarityModel_new(Shared* sh, uint64_t size, size_t max_match_distance, size_t max_record_length);
void SimilarityModel_delete(SimilarityModel* sim);

void SimilarityModel_update(SimilarityModel* sim, uint64_t warmup);
void SimilarityModel_mix(SimilarityModel* sim, Mixer* m);
