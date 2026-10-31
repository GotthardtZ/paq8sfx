#pragma once

#include "../Shared.hpp"
#include "../ContextMap2.hpp"
#include "../HashElementForMatchPositions.hpp"

#include "MatchInfo.hpp"

/**
 * Predict the next bit based on a preceding long matching byte sequence
 *
 * This model monitors byte sequences and keeps their most recent positions in a hashtable.
 * When the current byte sequence matches an older sequence (having the same hash) the model predicts the forthcoming bits.
 */
class MatchModel {
private:
  static constexpr int nCM = 2;
  static constexpr int nST = 3;
  Shared * const shared;
  Array<HashElementForMatchPositions> hashtable;
  StateMap stateMaps[nST];
  ContextMap2 cm;
  uint32_t ctx[nST] {0};
  const int hashBits;
  Ilog *ilog = &Ilog::getInstance();

  static constexpr int MINLEN_RM = 3; //minimum length in recovery mode before we "fully recover"
  static constexpr int LEN1 = 5;  //note: this length is modelled by NormalModel
  static constexpr int LEN2 = 7;  //note: this length is *not* modelled by NormalModel
  static constexpr int LEN3 = 9;  //note: this length is *not* modelled by NormalModel

  static constexpr size_t N = 4; // maximum number of match candidates
  Array<MatchInfo> matchCandidates{ N };
  uint32_t numberOfActiveCandidates = 0;

  bool isMatch(const uint32_t pos, const uint32_t MINLEN) const;
  void AddCandidates(HashElementForMatchPositions* matches, uint32_t LEN);

public:
  static constexpr int MIXERINPUTS =
    2 + // direct mixer inputs based on expectedBit
    nCM * (ContextMap2::MIXERINPUTS) +
    nST * 2; //22
  static constexpr int MIXERCONTEXTS = 20;
  static constexpr int MIXERCONTEXTSETS = 1;
  MatchModel(Shared* const sh, const uint64_t hashtablesize, const uint64_t mapmemorysize);
  void update();
  void mix(Mixer &m);
};
