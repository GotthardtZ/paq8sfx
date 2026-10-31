#pragma once

#include "SystemDefines.h"

/**
 * 32-bit pseudo random number generator
 */
typedef struct Random {
  uint64_t _state;
} Random;

void Random_init(Random* rnd);
uint32_t Random_next(Random* rnd, int numberOfBits);
