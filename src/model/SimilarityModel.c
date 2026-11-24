#include "SimilarityModel.h"
#include "../Array.h"
#include "../Hash.h"

SimilarityModel* SimilarityModel_new(Shared* const sh, const uint64_t size, const size_t max_match_distance, const size_t max_record_length) {
  SimilarityModel* const sim = ARRAY_NEW(SimilarityModel, 1); // zeroed
  sim->shared = sh;
  /* ResidualMap: numContexts, histogramsPerContext, scale=64 */
  ResidualMap_init(&sim->mapR1, sh, SimilarityModel_nRM1, 32, 64);
  ResidualMap_init(&sim->mapR2, sh, SimilarityModel_nRM2, 64 * 16, 64);
  /* ContextMap2: size, numContexts, scale */
  ContextMap2_init(&sim->cm, sh, size, SimilarityModel_nCM, 64);
  sim->record_len = 1;
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

// Horizontal extrapolations from the row that starts dist bytes back (N, NW,
// NWW, NWWWW are the bytes at dist, dist+1, dist+2, dist+4):
//   W + N - NW    with the context: how well NW predicted W
//   WW + N - NWW  with the context: how well NWW predicted WW
// Both go to mapR1; the predictions are returned in e[0], e[1].
static void SimilarityModel_extrapolate(SimilarityModel* const sim, const uint32_t dist, short* const e) {
  const Shared* const shared = sim->shared;
  INJECT_SHARED_buf
  const uint8_t W = RingBuffer_back(buf, 1);
  const uint8_t WW = RingBuffer_back(buf, 2);
  const uint8_t WWWW = RingBuffer_back(buf, 4);
  const uint8_t N = RingBuffer_back(buf, dist);
  const uint8_t NW = RingBuffer_back(buf, dist + 1);
  const uint8_t NWW = RingBuffer_back(buf, dist + 2);
  const uint8_t NWWWW = RingBuffer_back(buf, dist + 4);
  ResidualMap_set(&sim->mapR1, e[0] = W + N - NW, min(rabs(W, WW + NW - NWW), 31));
  ResidualMap_set(&sim->mapR1, e[1] = WW + N - NWW, min(rabs(WW, WWWW + NWW - NWWWW), 31));
}

// Vertical extrapolations of order 2, 3 and 4 from the rows N, NN, NNN, ...:
// the coefficients of the rows (the k-th differences)
static const int8_t verticalCoefficients[3][4] = {
  { 2, -1,  0,  0 }, // o2: N + (N - NN)
  { 3, -3,  1,  0 }, // o3: N + (N - NN) + ((N - NN) - (NN - NNN))
  { 4, -6,  4, -1 }, // o4
};

void SimilarityModel_mix(SimilarityModel* const sim, Mixer* const m) {
  const Shared* const shared = sim->shared;
  INJECT_SHARED_bpos
  if (bpos == 0) {
    INJECT_SHARED_buf
    // cm contexts (the values hashed with their index, after the first one)
    uint32_t cmContexts[SimilarityModel_nCM - 1];

    // match model: predicted byte according to the best/second-best match period
    // period = MAX_MATCH_DISTANCE - idx, buf distance = period
    short extrapolations[4];
    for (int k = 0; k < 2; k++) {
      const uint32_t dist = sim->MAX_MATCH_DISTANCE - sim->match_index[k];
      const uint8_t expected_byte = RingBuffer_back(buf, dist);
      cmContexts[k] = expected_byte; //strong
      ResidualMap_set(&sim->mapR1, expected_byte, min(sim->match_score[k] >> 8, 31));
    }
    const uint8_t expected_byte0 = cmContexts[0];
    uint32_t r = min(rabs(expected_byte0, cmContexts[1]) >> 1, 63); // 0..63
    ResidualMap_set(&sim->mapR2, expected_byte0, r << 4 | min(sim->match_score[0] >> 9, 15));
    for (int k = 0; k < 2; k++) {
      SimilarityModel_extrapolate(sim, sim->MAX_MATCH_DISTANCE - sim->match_index[k], &extrapolations[k * 2]);
    }
    const uint8_t W = RingBuffer_back(buf, 1);

    // record model: the rows above (row[1] = N, row[2] = NN, ...) and the
    // column before them (col[1] = NW, col[2] = NNW, ...)
    const uint32_t record_len = sim->record_len;
    uint8_t row[6], col[5];
    uint16_t score[5];
    int best = 1;
    for (int k = 1; k <= 5; k++) {
      row[k] = RingBuffer_back(buf, k * record_len);
      if (k <= 4) {
        col[k] = RingBuffer_back(buf, k * record_len + 1);
        score[k] = sim->ema_buf[sim->MAX_MATCH_DISTANCE - record_len * k];
        if (score[k] < score[best]) { // predict based on the row with the lowest score
          best = k;
        }
      }
    }
    const uint8_t N = row[1];
    const uint8_t best_N = row[best];
    const uint32_t best_score = score[best] >> 8;

    // measure of consistency of the record-predictions
    uint32_t column_score = 0;
    uint32_t lowest = 255, highest = 0, sum = 2;
    for (int k = 1; k <= 4; k++) {
      column_score += rabs(best_N, row[k]);
      lowest = min(lowest, row[k]);
      highest = max(highest, row[k]);
      sum += row[k];
    }
    // prediction based on the lowest score among the rows
    ResidualMap_set(&sim->mapR2, best_N, min(best_score >> 1, 31) << 5 | min(column_score >> 2, 31)); // weak
    short bestExtrapolations[2];
    SimilarityModel_extrapolate(sim, best * record_len, bestExtrapolations);

    // predict based on individual row score
    for (int k = 1; k <= 4; k++) {
      ResidualMap_set(&sim->mapR1, row[k], min((score[k] >> 8) >> 1, 31));
    }

    // vertical extrapolations (o2, o3, o4)
    short o2_N = 0;
    for (int o = 0; o < 3; o++) {
      const int8_t* const c = verticalCoefficients[o];
      int pred = 0, errN = 0, errW = 0;
      for (int k = 0; k < 4; k++) {
        pred += c[k] * row[k + 1];
        errN += c[k] * row[k + 2];
        errW += c[k] * col[k + 1];
      }
      if (o == 0) {
        o2_N = (short)pred;
        ResidualMap_set(&sim->mapR1, (short)pred, min(rabs(W, errW), 31));
      }
      else {
        ResidualMap_set(&sim->mapR1, (short)pred, min(rabs(N, errN) + rabs(W, errW), 63) >> 1);
      }
    }

    // average
    ResidualMap_set(&sim->mapR1, sum >> 2, min((highest - lowest) >> 1, 31)); // weak

    // cm contexts
    ContextMap2_set(&sim->cm, hash4(row[1], row[2], row[3], row[4]));
    // highly adaptive contexts: expected_byte0 (strong), expected_byte1 (strong),
    // horizontal extrapolations 0 (strong), 1 (mid), 2 (mid), o2_N (mid, rarely useful), best_N (mid)
    cmContexts[2] = extrapolations[0] + 256;
    cmContexts[3] = extrapolations[1] + 256;
    cmContexts[4] = extrapolations[2] + 256;
    cmContexts[5] = o2_N + 256;
    cmContexts[6] = best_N;
    for (int k = 0; k < SimilarityModel_nCM - 1; k++) {
      ContextMap2_set(&sim->cm, hash2(k + 1, cmContexts[k]));
    }

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
