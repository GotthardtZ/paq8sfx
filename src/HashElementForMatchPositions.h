#pragma once

#include "SystemDefines.h"

#define HashElementForMatchPositions_N 3

typedef struct HashElementForMatchPositions {
  uint32_t matchPositions[HashElementForMatchPositions_N];
} HashElementForMatchPositions;

_Static_assert(sizeof(HashElementForMatchPositions) == 3 * 4, "HashElementForMatchPositions must be 12 bytes");

static inline void HashElementForMatchPositions_add(HashElementForMatchPositions* const e, const uint32_t pos) {
  if (HashElementForMatchPositions_N > 1) {
    memmove(&e->matchPositions[1], &e->matchPositions[0], (HashElementForMatchPositions_N - 1) * sizeof(e->matchPositions[0]));
  }
  e->matchPositions[0] = pos;
}
