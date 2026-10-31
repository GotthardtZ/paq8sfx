#include "SimilarityModel.h"
#include "../Array.h"
#include "../Hash.h"

SimilarityModel* SimilarityModel_new(Shared* const sh, const uint64_t size, const size_t max_match_distance, const size_t max_record_length) {
  SimilarityModel* const sim = (SimilarityModel*)calloc(1, sizeof(SimilarityModel));
  if (sim == NULL) {
    quit("Out of memory.");
  }
  sim->shared = sh;
  /* ResidualMap: numContexts, histogramsPerContext, scale=64 */
  ResidualMap_init(&sim->mapR1, sh, SimilarityModel_nRM1, 32, 64);
  ResidualMap_init(&sim->mapR2, sh, SimilarityModel_nRM2, 64 * 16, 64);
  /* ContextMap2: size, numContexts, scale */
  ContextMap2_init(&sim->cm, sh, size, SimilarityModel_nCM, 64);
  sim->record_len = 1;
  sim->record_score = 0;
  sim->mctx1 = 0;
  sim->mctx2 = 0;
  sim->MAX_MATCH_DISTANCE = max_match_distance;
  sim->MAX_RECORD_LENGTH = max_record_length;
  sim->ema_buf = (uint16_t*)Array_new(sim->MAX_MATCH_DISTANCE, sizeof(uint16_t), 32);

  assert(sim->MAX_MATCH_DISTANCE % 16 == 0);   // AVX2 loop stride
  assert(sim->MAX_MATCH_DISTANCE >= sim->MAX_RECORD_LENGTH * 4);
  assert(RingBuffer_size(&sh->buf) > sim->MAX_MATCH_DISTANCE);

  for (size_t i = 0; i < sim->MAX_MATCH_DISTANCE; i++) {
    sim->ema_buf[i] = 64 << 8; // 64 = expected rabs value for random x1,x2 values (in fixed point)
  }
  return sim;
}

void SimilarityModel_delete(SimilarityModel* const sim) {
  if (sim == NULL) {
    return;
  }
  ResidualMap_destroy(&sim->mapR1);
  ResidualMap_destroy(&sim->mapR2);
  ContextMap2_destroy(&sim->cm);
  Array_delete(sim->ema_buf);
  free(sim);
}

void SimilarityModel_update(SimilarityModel* const sim, const uint64_t warmup) {

  // Find best record period
  const size_t count = min(warmup, sim->MAX_RECORD_LENGTH);
  sim->record_score = UINT32_MAX;

  // if two periods have identical ema values, the one with the shorter period wins
  //   why: - shorter periods are usually the 'correct' ones when one detected period length is a multiple of the other
  //        - shorter periods are closer to the current byte position which is usually a somewhat stronger context
  for (size_t i = 0; i < count; i++) {
    const size_t rec_len = count - i; // candidate period length, decreasing as i increases
    uint32_t r = (
      sim->ema_buf[sim->MAX_MATCH_DISTANCE - rec_len * 1] +
      sim->ema_buf[sim->MAX_MATCH_DISTANCE - rec_len * 2] +
      sim->ema_buf[sim->MAX_MATCH_DISTANCE - rec_len * 3] +
      sim->ema_buf[sim->MAX_MATCH_DISTANCE - rec_len * 4]
      );
    if (r <= sim->record_score) {
      sim->record_len = rec_len;
      sim->record_score = r;
    }
  }
}

