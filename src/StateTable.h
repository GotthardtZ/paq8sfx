#pragma once

#include "Random.h"

/**
 * State table:
 *   nex(state, 0) = next state if bit y is 0, 0 <= state < 256
 *   nex(state, 1) = next state if bit y is 1
 *   nex(state, 2) = number of zeros in bit history represented by state
 *   nex(state, 3) = number of ones represented
 *
 * States represent a bit history within some context.
 * State 0 is the starting state (no bits seen).
 * States 1-30 represent all possible sequences of 1-4 bits.
 * States 31-252 represent a pair of counts, (n0,n1), the number
 *   of 0 and 1 bits respectively. If n0+n1 < 16 then there are
 *   two states for each pair, depending on if a 0 or 1 was the last bit seen.
 * If n0 and n1 are too large, then there is no state to represent this
 * pair, so another state with about the same ratio of n0/n1 is substituted.
 * Also, when a bit is observed and the count of the opposite bit is large,
 * then part of this count is discarded to favor newer data over old.
 */
uint8_t StateTable_next(uint8_t state, int y);

/**
  * Next state with probabilistic increment
  * @param oldState
  * @param y
  * @param rnd
  * @return
  */
uint8_t StateTable_nextRandom(uint8_t oldState, int y, Random* rnd);

void StateTable_update(uint8_t* state, int y, Random* rnd);

uint8_t StateTable_group(uint8_t state);

uint8_t StateTable_prio(uint8_t state);
