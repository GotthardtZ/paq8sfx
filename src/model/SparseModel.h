#pragma once

#include "../ContextMap2.h"
#include "../Mixer.h"
#include "../Shared.h"

/**
 * Model order 1-2-3 contexts with gaps.
 */
enum {
  SparseModel_nCM = 28 + 3,
  SparseModel_MIXERINPUTS = SparseModel_nCM * (ContextMap2_MIXERINPUTS), // 217
  SparseModel_MIXERCONTEXTS = 4 * 256,
  SparseModel_MIXERCONTEXTSETS = 1
};

typedef struct SparseModel {
  const Shared* shared;
  ContextMap2 cm;
  uint32_t ctx;
} SparseModel;

void SparseModel_init(SparseModel* sm, const Shared* sh, uint64_t size);
void SparseModel_mix(SparseModel* sm, Mixer* m);