void SimilarityModel_mix(SimilarityModel* const sim, Mixer* const m) {
  const Shared* const shared = sim->shared;
  INJECT_SHARED_bpos
  if (bpos == 0) {
    // predicted byte according to the best/second-best match period
    // period = MAX_MATCH_DISTANCE - idx, buf distance = period
    INJECT_SHARED_buf
    uint8_t expected_byte0 = RingBuffer_back(buf, sim->MAX_MATCH_DISTANCE - sim->match_index[0]); // the expected byte based on the best match
    uint8_t expected_byte1 = RingBuffer_back(buf, sim->MAX_MATCH_DISTANCE - sim->match_index[1]); // the expected byte based on the second best match

    // match model

    uint32_t histogram_id0 = min(sim->match_score[0] >> 8, 31);
    uint32_t histogram_id1 = min(sim->match_score[1] >> 8, 31);
    ResidualMap_set(&sim->mapR1, expected_byte0, histogram_id0);
    ResidualMap_set(&sim->mapR1, expected_byte1, histogram_id1);

    uint32_t r = min(rabs(expected_byte0, expected_byte1) >> 1, 63); // 0..63
    uint32_t histogram_id3 = r << 4 | min(sim->match_score[0] >> 9, 15);
    ResidualMap_set(&sim->mapR2, expected_byte0, histogram_id3);

    const uint8_t W = RingBuffer_back(buf, 1);
    const uint8_t WW = RingBuffer_back(buf, 2);
    const uint8_t WWWW = RingBuffer_back(buf, 4);
    short horizontal_extrapolation0;
    short horizontal_extrapolation1;
    short horizontal_extrapolation2;
    short horizontal_extrapolation3;
    {
      uint32_t dist = sim->MAX_MATCH_DISTANCE - sim->match_index[0]; // period = distance
      const uint8_t N0 = RingBuffer_back(buf, dist);
      const uint8_t NW0 = RingBuffer_back(buf, dist + 1);
      const uint8_t NWW0 = RingBuffer_back(buf, dist + 2);
      const uint8_t NWWWW0 = RingBuffer_back(buf, dist + 4);
      //W+N-NW (horizontal extrapolation)
      ResidualMap_set(&sim->mapR1, horizontal_extrapolation0 = W + N0 - NW0, min(rabs(W, WW + NW0 - NWW0), 31));
      //WW+NW-NWW (horizontal extrapolation)
      ResidualMap_set(&sim->mapR1, horizontal_extrapolation1 = WW + N0 - NWW0, min(rabs(WW, WWWW + NWW0 - NWWWW0), 31));
    }

    {
      uint32_t dist = sim->MAX_MATCH_DISTANCE - sim->match_index[1]; // period = distance
      const uint8_t N0 = RingBuffer_back(buf, dist);
      const uint8_t NW0 = RingBuffer_back(buf, dist + 1);
      const uint8_t NWW0 = RingBuffer_back(buf, dist + 2);
      const uint8_t NWWWW0 = RingBuffer_back(buf, dist + 4);
      //W+N-NW (horizontal extrapolation)
      ResidualMap_set(&sim->mapR1, horizontal_extrapolation2 = W + N0 - NW0, min(rabs(W, WW + NW0 - NWW0), 31));
      //WW+NW-NWW (horizontal extrapolation)
      ResidualMap_set(&sim->mapR1, horizontal_extrapolation3 = WW + N0 - NWW0, min(rabs(WW, WWWW + NWW0 - NWWWW0), 31));
    }

    // record model

    const uint8_t N = RingBuffer_back(buf, 1 * sim->record_len);
    const uint8_t NN = RingBuffer_back(buf, 2 * sim->record_len);
    const uint8_t NNN = RingBuffer_back(buf, 3 * sim->record_len);
    const uint8_t NNNN = RingBuffer_back(buf, 4 * sim->record_len);

    // predict based on the row with the lowest score
    uint8_t best_N = N;
    uint8_t best_NW = RingBuffer_back(buf, sim->record_len + 1);
    uint8_t best_NWW = RingBuffer_back(buf, sim->record_len + 2);
    uint8_t best_NWWWW = RingBuffer_back(buf, sim->record_len + 4);
    uint16_t score_N = sim->ema_buf[sim->MAX_MATCH_DISTANCE - sim->record_len * 1];
    uint16_t score_NN = sim->ema_buf[sim->MAX_MATCH_DISTANCE - sim->record_len * 2];
    uint16_t score_NNN = sim->ema_buf[sim->MAX_MATCH_DISTANCE - sim->record_len * 3];
    uint16_t score_NNNN = sim->ema_buf[sim->MAX_MATCH_DISTANCE - sim->record_len * 4];
    uint16_t best_score = score_N;
    if (score_NN < best_score) {
      best_N = NN;
      best_NW = RingBuffer_back(buf, sim->record_len * 2 + 1);
      best_NWW = RingBuffer_back(buf, sim->record_len * 2 + 2);
      best_NWWWW = RingBuffer_back(buf, sim->record_len * 2 + 4);
      best_score = score_NN;
    }
    if (score_NNN < best_score) {
      best_N = NNN;
      best_NW = RingBuffer_back(buf, sim->record_len * 3 + 1);
      best_NWW = RingBuffer_back(buf, sim->record_len * 3 + 2);
      best_NWWWW = RingBuffer_back(buf, sim->record_len * 3 + 4);
      best_score = score_NNN;
    }
    if (score_NNNN < best_score) {
      best_N = NNNN;
      best_NW = RingBuffer_back(buf, sim->record_len * 4 + 1);
      best_NWW = RingBuffer_back(buf, sim->record_len * 4 + 2);
      best_NWWWW = RingBuffer_back(buf, sim->record_len * 4 + 4);
      best_score = score_NNNN;
    }
    best_score >>= 8;
    score_N >>= 8;
    score_NN >>= 8;
    score_NNN >>= 8;
    score_NNNN >>= 8;

    // measure of consistency of the record-predictions
    uint32_t column_score = (rabs(best_N, N) + rabs(best_N, NN) + rabs(best_N, NNN) + rabs(best_N, NNNN));
    // prediction based on the lowest score among the rows
    ResidualMap_set(&sim->mapR2, best_N, min(best_score >> 1, 31) << 5 | min(column_score >> 2, 31)); // weak

    // W+N-NW (horizontal extrapolation)
    ResidualMap_set(&sim->mapR1, W + best_N - best_NW, min(rabs(W, WW + best_NW - best_NWW), 31));
    // WW+NW-NWW (horizontal extrapolation)
    ResidualMap_set(&sim->mapR1, WW + best_N - best_NWW, min(rabs(WW, WWWW + best_NWW - best_NWWWW), 31));

    // predict based on individual row score
    ResidualMap_set(&sim->mapR1, N, min(score_N >> 1, 31)); // +1 to map only 0 to 0
    ResidualMap_set(&sim->mapR1, NN, min(score_NN >> 1, 31));
    ResidualMap_set(&sim->mapR1, NNN, min(score_NNN >> 1, 31));
    ResidualMap_set(&sim->mapR1, NNNN, min(score_NNNN >> 1, 31));

    // vertical extrapolations (o2, o3, o4)

    const uint8_t NW = RingBuffer_back(buf, sim->record_len + 1);
    const uint8_t NWW = RingBuffer_back(buf, sim->record_len + 2);
    const uint8_t NNW = RingBuffer_back(buf, 2 * sim->record_len + 1);
    short o2_N = 2 * N - NN;
    int o2_N_err_with_W = 2 * NW - NNW;
    ResidualMap_set(&sim->mapR1, o2_N, min(rabs(W, o2_N_err_with_W), 31));

    const uint8_t NNNW = RingBuffer_back(buf, 3 * sim->record_len + 1);
    const uint8_t NNNNW = RingBuffer_back(buf, 4 * sim->record_len + 1);
    const uint8_t NNNNN = RingBuffer_back(buf, 5 * sim->record_len);

    short o3_N = 3 * N - 3 * NN + NNN;  // N + (N - NN) + ((N - NN) - (NN - NNN))
    int o3_N_err_with_N = 3 * NN - 3 * NNN + NNNN;
    int o3_N_err_with_W = 3 * NW - 3 * NNW + NNNW;
    ResidualMap_set(&sim->mapR1, o3_N, min(rabs(N, o3_N_err_with_N) + rabs(W, o3_N_err_with_W), 63) >> 1);

    short o4_N = 4 * N - 6 * NN + 4 * NNN - NNNN;
    int o4_N_err_with_N = 4 * NN - 6 * NNN + 4 * NNNN - NNNNN;
    int o4_N_err_with_W = 4 * NW - 6 * NNW + 4 * NNNW - NNNNW;
    ResidualMap_set(&sim->mapR1, o4_N, min(rabs(N, o4_N_err_with_N) + rabs(W, o4_N_err_with_W), 63) >> 1);

    // average

    uint32_t lowest = min(min(min(N, NN), NNN), NNNN);
    uint32_t highest = max(max(max(N, NN), NNN), NNNN);
    uint32_t span = highest - lowest;
    ResidualMap_set(&sim->mapR1, (N + NN + NNN + NNNN + 2) >> 2, min(span >> 1, 31)); // weak

    // cm contexts

    uint64_t i = 0;
    ContextMap2_set(&sim->cm, hash4(N, NN, NNN, NNNN));
    i++;

    // highly adaptive contexts
    ContextMap2_set(&sim->cm, hash2(i++, expected_byte0)); //strong
    ContextMap2_set(&sim->cm, hash2(i++, expected_byte1)); //strong
    ContextMap2_set(&sim->cm, hash2(i++, horizontal_extrapolation0 + 256)); //strong
    ContextMap2_set(&sim->cm, hash2(i++, horizontal_extrapolation1 + 256)); //mid
    ContextMap2_set(&sim->cm, hash2(i++, horizontal_extrapolation2 + 256)); //mid
    ContextMap2_set(&sim->cm, hash2(i++, o2_N + 256)); //mid (rarely useful)
    ContextMap2_set(&sim->cm, hash2(i++, best_N)); //mid

    assert(i == SimilarityModel_nCM);

    // mixer contexts

    uint32_t column_ctx = column_score <= 1 ? 0 : column_score < (sim->record_score >> 10) * 3 ? 1 : 2; // 0..2
    uint32_t match_ctx = sim->match_score[0] == 0 ? 0 : 1 + min((sim->match_score[0] >> 8) >> 2, 30); // 0..31

    sim->mctx1 =
      column_ctx << 5 |
      match_ctx;

    sim->mctx2 =
      column_ctx << 1 |
      (sim->record_score == 0);
  }

  // for every bit

  ResidualMap_mix(&sim->mapR1, m);
  ResidualMap_mix(&sim->mapR2, m);
  ContextMap2_mix(&sim->cm, m);

  Mixer_set(m, sim->mctx1, 3 * 32);
  Mixer_set(m, sim->mctx2, 3 * 2);
}
