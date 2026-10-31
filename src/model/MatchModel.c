#include "MatchModel.h"
#include "../Array.h"
#include "../Ilog.h"

#define MINLEN_RM 3 //minimum length in recovery mode before we "fully recover"
#define LEN1 5  //note: this length is modelled by NormalModel
#define LEN2 7  //note: this length is *not* modelled by NormalModel
#define LEN3 9  //note: this length is *not* modelled by NormalModel

#define nST MatchModel_nST
#define N MatchModel_N

void MatchModel_init(MatchModel* const mm, Shared* const sh, const uint64_t hashtablesize, const uint64_t mapmemorysize) {
  mm->shared = sh;
  mm->hashtable = ARRAY_NEW(HashElementForMatchPositions, hashtablesize);
  StateMap_init(&mm->stateMaps[0], sh, 1, 28 * 2 * 8,    1023, STATEMAP_GENERIC);
  StateMap_init(&mm->stateMaps[1], sh, 1, 256 * 8 * 256, 1023, STATEMAP_GENERIC);
  StateMap_init(&mm->stateMaps[2], sh, 1, 256 * 256,     1023, STATEMAP_GENERIC);
  ContextMap2_init(&mm->cm, sh, mapmemorysize, MatchModel_nCM, 64);
  memset(mm->ctx, 0, sizeof(mm->ctx));
  mm->hashBits = ilog2((uint32_t)hashtablesize);
  memset(mm->matchCandidates, 0, sizeof(mm->matchCandidates));
  mm->numberOfActiveCandidates = 0;
  assert(isPowerOf2(hashtablesize));
  assert(isPowerOf2(mapmemorysize));
}

void MatchModel_destroy(MatchModel* const mm) {
  Array_delete(mm->hashtable);
  mm->hashtable = NULL;
  for (int i = 0; i < nST; i++) {
    StateMap_destroy(&mm->stateMaps[i]);
  }
  ContextMap2_destroy(&mm->cm);
}

static bool isMatch(const MatchModel* const mm, const uint32_t pos, const uint32_t MINLEN) {
  const Shared* const shared = mm->shared;
  INJECT_SHARED_buf
    for (uint32_t length = 1; length <= MINLEN; length++) {
      if (RingBuffer_back(buf, length) != RingBuffer_at(buf, pos - length))
        return false;
    }
  return true;
}

static void AddCandidates(MatchModel* const mm, HashElementForMatchPositions* matches, uint32_t LEN) {
  uint32_t i = 0;
    while (mm->numberOfActiveCandidates < N && i < HashElementForMatchPositions_N) {
      uint32_t matchpos = matches->matchPositions[i];
      if (matchpos == 0)
        break;
      if (isMatch(mm, matchpos, LEN)) {
        bool isSame = false;
        //is this position already registered?
        for (uint32_t j = 0; j < mm->numberOfActiveCandidates; j++) {
          MatchInfo* oldcandidate = &mm->matchCandidates[j];
          isSame = oldcandidate->index == matchpos;
          if (isSame)
            break;
        }
        if (!isSame) { //don't register an already registered sequence
          MatchInfo_registerMatch(&mm->matchCandidates[mm->numberOfActiveCandidates], matchpos, LEN, LEN1);
          mm->numberOfActiveCandidates++;
        }
      }
      i++;
    }
}

void MatchModel_update(MatchModel* const mm) {
  const Shared* const shared = mm->shared;

  //update active candidates, remove dead candidates
  size_t n = max(mm->numberOfActiveCandidates, 1);
  for (size_t i = 0; i < n; i++) {
    MatchInfo* matchInfo = &mm->matchCandidates[i];
    MatchInfo_update(matchInfo, shared, MINLEN_RM);
    if (mm->numberOfActiveCandidates != 0 && MatchInfo_isInNoMatchMode(matchInfo)) {
      mm->numberOfActiveCandidates--;
      if (mm->numberOfActiveCandidates == i)
        break;
      memmove(&mm->matchCandidates[i], &mm->matchCandidates[i + 1], (mm->numberOfActiveCandidates - i) * sizeof(MatchInfo));
      i--;
    }
  }

  INJECT_SHARED_bpos
  if( bpos == 0 ) {
    
    INJECT_SHARED_pos
    uint64_t hash;
    HashElementForMatchPositions* matches;

    hash = shared->State.NormalModel.cxt[LEN3];
    matches = &mm->hashtable[finalize64(hash, mm->hashBits)];
    if (mm->numberOfActiveCandidates < N)
      AddCandidates(mm, matches, LEN3); //longest
    HashElementForMatchPositions_add(matches, pos);

    hash = shared->State.NormalModel.cxt[LEN2];
    matches = &mm->hashtable[finalize64(hash, mm->hashBits)];
    if (mm->numberOfActiveCandidates < N)
      AddCandidates(mm, matches, LEN2); //middle
    HashElementForMatchPositions_add(matches, pos);

    hash = shared->State.NormalModel.cxt[LEN1];
    matches = &mm->hashtable[finalize64(hash, mm->hashBits)];
    if (mm->numberOfActiveCandidates < N)
      AddCandidates(mm, matches, LEN1); //shortest
    HashElementForMatchPositions_add(matches, pos);

    INJECT_SHARED_buf
    for (size_t i = 0; i < mm->numberOfActiveCandidates; i++) {
      mm->matchCandidates[i].expectedByte = RingBuffer_at(buf, mm->matchCandidates[i].index);
    }
  }
}

