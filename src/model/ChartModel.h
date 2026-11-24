#pragma once

#include "../ContextMap2.h"
#include "../Shared.h"

enum {
  ChartModel_nCM = (3 * 4) + 3 + (8 + 8 + 8) * 3, //87
  ChartModel_MIXERINPUTS = ChartModel_nCM * (ContextMap2_MIXERINPUTS), // 348
  ChartModel_MIXERCONTEXTS = 0,
  ChartModel_MIXERCONTEXTSETS = 0
};

typedef struct ChartModel {
  const Shared* shared;
  uint32_t chart[3*8];
  uint32_t charGroup;
  ContextMap2 cm;
  uint8_t indirect1[1024+64];
  uint8_t indirect2[256];
  uint8_t indirect3[256 * 256];
} ChartModel;

void ChartModel_init(ChartModel* cmodel, const Shared* sh, uint64_t size);
void ChartModel_mix(ChartModel* cmodel, Mixer* m);
