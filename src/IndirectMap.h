#pragma once

#include "IPredictor.h"
#include "Hash.h"
#include "Mixer.h"
#include "Random.h"
#include "Shared.h"
#include "StateMap.h"

enum { IndirectMap_MIXERINPUTS = 2 };

typedef struct IndirectMap {
  IPredictor predictor;
  const Shared* shared;
  Random rnd;
  uint8_t* data;
  uint32_t mask;
  uint32_t maskBits;
  uint32_t stride;
  uint32_t bTotal;
  uint32_t b;
  uint32_t bCount;
  uint32_t context;
  uint8_t* cp;
  int scale;
  StateMap sm;
} IndirectMap;

void IndirectMap_init(IndirectMap* im, const Shared* sh, int bitsOfContext, int inputBits, int scale, int limit);
void IndirectMap_set(IndirectMap* im, uint64_t ctx);
void IndirectMap_setScale(IndirectMap* im, int Scale);
void IndirectMap_mix(IndirectMap* im, Mixer* m);
