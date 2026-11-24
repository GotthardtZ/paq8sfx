#pragma once

#include "SystemDefines.h"

/**
 * 32-bit pseudo random number generator
 */
typedef struct Random {
  uint64_t _state;
} Random;

uint32_t Random_next(Random* rnd, int numberOfBits);