void MatchModel_mix(MatchModel* const mm, Mixer* const m) {
  MatchModel_update(mm);
  const Shared* const shared = mm->shared;

  for( uint32_t i = 0; i < nST; i++ ) { // reset contexts
    mm->ctx[i] = 0;
  }
  
  size_t bestCandidateIdx = 0; //default item is the first candidate, let's see if any other candidate is better
  for (size_t i = 1; i < mm->numberOfActiveCandidates; i++) {
    if (MatchInfo_isBetterThan(&mm->matchCandidates[i], &mm->matchCandidates[bestCandidateIdx]))
      bestCandidateIdx = i;
  }

  const MatchInfo* const best = &mm->matchCandidates[bestCandidateIdx];
  const uint32_t length = best->length;
  const uint8_t expectedByte = best->expectedByte;
  const bool isInNoMatchMode = MatchInfo_isInNoMatchMode(best);
  const bool isInDeltaMode = best->delta;
  const bool isInPreRecoveryMode = MatchInfo_isInPreRecoveryMode(best);
  const bool isInRecoveryMode = MatchInfo_isInRecoveryMode(best);

  INJECT_SHARED_bpos
  INJECT_SHARED_c0
  INJECT_SHARED_c1
  const int expectedBit = length != 0 ? (expectedByte >> (7 - bpos)) & 1 : 0;

  uint32_t n0 = 0;
  uint32_t n1 = 0;
  for (size_t i = 0; i < mm->numberOfActiveCandidates; i++) {
    if (mm->matchCandidates[i].length == 0)
      continue;
    int expectedBit = (mm->matchCandidates[i].expectedByte >> (7 - bpos)) & 1;
    n0 += 1 - expectedBit;
    n1 += expectedBit;
  }
  
  const int isUncertain = (int)(n0 != 0 && n1 != 0);

  uint32_t denselength = 0; // 0..27
  if (length != 0) {
    if (length <= 16) {
      denselength = length - 1; // 0..15
    } else {
      denselength = 12 + (min(length - 1, 63) >> 2); // 16..27
    }
    mm->ctx[0] = 1 + ((denselength << 4) | (expectedBit << 3) | bpos); // 1 .. 28*2*8+1
    mm->ctx[1] = 1 + ((expectedByte << 11) | (bpos << 8) | c1);        // 1 .. 256*8*256+1
    const int sign = 2 * expectedBit - 1;
    Mixer_add(m, sign * (min(length, 32) << 5)); // +/- 32..1024
    Mixer_add(m, sign * (Ilog_log(min(length, 65535)) << 2)); // +/-  0..1024
  } else { // no match at all or delta mode
    Mixer_add(m, 0);
    Mixer_add(m, 0);
  }

  if( isInDeltaMode ) { // delta mode: helps predicting the remaining bits of a character when a mismatch occurs
    mm->ctx[2] = (expectedByte << 8) | c0;
  }

  for( uint32_t i = 0; i < nST; i++ ) {
    const uint32_t c = mm->ctx[i];
    if( c != 0 ) {
      const int p1 = StateMap_p1(&mm->stateMaps[i], c - 1);
      const int st = stretch(p1);
      Mixer_add(m, st >> 2);
      Mixer_add(m, (p1 - 2048) >> 3);
    } else {
      Mixer_add(m, 0);
      Mixer_add(m, 0);
    }
  }

  const uint32_t length2 = min(length >> 2, 3); //0-3 (2 bits)

  const uint8_t mode3 = 
    isInNoMatchMode ? 0 :
    isInDeltaMode ? 1 :
    isInPreRecoveryMode ? 2 :
    isInRecoveryMode ? 3 :
    4 + length2; //0..7 //we don't need expectedBit and isUncertain as this context is used on the Byte-level
  
  const uint32_t mode5 =
    isInNoMatchMode ? 0 :
    isInDeltaMode ? 1 :
    isInPreRecoveryMode ? 2 :
    isInRecoveryMode ? 3 :
    4 + (length2 << 2 | expectedBit << 1 | isUncertain); //0..19

  //bytewise contexts
  INJECT_SHARED_c4
  if( bpos == 0 ) {
    ContextMap2_set(&mm->cm, hash1((length != 0 ? expectedByte : c1) << 3 | mode3)); //max context bits: 8+8+3 = 19
    ContextMap2_set(&mm->cm, hash2(length != 0 ? expectedByte : ((c4 >> 8) & 0xff), (c1 << 3) | mode3)); //max context bits: 8+8+8+3=27
  }
  ContextMap2_mix(&mm->cm, m);

  Mixer_set(m, mode5, 20);

  mm->shared->State.Match.length2 =
    isInDeltaMode ? 1 :
    length == 0 ? 0 :
    length <= 7 ? 2 : 3;
  mm->shared->State.Match.mode3 = mode3; 
  mm->shared->State.Match.mode5 = mode5;
  mm->shared->State.Match.expectedByte = length != 0 ? expectedByte : 256;
}
