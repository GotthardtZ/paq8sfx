#pragma once

#include "../ContextMap2.h"
#include "../Shared.h"

/**
 * Modeling bit contexts with various masks.
 */
enum {
  SparseBitModel_nCM = 6 + 4,
  SparseBitModel_MIXERINPUTS = SparseBitModel_nCM * (ContextMap2_MIXERINPUTS), // 40
  SparseBitModel_MIXERCONTEXTS = 0,
  SparseBitModel_MIXERCONTEXTSETS = 0
};

typedef struct SparseBitModel {
  const Shared* shared;
  ContextMap2 cm;
} SparseBitModel;

void SparseBitModel_init(SparseBitModel* sbm, const Shared* sh, uint64_t size);
void SparseBitModel_mix(SparseBitModel* sbm, Mixer* m);
