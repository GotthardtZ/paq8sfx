#include "StateTable.h"
#include "Rle.h"
#include "Utils.h"

// The tables are expanded by StateTable_init(); their readable form is
// tools/tables/StateTables.c.
#include "StateTableData.inc"

static uint8_t stateTable[256][4]; /**< next state after a 0 bit, after a 1 bit; n0, n1 */
static uint8_t stateGroup[256];
static uint8_t statePrio[256]; // this table needs tuning

void StateTable_init(void) {
  Rle_decode(stateGroup, stateGroupRle, STATE_TABLE_HEAD, 5);
  Rle_decode(statePrio, statePrioRle, STATE_TABLE_HEAD, 5);
  for (int i = 0; i < 253; i++) {
    uint8_t* const t = stateTable[i];
    if (i < STATE_TABLE_HEAD) {
      t[0] = stateNextHead[i * 2];
      t[1] = stateNextHead[i * 2 + 1];
      t[2] = stateCountsHead[i] & 15;
      t[3] = stateCountsHead[i] >> 4;
    }
    else {
      // contexts representing strong trend of 0s or 1s: groups of 4 states
      // (149-152, 153-156, ...; from 205 with incremental counting):
      // j=0: n0 0s, j=1: n0 0s and a 1, j=2: a 0 and n1 1s, j=3: n1 1s
      const int k = (i - STATE_TABLE_HEAD) >> 2;
      const int j = (i - STATE_TABLE_HEAD) & 3;
      const int advance = min(i + 4, 249 + j); // the next state of the group, up to 249-252
      const int other = (0x6450455F >> (j * 8)) & 0xff; // the state after the other bit: 95, 69, 80, 100
      const int n = (j == 1 || j == 2 ? 15 : 16) + k;
      t[j >> 1] = (uint8_t)advance;
      t[(j >> 1) ^ 1] = (uint8_t)other;
      t[2 + (j >> 1)] = (uint8_t)n;
      t[3 - (j >> 1)] = (uint8_t)(j == 1 || j == 2);
      static const uint8_t groupLimits[4] = { 15, 5, 5, 14 }; // the group number increases by 1 at this k
      static const uint8_t groups[4] = { 19, 22, 30, 16 };
      stateGroup[i] = groups[j] + (j == 1 ? -(k >= groupLimits[j]) : (k >= groupLimits[j]));
      statePrio[i] = (uint8_t)((j == 1 || j == 2 ? 28 : 31) + 2 * k);
    }
  }
  // 253-255 are unused (and in case we reach such a state we'll need to restart)
  for (int i = 253; i < 256; i++) {
    stateTable[i][0] = 1;
    stateTable[i][1] = 2;
  }
}

uint8_t StateTable_next(const uint8_t state, const int y) {
  return stateTable[state][y];
}

uint8_t StateTable_nextRandom(const uint8_t oldState, const int y, Random* const rnd) {
  const uint8_t newState = stateTable[oldState][y];
  if (newState >= 205 && newState >= oldState + 4) {
    // Apply probabilistic increment for contexts with strong trend of 0s or 1s.
    // Applicable to states 149 and above, but we'll start applying it from states (205-208).
    // That means...
    // For states 205..208, 209..212, ... 249..252 a group of 4 states is not represented by
    // the counts indicated in the state table. An exponential scale is used instead.
    // The highest group (249..252) represents the top of this scale, where we can not increment anymore.
    // Reaching the highest states (249-252) from states (201-204) takes 12 steps.
    // thus the probability of reaching the highest states (249-252) is ~ 1/2048 
    if (Random_next(rnd, 1) != 0) { // random bit: p()=1/2 -> the probability to advance to a higher state is 1/2
      return oldState; // don't advance
    }
  }
  return newState;
}

void StateTable_update(uint8_t* const state, const int y, Random* const rnd) {
  *state = StateTable_nextRandom(*state, y, rnd);
}

uint8_t StateTable_group(const uint8_t state) {
  return stateGroup[state];
}

uint8_t StateTable_prio(const uint8_t state) {
  return statePrio[state];
}
