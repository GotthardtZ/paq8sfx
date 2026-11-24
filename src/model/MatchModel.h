#pragma once

#include "../Shared.h"
#include "../ContextMap2.h"
#include "../HashElementForMatchPositions.h"

#include "MatchInfo.h"

/**
 * Predict the next bit based on a preceding long matching byte sequence
 *
 * This model monitors byte sequences and keeps their most recent positions in a hashtable.
 * When the current byte sequence matches an older sequence (having the same hash) the model predicts the forthcoming bits.
 */
enum {
  MatchModel_nCM = 2,
  MatchModel_nST = 3,
  MatchModel_MIXERINPUTS =
    2 + // direct mixer inputs based on expectedBit
    MatchModel_nCM * (ContextMap2_MIXERINPUTS) +
    MatchModel_nST * 2, //22
  MatchModel_MIXERCONTEXTS = 20,
  MatchModel_MIXERCONTEXTSETS = 1,
  MatchModel_N = 4 // maximum number of match candidates
};

typedef struct MatchModel {
  Shared* shared;
  HashElementForMatchPositions* hashtable;
  uint32_t ctx[MatchModel_nST];
  int hashBits;

  MatchInfo matchCandidates[MatchModel_N];
  uint32_t numberOfActiveCandidates;
  StateMap stateMaps[MatchModel_nST];
  ContextMap2 cm;
} MatchModel;

void MatchModel_init(MatchModel* mm, Shared* sh, uint64_t hashtablesize, uint64_t mapmemorysize);
void MatchModel_update(MatchModel* mm);
void MatchModel_mix(MatchModel* mm, Mixer* m);
